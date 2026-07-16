# YAFS — Yet Another File System (also YAku File System)

> **Implementation Status**: This document describes the full YAFS design. The current implementation (`sys/src/fs/yafs/`) covers Phase 1 — basic B+Tree (insert/lookup/delete/walk), superblock, free-space tracking, block I/O, and FHS directory tree creation. Encryption, snapshots, rollback, parallel reads, and the `yafsctl` tool have not been implemented.

YAFS is a **log-structured, copy-on-write (CoW)** file system designed for the M4KK1
kernel. Its design is inspired by Btrfs and ZFS but aims for an implementation
simple enough to build from scratch in a kernel-development environment.

---

## 1. Core Mechanisms

### 1.1 Disk Layout

The disk is divided into fixed-size **blocks** (default 4096 bytes). The layout is
determined by the superblock and dynamically allocated B+Trees — there are no
fixed-size block groups or pre-allocated inode tables.

```
 Block 0          : Primary Superblock
 Block 1          : Backup Superblock
 Block 2          : Free-Space B+Tree Root
 Block 3          : (reserved)
 Block 4          : Metadata B+Tree Root (files, directories, extents)
 Block 5          : Snapshot B+Tree Root
 Block 6 ... N-1  : Free blocks (allocated on demand)
```

**Superblock (struct yafs_superblock)** — 4096 bytes, written at block 0.
- magic, version, block_size, total_blocks, blocks_used
- root_tree_addr, snapshot_tree_addr, free_tree_addr
- encryption_flags, kmk_salt, kmk_hash, enc_fek_blk
- sb_backup_addr (mirror copy location)
- creation_time, mount_time, mount_count, state_flags
- checksum (CRC-32C covers everything before it)

A backup copy is written at block 1 whenever the superblock is updated.  On
mount, if the primary passes checksum it is used; otherwise the backup is tried.

**B+Tree Node Header (struct yafs_node_header_t)** — every B+Tree block starts
with a 32-byte header (defined in `yafs_btree.h`):
- magic (`0x59414B55` = "YAKU")
- checksum (CRC-32C of the payload after the header)
- flags (bit 0 = leaf, bit 1 = deleted)
- level (0 = leaf, >0 = internal, root has the highest level)
- entry_count (number of key–value pairs stored)
- parent_lba (block address of parent node, 0 = root; best-effort only)

Data blocks (inodes, snapshots) do not use this header; they are raw
`yafs_inode_value` / `yafs_snapshot_value` blocks referenced by the B+Tree.

**Free-Space Tracking** — a B+Tree maps `(start_block → length)` ranges of
free blocks.  When a block is freed its address is inserted; when one is
allocated a range is split or removed.  This replaces traditional bitmaps and
is itself CoW.

**B+Tree metadata tree** (root at `sb->root_tree_addr`) — a single B+Tree
that stores everything via key-space tags (upper 4 bits of the 64-bit key):

| Key space (tag) | Key format | Value (8 bytes) |
|-----------------|------------|-----------------|
| KS_INODE (0)    | inode_number | LBA of `yafs_inode_value` block |
| KS_DIR_ENTRY (1)| (parent_inode << 16) \| name_hash | child inode number |
| KS_EXTENT (2)   | (inode_nr << 16) \| block_index | packed (48-bit LBA, 16-bit length) |
| KS_SNAPSHOT (3) | timestamp | LBA of `yafs_snapshot_value` block |

Keys are 64-bit integers.  The upper 4 bits tag the key space; the lower
60 bits carry the actual key data.  Values are also 64-bit integers — either
a direct child inode / packed extent, or a block address pointing to a larger
struct.

**Snapshot B+Tree** (root at `sb->snapshot_tree_addr`): key = timestamp,
value = LBA of a `yafs_snapshot_value` block.

---

### 1.2 Copy-on-Write (CoW)

Every write operation — whether modifying a leaf data block, a B+Tree internal
node, or the superblock itself — **allocates a new block** instead of
overwriting in place.

#### B+Tree Path-Copying (CoW) Update

Every B+Tree mutation uses **path-copying**: instead of modifying a node in
place, the entire path from root to leaf is **cloned**, modified, and written
to new blocks.  The old root remains intact, providing a built-in snapshot.

```
btree_insert(root_lba, key, value):
  1. Read the root node at root_lba.
  2. Recursively descend (root → leaf):
     a. At each level, read the child node.
     b. Clone the child into a new buffer.
     c. Apply the modification:
        - Leaf: insert/update the key-value pair.
        - Internal: update the child pointer returned from the
          recursive call; if the child was split, also insert
          the promoted key + new child pointer.
     d. Write the cloned node to a new block.
     e. Return the new block's LBA to the parent.
  3. If the leaf overflows, split it into two:
     a. Keep the first half in the current leaf.
     b. Allocate a new leaf for the second half.
     c. Promote the first key of the right leaf to the parent.
  4. If the root was split, create a new root internal node
     with two children (left and right halves).
  5. The caller atomically updates sb->root_tree_addr to the
     new root LBA (see Double Superblock, §4.1).
```

Key property: if power fails before step 5, the old root LBA in the
superblock is still valid and references the tree before the mutation.
There is never an intermediate corrupted state — CoW guarantees
**crash consistency at every write**.

Splitting and promotion are the only structural changes.  Deletion does
**not** merge underfilled nodes — empty nodes are reclaimed by a background
GC thread.

---

### 1.3 Snapshot Mechanism

A snapshot is a **point-in-time copy of the root tree pointer**.

**Creation** (O(1) time, O(1) space):

```
yafs_create_snapshot(m, "before_upgrade"):
  1. Read current sb->root_tree_addr.
  2. Build a struct yafs_snapshot_value with:
       root_addr = current root_tree_addr
       timestamp = now()
       flags = YAFS_SNAP_READONLY
       name = "before_upgrade"
  3. Insert into snapshot B+Tree (key = timestamp).
  4. Write superblock (may be deferred to batch tx commit).
```

No blocks are copied.  Unmodified data blocks are transparently shared.
Space overhead: 128 bytes per snapshot.

**Listing snapshots:** walk the snapshot B+Tree in key order (chronological).

---

### 1.4 Rollback Mechanisms

#### Full Rollback — O(1), replaces the entire tree root

```
yafs_full_rollback(m, "before_upgrade"):
  1. Find snapshot S by name in snapshot B+Tree.
  2. current_tree = sb->root_tree_addr
  3. sb->root_tree_addr = S.root_addr
  4. Decrement refcount recursively from current_tree.
  5. Increment refcount recursively from S.root_addr.
  6. Write superblock.
```

All modifications made after the snapshot are discarded.  The old tree becomes
freeable if no other snapshot references it.

#### Fast/Incremental Rollback — file-level, O(diff)

Only reverts files or directories that were **created, modified, or deleted**
after the snapshot.  Unchanged parts remain intact.

```
yafs_fast_rollback(m, "before_upgrade"):
  1. Find snapshot S.
  2. Do a simultaneous walk of the current tree (T_cur) and the
     snapshot tree (T_snap), comparing inode and dirent keys:
     a. If a key exists in T_cur but not in T_snap:
        → delete it (file/dir created after snapshot → remove).
     b. If a key exists in T_snap but not in T_cur:
        → insert it (file/dir deleted after snapshot → restore).
     c. If a key exists in both but values differ:
        → replace T_cur's value with T_snap's value (e.g. file size,
           data extents, directory entries).
  3. For each extent key that changed under an inode:
     - Replace the extent pointer in T_cur with the snapshot's pointer.
     - CoW semantics: increment refcount on snapshot blocks, decrement
       on old blocks.
  4. Update sb->root_tree_addr to the new root (which now diverges from
     the snapshot's root only for untouched parts).
```

Complexity: O(K) where K is the number of keys that differ between the current
tree and the snapshot tree.  For a typical system with few recent changes, this
is much faster than a full rollback.

---

### 1.5 AES-128 Encryption (XTS Mode)

**XTS-AES-128** encrypts each block independently using two 128-bit keys:

- key1 (data encryption): AES-128 encrypt of plaintext XOR tweak
- key2 (tweak encryption): AES-128 encrypt of the logical LBA

The tweak is the **logical block address** (logical_lba in the block header).
Because XTS is a block cipher mode, each 16-byte AES block within a 4096-byte
sector is encrypted with a tweak derived from the LBA and the block index.

**Key hierarchy:**

```
Password ──PBKDF2──→ KMK (256-bit)
                        │
                        ├── KMK_hash = SHA-256(KMK) — stored in superblock
                        │
                        └── AES-128-W rap ──→ encrypted FEK (stored at
                             of FEK               block enc_fek_blk)
```

- **FEK** (256 bits = two 128-bit XTS keys) is generated randomly at `yafs_format`.
- **KMK** (Key Management Key) is derived from the user password via PBKDF2
  with 100 000 iterations and a random salt stored in `sb->kmk_salt`.
- During mount, the password is hashed, KMK is derived, and KMK_hash is
  compared against `sb->kmk_hash`.  On match, the FEK is decrypted with KMK.

**Interaction with CoW and snapshots:**

- Block encryption uses `logical_lba` as the XTS tweak.
- When CoW allocates a new block, the new block gets a new `logical_lba`
  (which may equal its physical address for simplicity, or an independent
  logical counter).  The data is re-encrypted with the new tweak.
- **Snapshots do not require re-encryption.**  Snapshot blocks are already
  encrypted on disk; they keep their original tweak and ciphertext.
- **Rollback does not require decryption/re-encryption.**  The rollback
  operation simply changes tree pointers; the encrypted blocks are moved as-is.

---

### 1.6 Chunked Parallel Reads

Large files are divided into fixed-size **chunks** (default 1 MB, configurable
via `yafsctl set --parallel`).

**Read path for a large file:**

```
yafs_read_file_parallel(m, inode, buf, offset, size, &read):
  1. Look up the inode's extent list in the B+Tree.
  2. Split the requested [offset, offset+size) range into chunk-aligned
     sub-requests.  Each sub-request covers at most chunk_size bytes and
     falls within one or more extents.
  3. Dispatch sub-requests to a bounded workqueue (size = parallel_workers):
     for each chunk:
       spawn_worker(chunk_index, disk_block, length, output_offset)
  4. Each worker:
     a. Read blocks from device (yafs_read_block → dev_read).
     b. If encrypted, decrypt in-place using FEK + logical_lba.
     c. memcpy(result_buffer + output_offset, decrypted_data, length).
     d. Decrement pending counter.
  5. Wait until pending == 0 (spin or completion callback).
  6. Return total bytes read.
```

**Ordering guarantee:** Each worker writes to a disjoint region of the output
buffer (determined by output_offset).  No ordering conflicts; results are
inherently ordered by their position in the buffer.

**Contention:** Workers share only the `pending` counter (atomic decrement).
The B+Tree lookup and extent parsing happen sequentially before workers are
spawned, so there is no lock contention on metadata.

---

## 2. Key Data Structures

All on-disk structures are defined in `sys/src/fs/yafs/include/yafs.h` with
`__attribute__((aligned(8)))`.  Below is a summary.

### Superblock (`struct yafs_superblock`)

| Offset | Size | Field             | Description                             |
|--------|------|-------------------|-----------------------------------------|
| 0      | 8    | magic             | `0x59414653` = "YAFS"                  |
| 8      | 8    | version           | superblock version (1)                  |
| 16     | 8    | block_size        | bytes per block (default 4096)          |
| 24     | 8    | total_blocks      | device capacity in blocks               |
| 32     | 8    | blocks_used       | currently allocated blocks              |
| 40     | 8    | root_tree_addr    | root of metadata B+Tree                 |
| 48     | 8    | snapshot_tree_addr| root of snapshot B+Tree                 |
| 56     | 8    | free_tree_addr    | root of free-space B+Tree               |
| 64     | 40   | reserved_roots    | future root pointer slots               |
| 104    | 8    | encryption_flags  | `YAFS_ENC_NONE` or `YAFS_ENC_AES128_XTS` |
| 112    | 16   | kmk_salt          | PBKDF2 salt                             |
| 128    | 32   | kmk_hash          | SHA-256(KMK) for password verification  |
| 160    | 8    | enc_fek_blk       | block containing wrapped FEK            |
| 168    | 8    | sb_backup_addr    | backup superblock block number          |
| 176    | 8    | creation_time     |                                        |
| 184    | 8    | mount_time        |                                        |
| 192    | 8    | mount_count       |                                        |
| 200    | 8    | state_flags       | YAFS_STATE_CLEAN / YAFS_STATE_DIRTY     |
| 208    | 4    | checksum          | CRC-32C covering bytes 0..211           |
| 212    | 4    | padding           |                                        |

### B+Tree Node Header (`yafs_node_header_t`, 32 bytes)

| Offset | Size | Field       | Description                             |
|--------|------|-------------|-----------------------------------------
| 0      | 8    | magic       | `0x59414B55` = "YAKU"                  |
| 8      | 4    | checksum    | CRC-32C of payload bytes               |
| 12     | 2    | flags       | bit 0 = leaf, bit 1 = deleted          |
| 14     | 2    | level       | tree level (0 = leaf)                  |
| 16     | 4    | entry_count | number of key–value pairs              |
| 20     | 8    | parent_lba  | parent block LBA (0 = root)            |
| 28     | 4    | _pad        | reserved                               |

### B+Tree Node (`yafs_node_t`, 4096 bytes total)

The header is followed by a union of two payload types, occupying the
remaining 4064 bytes of a 4 KB block:

**Internal node** (level > 0, max 248 keys):

```
[header (32)]
[keys[0..247]   — 248 × 8 bytes = 1984]    sorted ascending
[children[0..248] — 249 × 8 bytes = 1992]   child[0] for keys < key[0],
                                             child[i+1] for keys ≥ key[i]
```

**Leaf node** (level = 0, max 248 entries):

```
[header (32)]
[keys[0..247]     — 248 × 8 bytes = 1984]   sorted ascending
[values[0..247]   — 248 × 8 bytes = 1984]   8-byte entries
[next_leaf (8)]                               sibling link for range scans
[prev_leaf (8)]
```

All values are 64-bit integers (alias `yafs_entry_t`):
- **Dir entry**: child inode number.
- **Extent**: packed as `(48-bit LBA) | (16-bit length << 48)`.  Helper
  macros `yafs_extent_pack`, `yafs_extent_lba`, `yafs_extent_length`
  (defined in `yafs_btree.h`).
- **Inode / snapshot**: LBA of a data block holding the full struct.

### Snapshot Value (`struct yafs_snapshot_value`, 128 bytes)

Stored in a dedicated data block; the B+Tree maps `KS_SNAPSHOT | timestamp`
→ the block's LBA.

| Offset | Size | Field            | Description                              |
|--------|------|------------------|------------------------------------------|
| 0      | 8    | root_addr        | root tree address at snapshot time       |
| 8      | 8    | timestamp        | creation time                            |
| 16     | 8    | parent_snap_addr | timestamp of parent snapshot (0 = first) |
| 24     | 8    | flags            | YAFS_SNAP_READONLY / YAFS_SNAP_ENCRYPTED |
| 32     | 32   | fek              | encrypted FEK (AES-128 XTS keys, 32 B)   |
| 64     | 56   | name             | null-terminated snapshot name (max 55)   |
| 120    | 4    | checksum         | CRC-32C of this struct                   |

### Extent Value (packed, 8 bytes — inline in B+Tree)

Extents are no longer stored as separate structs in a data area.  Each
extent occupies one `yafs_entry_t` (8 bytes) in a B+Tree leaf node:

| Bits  | Field  | Description                            |
|-------|--------|----------------------------------------|
| 0–47  | lba    | physical block address (up to 256 TB)  |
| 48–63 | length | contiguous blocks (up to 65536)        |

The old `struct yafs_extent_value` (24 bytes) is retained in `yafs.h` for
reference but is **not used** by the B+Tree.  Future cache layers may use
it for in-memory extent coalescing.

---

## 3. Command-Line Interface — `yafsctl`

### 3.1 Create Snapshot

```
yafsctl snap <device> <snapshot_name>
```

- **device**: path to the block device (e.g. `/dev/hda1`).
- **snapshot_name**: alphanumeric name, max 63 characters.
- **Behavior**: creates a point-in-time snapshot.  The kernel writes the
  current root tree address into the snapshot B+Tree.  Idempotent — creating
  a snapshot with the same name twice overwrites the earlier one.

**Example:**
```
# yafsctl snap /dev/hda1 before_upgrade
Created snapshot 'before_upgrade' at 2026-07-13 16:00:00
```

### 3.2 Full Rollback

```
yafsctl rollback --type full --snap <snapshot_name> <device>
```

- **--type full**: select full rollback mode.
- **--snap**: name of the snapshot to roll back to.
- **device**: block device path.
- **Behavior**: the current root tree pointer is replaced with the snapshot's
  root pointer.  All modifications after the snapshot are discarded instantly.
  The device must not be mounted (or must be remounted read-only).

**Example:**
```
# yafsctl rollback --type full --snap before_upgrade /dev/hda1
Full rollback to 'before_upgrade' completed.
```

### 3.3 Fast/Incremental Rollback

```
yafsctl rollback --type fast --snap <snapshot_name> <mount_point>
```

- **--type fast**: select fast file-level rollback.
- **--snap**: target snapshot name.
- **mount_point**: the mounted directory path.
- **Behavior**: only files/directories that differ from the snapshot are
  reverted.  Unchanged files remain untouched.  The filesystem stays mounted
  and writable during the operation.

**Example:**
```
# yafsctl rollback --type fast --snap before_upgrade /mnt/yafs
Fast rollback to 'before_upgrade' completed.  15 files reverted, 0 skipped.
```

### 3.4 Set Parallel Read Parameter

```
yafsctl set --parallel <N> <mount_point>
```

- **--parallel N**: number of parallel read workers (1–16).
- **mount_point**: mounted directory.
- **Behavior**: updates the in-memory mount structure's `parallel_workers`
  field.  Subsequent reads use the new concurrency level.

**Example:**
```
# yafsctl set --parallel 8 /mnt/yafs
Parallel read workers set to 8.
```

### 3.5 Encrypted Mount

```
mount -t yafs -o aes128 <device> <mount_point>
Password: ********
```

- **-t yafs**: filesystem type.
- **-o aes128**: enable decryption with AES-128 XTS.
- **Behavior**: the kernel calls `yafs_mount_fs()` which prompts for a
  password (via `yafsctl`'s stdin or through the kernel's console input),
  derives the KMK, verifies `kmk_hash`, decrypts the FEK, and mounts with
  transparent encryption.

**Example:**
```
# mount -t yafs -o aes128 /dev/hda1 /mnt/yafs
Password:
Mounting YAFS with AES-128 XTS encryption...
Mounted successfully.
```

---

## 4. Crash Consistency

Because YAFS uses CoW for all writes, the on-disk state is **never partially
updated** — the old tree root remains valid until the superblock is updated.

### 4.1 Power Loss During a Write

**Scenario:** The kernel is writing new data (allocating blocks, updating
B+Tree nodes) when power is lost.

**Recovery:**
1. On next mount, read the primary superblock at block 0.
2. If its checksum is valid, use it — all prior transactions are intact
   because CoW ensures the old tree referenced only valid, fully-written blocks.
3. If the primary superblock checksum is invalid (partial write), read the
   **backup superblock** at `sb->sb_backup_addr` (block 1).
4. If the backup is valid, use it.  Both copies are updated atomically via
   a full-block write (disk firmware guarantees 512-byte sector atomicity;
   for 4K blocks, the backup provides the safety net).
5. Any blocks that were allocated by the in-flight transaction but not
   referenced by the surviving root are leaked.  A background `yafs_clean()`
   can walk the free-space B+Tree and reclaim orphans by comparing against
   the reachable block set.

### 4.2 Checksum-Based Corruption Detection

Every block stores a checksum in its block header:
- **Metadata blocks** (B+Tree nodes): CRC-32C.
- **Data blocks**: CRC-64 (or CRC-32C for simpler hardware).

On read:
```
yafs_read_block(m, addr, buf):
  1. Issue dev_read(m->device, addr, buf, 1).
  2. Compute checksum of buf[32..block_size-1] (skip header).
  3. Compare against header.checksum.
  4. If mismatch: try reading from an alternate location if available
     (future: DUP profile), or return EIO.
```

For metadata, a checksum failure causes the filesystem to remount read-only
and log the bad block address for `yafsck` repair.

### 4.3 Crash During Snapshot Rollback

Both rollback modes are designed to be **atomic by construction**:

- **Full rollback:** The single superblock write that changes
  `sb->root_tree_addr` is the commit point.  If the system crashes before
  this write, the old tree is intact.  If it crashes after, the new tree
  (snapshot root) is active.  There is no intermediate state.

- **Fast rollback:** The operation modifies the B+Tree via normal CoW updates.
  Each key-value replacement is a separate CoW transaction.  If the system
  crashes mid-rollback, some keys will have been reverted and others not.
  On remount, the tree is consistent (CoW guarantee) but **semantically
  partially rolled back**.  To make fast rollback fully atomic, the
  implementation can:
  1. Record the rollback intent in a small log block.
  2. Perform the CoW updates.
  3. Clear the log block.
  
  If step 2 is interrupted, the log block persists.  On next mount, the
  rollback is replayed or reversed.

---

## 5. VFS Integration

YAFS plugs into the M4KK1 VFS layer (`sys/src/include/vfs.h`) by implementing
the following operations:

### 5.1 `yafs_mount()`

```c
int yafs_mount(struct yafs_mount *m, const char *password);
```

1. Parse mount options (encryption, parallelism, read-only).
2. Read the superblock via `yafs_read_superblock()`.
3. If encryption is requested, call `yafs_setup_encryption(m, password)`.
4. Validate checksums and state flags.
5. Initialize in-memory structures (block cache, workqueue for parallel reads).
6. Populate `m->root_tree_addr`, `m->snapshot_tree_addr`, `m->free_tree_addr`.
7. Return 0 on success.

**VFS mapping:** The mount creates a `mount_entry_t` with fstype="yafs" and
stores the `struct yafs_mount *` in an in-memory mount table.

### 5.2 `yafs_open()` / `yafs_read()` / `yafs_write()` / `yafs_close()`

**yafs_open(pathname, flags):**
1. Walk the path through the directory tree using B+Tree lookups.
2. For each component, call `yafs_lookup(m, dir_inode, name, &child_inode, ...)`.
3. Allocate a `struct yafs_file` and a VFS `file_descriptor_t`.
4. If O_CREAT, call `yafs_create_inode()` + `yafs_create_dirent()`.

**yafs_read(fd, buf, count):**
1. Get `struct yafs_file *file` from the fd table.
2. If file size ≥ chunk_size and parallel_workers > 1:
   `yafs_read_file_parallel(m, file->inode, buf, file->offset, count, &read)`
3. Else (sequential or small file):
   `yafs_read_file_data(m, file->inode, buf, file->offset, count, &read)`
4. Advance file→offset.  Return read bytes.

**yafs_write(fd, buf, count):**
1. CoW-allocate new blocks as needed via `yafs_alloc_block()`.
2. Write extent records into the B+Tree.
3. Update the inode's size and mtime via `yafs_write_inode()`.
4. Advance file→offset.

**yafs_close(fd):**
1. Free the `struct yafs_file` and VFS descriptor.

### 5.3 `yafs_lookup()`

```c
int yafs_lookup(struct yafs_mount *m, uint64_t dir_inode,
                const char *name, uint64_t *child_inode, uint32_t *file_type);
```

1. Compute `name_hash = yafs_name_hash(name)`.
2. Build key: `key = (YAFS_KS_DIR_ENTRY << 60) | (dir_inode << 16) | name_hash`.
3. `yafs_btree_lookup(m->root_tree_addr, key, &entry)`.
4. Extract `child_inode = entry` (the value IS the child inode number);
   read the inode block to get `file_type` if needed.
5. Return 0 on success, -ENOENT on miss.

### 5.4 `yafs_ioctl()`

Handles snapshot and rollback commands from `yafsctl`:

```c
int yafs_ioctl(struct yafs_mount *m, uint32_t cmd, void *arg);
```

| cmd                        | arg                                     | action                        |
|----------------------------|------------------------------------------|-------------------------------|
| YAFS_IOCTL_CREATE_SNAP     | `char name[64]`                          | yafs_create_snapshot(m, name) |
| YAFS_IOCTL_ROLLBACK        | `struct { char name[64]; int type; }`    | full or fast rollback          |
| YAFS_IOCTL_SET_PARALLEL    | `uint32_t n`                             | set parallel_workers           |

### 5.5 Concurrency (Locking Strategy)

- **One mount-level reader-writer lock (`rwlock`)** protecting B+Tree
  traversals: multiple concurrent readers, exclusive on metadata writes.
- **Per-inode mutex** for file data writes (serializes CoW allocation within
  a file).
- **Block cache** protected by a spinlock (short critical sections).
- **Parallel reads** are lock-free after the extent list is extracted; each
  worker writes to disjoint buffer regions.

---

## 6. Performance Analysis and Optimizations

### 6.1 Creation and Deletion of Many Small Files (B+Tree Churn)

**Scenario:** `touch {1..100000}` creates 100 000 0-byte files.

**Behaviour:** Each file creates one inode record and one directory-entry
record in the B+Tree.  200 000 insertions into a single B+Tree causes:

- **Leaf splits:** When a leaf fills (max ~170 keys per 4K block), it
  splits into two.  This propagates up the tree, potentially splitting
  internal nodes.
- **Tree depth:** ~3–4 levels for 100K entries (branching factor ~170 per
  node → ~170^3 ≈ 5M entries).
- **Amortized writes per insertion:** O(log_B N) ≈ 3–4 block writes (CoW).

**Optimizations:**
- **Delayed merging:** When deleting files, don't immediately merge
  underfilled nodes.  A background thread coalesces sparse nodes when
  utilisation drops below a threshold (~30%).
- **BTree leaf grouping:** Batch small-file creations into a single
  transaction so that leaf splits are amortized across multiple insertions.
- **Inline directories:** For directories with ≤ 8 entries, store the
  entries directly in the inode value instead of separate B+Tree records
  (like ext4's inline data).

### 6.2 Sequential Read/Write of Large Files

**Scenario:** Reading a 1 GB file sequentially.

**Single-threaded read:**
- Issue one `dev_read` per block; wait for completion.
- Throughput: limited by device latency × block count.

**Chunked parallel read (configured to 4 workers):**
- Split into four 256 MB chunks.
- Workers issue `dev_read` in parallel.
- On NVMe or multi-queue block devices, throughput scales near-linearly
  with worker count until the device saturates.

**Expected throughput ratio:**

| Workers | Relative Throughput       |
|---------|---------------------------|
| 1       | 1.0× (baseline)          |
| 2       | 1.8–1.9×                 |
| 4       | 3.0–3.5×                 |
| 8       | 4.0–5.0× (diminishing)   |
| 16      | 4.5–5.5× (saturation)    |

### 6.3 First Write After a Snapshot

**Scenario:** Create snapshot, then modify a single file.

**Overhead of CoW on first post-snapshot write:**
1. The file's data block has refcount = 2 (live tree + snapshot).
2. Write triggers a copy: allocate new block, copy old data, modify.
3. Decrement refcount on old block (now refcount = 1 for snapshot).
4. Update the extent record in the B+Tree leaf.
5. CoW propagates up the tree: copy leaf → allocate + modify →
   copy parent → allocate + modify → … up to root.
6. Write superblock.

**Block writes:** (1 data block) + (tree depth ≈ 3–4 metadata blocks) +
  (1 superblock) = 5–6 block writes for a single-block modification.

**Mitigation:** The overhead is proportional to tree depth and is intrinsic
to CoW.  ZFS mitigates this with a **transaction group** (txg) that batches
many mutations before committing; YAFS can adopt a similar batching:

- Accumulate mutations in memory.
- Every N seconds (or after M bytes), commit them as one atomic CoW update.
- This amortizes the root-write overhead across many mutations.

### 6.4 CPU Cache and Lock Contention During Parallel Reads

**Analysis:**

- **Cache:** Each worker reads disjoint blocks and writes to disjoint output
  buffer regions → no false sharing if buffer is cache-line aligned (and
  chunk_size is large).
- **Lock contention:** The B+Tree traversal and extent lookup happen
  **before** workers are spawned.  Workers only call `dev_read` and
  (optionally) `yafs_decrypt_block`, which are stateless.  The only shared
  counter is `pending` (atomic decrement) → negligible contention.

**From the device side:** Multiple concurrent `dev_read` calls may contend
on the block device's internal queue.  If the device supports multiple queues
(NVMe), each worker can be pinned to a separate queue for maximum parallelism.

### 6.5 Encryption / Decryption Impact

**AES-128 XTS** per 4K block:
- 2 AES-128 encryptions per 16-byte block (one for data, one for tweak).
- 4096 / 16 = 256 AES blocks per sector × 2 = 512 AES-128 operations per 4 KB.
- On a modern x86_64 core with **AES-NI**: ~1 cycle/byte → ~4 000 cycles per
  4 KB block → at 2 GHz, ~2 µs per block.
- Without AES-NI (our kernel, which targets i386 without SSE): pure software
  AES is ~20–30 cycles/byte → ~80–120 µs per block.

**Impact on throughput (4K blocks, 4 workers, no AES-NI):**

| Block size | Encrypt time/block | Max throughput (4 workers) |
|------------|-------------------|---------------------------|
| 4 KB       | 100 µs           | 40 MB/s                   |
| 64 KB      | 1 600 µs         | 40 MB/s                   |

**Optimizations:**
- Use **larger block size** (64 KB) to reduce per-byte overhead of the tweak
  computation.
- Implement **AES-NI** support when running on x86_64 (the kernel format
  supports both i386 and x86_64).
- **Pre-compute tweaks** for a chunk's worth of blocks.

---

## 7. Security Model

### 7.1 Unauthorised Access Prevention

- The FEK is never stored in plaintext on disk.  It is wrapped (encrypted)
  with the KMK and stored at block `sb->enc_fek_blk`.
- The KMK is derived from the user password via PBKDF2 with a random salt
  (`sb->kmk_salt`) and 100 000 iterations.
- Without the correct password, an attacker cannot derive the KMK, cannot
  unwrap the FEK, and cannot decrypt any data block.
- The `kmk_hash` in the superblock acts as a **password verifier** but does
  not leak the KMK (it is SHA-256 of the KMK, not the password).
- **Key storage:** keys exist only in kernel memory (struct yafs_mount → fek).
  They are never written to the swap device (or if swap is enabled, swap is
  encrypted with a separate key).

### 7.2 Snapshot Encryption Inheritance

- When a snapshot is taken on an encrypted filesystem, the FEK is **copied**
  into the snapshot value (struct yafs_snapshot_value → fek), encrypted with
  the KMK.
- The snapshot inherits the encryption attributes of the parent filesystem.
- **Separate encryption per snapshot:** A future extension could let the
  administrator specify a different password for a snapshot, which would
  cause the snapshot to store the FEK wrapped with a different KMK.

### 7.3 Rollback Attack Prevention

A "rollback attack" occurs when an attacker with physical access rolls back
the filesystem to an older snapshot to bypass security patches or resurrect
vulnerable code.

**Mitigation strategies:**

1. **Secure Version Counter (SVC):** An anti-rollback counter stored outside
   the filesystem (in UEFI NVRAM, a TPM NVRAM index, or a dedicated secure
   element).  Each mount increments the counter.  Snapshots store the counter
   value at creation time.  Rollback is only allowed to a snapshot whose
   counter value ≥ the current counter.

2. **Snapshot Expiry / WORM:** Snapshots can be marked as append-only or
   have an expiration time.  `yafsctl snap --immutable` marks a snapshot
   as undeletable until a secure counter advances.

3. **Log-based audit trail:** Every rollback event is appended to a small,
   append-only audit log (separate B+Tree) that includes the snapshot name,
   timestamp, and a cryptographic hash chain.  An external auditor can detect
   unauthorised rollbacks.

Without a TPM or secure element, YAFS relies on **filesystem-level policies**
(e.g. snapshots are read-only by default; deletion requires an ioctl that
can be restricted to privileged processes).

### 7.4 Checksums: Tamper Resistance vs Error Correction

**Current:** Checksums detect accidental corruption (bit rot).  They are not
cryptographic MACs — an attacker who can write to the disk can recompute the
CRC-32C after tampering.

**To make checksums tamper-resistant:**

1. **Replace CRC-32C with HMAC-SHA256** in the block header `checksum` field,
   keyed with the FEK or a separate authentication key derived from the KMK.
   This turns checksums into **authenticators** — an attacker without the key
   cannot forge a valid block.

2. **Authenticated encryption:** Use AES-256-GCM instead of AES-128-XTS.
   GCM provides both encryption and integrity (GMAC), so a single operation
   produces ciphertext + authentication tag.  The tag is stored in the block
   header's checksum field.  This eliminates the need for a separate HMAC.

**Trade-offs:**

| Scheme        | Tamper Resistance | Performance  | Implementation Complexity |
|---------------|-------------------|--------------|---------------------------|
| CRC-32C       | None              | Fastest      | Trivial                   |
| CRC-32C + CoW | Weak (CoW itself  | Fast         | Low                       |
|               | prevents undetected overwrite) |    |                           |
| AES-128-XTS   | No (encryption ≠  | Medium       | Medium                    |
| + HMAC-SHA256 | integrity)        |              |                           |
| AES-256-GCM   | Yes               | Slower       | High (needs per-block IV) |

**Recommendation for YAFS 1.0:** Use AES-128-XTS for confidentiality and
CRC-32C + CoW for integrity.  CoW prevents undetected overwrite attacks
because overwriting a block in place would leave the old block (with a
matching checksum) still referenced by the B+Tree — the attacker would also
need to update the B+Tree, which requires passing through CoW (and thus the
kernel).  This makes **unattended tampering detectable** by simple `fsck`.

---

## 8. Implementation Plan

### Phase 1 — Core (in progress)
- [ ] Superblock read/write/verify with backup mirror
- [ ] Free-space B+Tree (alloc/free blocks)
- [ ] Block read/write with checksum verification
- [ ] B+Tree library (lookup, insert, delete, walk)

### Phase 2 — Metadata
- [ ] Inode and dirent operations via B+Tree
- [ ] Path lookup (namei)
- [ ] VFS integration: mount, open, read, write, close, lookup

### Phase 3 — CoW + Snapshots
- [ ] Reference counting in block header
- [ ] CoW propagation on B+Tree updates
- [ ] Snapshot creation (copy root pointer + snapshot B+Tree)
- [ ] Full rollback
- [ ] Fast/incremental rollback

### Phase 4 — Encryption + Parallelism
- [ ] AES-128 XTS encrypt/decrypt (software implementation)
- [ ] PBKDF2 key derivation
- [ ] Transparent encryption in read/write block path
- [ ] Chunked parallel read (workqueue dispatch)
- [ ] yafsctl tool

### Phase 5 — Reliability + Polish
- [ ] Crash recovery (orphan block detection)
- [ ] Snapshot audit log
- [ ] Large-scale testing (many small files, large files, power-loss simulation)
