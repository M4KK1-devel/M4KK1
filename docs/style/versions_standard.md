# M4KK1 4P1 Versioning & ISO Naming Standard
**Version**: v1.0  
**Status**: Active  
**Scope**: M4KK1 Project Artifacts (ISO, Binaries, Releases)

---

## 1. Core Naming Schema

All M4KK1 release images (ISO) and core binary packages **must** adhere to the following naming schema, they must under ./iso:

```text
m4kk1_<MAJOR>.<MINOR>.<PATCH>_build<BUILD>-<CLASSIFIER><CLASSIFIER_VER>.iso
```

**Example parsed name**:  
`m4kk1_1.7.2_build1267-alpha4.iso`

| Segment | Example Value | Meaning |
| :--- | :--- | :--- |
| **Product** | `m4kk1` | Fixed project prefix (lowercase). |
| **MAJOR** | `1` | Major architectural changes or breaking API changes (see §2). |
| **MINOR** | `7` | New features, backward‑compatible (**increments up to 19, then carries to MAJOR**, see §3). |
| **PATCH** | `2` | Bug fixes, security patches, performance tweaks (no new features). |
| **BUILD** | `1267` | Automatically incremented build number (starts at 1). |
| **CLASSIFIER** | `alpha` | Release stage identifier (see §4). |
| **CLASSIFIER_VER** | `4` | Iteration number for that classifier (4th alpha). |

---

## 2. Segment Definitions

### 2.1 MAJOR Version
- **Triggers**:
  - **Kernel ABI break**: changes to `int 0x4D` syscall numbers, or significant changes to core `mkrn_task_t` layout.
  - **Filesystem format change**: YAFS on‑disk format modifications (e.g., inode structure changes) that prevent mounting older images.
  - **Major architecture shift**: e.g., moving from i386 to x86_64 and dropping 32‑bit support, or rewriting the scheduler core causing behaviour changes.
- **Increment rule**: Manual (decided by the lead maintainer). If MINOR reaches 20 and carries over, MAJOR is incremented by 1 and MINOR resets to 0.

### 2.2 MINOR Version
- **Triggers**:
  - Addition of new system calls (`M4K_SYS_*`).
  - Addition of new standard tools (e.g., `chmod`, `usermod`).
  - Extension of `/sys/proc` fields or addition of new kernel‑exported files.
  - MINOR starts at 0 and increments up to 19.
  - When MINOR reaches `20`, it **must** carry over: `MAJOR += 1`, `MINOR = 0`.
  - *Example*: the version after `1.19.0` must be `2.0.0` (not `1.20.0`).
  - *Rationale*: This reflects the 4P1 philosophy of "minimalism" – forcing a consolidation and architectural review after 20 feature milestones.

### 2.3 PATCH Version
- **Triggers**:
  - Fixes for critical kernel crashes (Panics) or memory leaks.
  - Documentation corrections or build script compatibility fixes.
  - **No** new features or API changes allowed in a PATCH release.
- **Increment rule**: Incremented manually or by script after a hotfix. When MAJOR or MINOR increments, PATCH resets to 0.

---

## 3. Build Metadata

### 3.1 BUILD Number
- **Definition**: `build<BUILD>` is a strictly increasing integer uniquely identifying a build artifact.
- **Source**: Recommended to use CI/CD system build numbers (e.g., GitHub Actions `run-id`), or local developers can use the last 4 digits of `date +%s`.
- **Reset policy**: This number **never resets** over the entire project lifetime. It distinguishes artifacts built under the same version string but with different environments (e.g., different GCC versions).

### 3.2 CLASSIFIER and CLASSIFIER_VER
Identifies the software lifecycle stage, helping testers assess stability.

| Classifier | Meaning | When used | Example |
| :--- | :--- | :--- | :--- |
| `alpha` | Internal development | Unstable, core developers only | `m4kk1_2.0.0_build100-alpha1.iso` |
| `beta` | Public beta | Main features locked, early adopters | `m4kk1_2.0.0_build150-beta3.iso` |
| `rc` (Release Candidate) | Candidate for release | Critical bugs fixed, preparing for final | `m4kk1_2.0.0_build200-rc1.iso` |
| *(omitted)* | Stable release | Fully tested, production‑ready | `m4kk1_1.7.2_build1267.iso` |

**Rules**:
- When releasing a **Stable** build, the `-classifier` suffix must be omitted.
- Classifier version starts at 1 and increments each time a new build of the same classifier is released (e.g., `rc1` → `rc2`).

---

## 4. Mapping to Git Tags

To ease traceability, Git tags should map to ISO version strings as follows:

| Type | Git Tag Format | Corresponding ISO Name |
| :--- | :--- | :--- |
| Development snapshot | `v1.7.2-alpha1` | `m4kk1_1.7.2_buildXXX-alpha1.iso` |
| Release Candidate | `v1.7.2-rc2` | `m4kk1_1.7.2_buildXXX-rc2.iso` |
| Stable release | `v1.7.2` | `m4kk1_1.7.2_buildXXX.iso` |

---

## 5. Version Increment Decision Table

| Change Type | MAJOR | MINOR | PATCH | Example (current: `1.7.2`) |
| :--- | :--- | :--- | :--- | :--- |
| **Incompatible syscall added** | +1 | 0 | 0 | `2.0.0` |
| **New 4P1 standard tool added** | 0 | +1 | 0 | `1.8.0` (or `2.0.0` if MINOR was 19) |
| **Critical panic/memory leak fixed** | 0 | 0 | +1 | `1.7.3` |
| **Documentation/build script fixes** | 0 | 0 | +1 | `1.7.3` |
| **Internal refactoring (no API changes)** | 0 | 0 | +1 | `1.7.3` (**must** change PATCH to ensure rollback traceability) |

---

## 6. Automation and Storage

### 6.1 Root `VERSION` File
To ensure build consistency, the project root **must** contain a `VERSION` file with the following content:

```text
MAJOR=1
MINOR=7
PATCH=2
BUILD=1267
CLASSIFIER=alpha4
```

Build scripts (Makefile) must read this file before packaging the ISO to guarantee the ISO name matches the code state.

### 6.2 Multi‑Architecture Support
When M4KK1 supports ARM64 or RISC‑V, the architecture label should be inserted (before `_build`):

```text
m4kk1_1.7.2_arm64_build1267-alpha4.iso
m4kk1_1.7.2_x86_64_build1267.iso
```

---

**End of Document**