/*
 * M4KK1 4P1 - pwd.c
 * Description: Password hashing (SHA-256), passwd.db parser, login.conf parser
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * §6.5, §6.7: passwd.db format, SHA-256 password hashing
 * §6.10: login.conf parser
 */

#include "../m4sh.h"

/* ── SHA-256 (§6.7) ── */

#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z) (((x)&(y))^((~(x))&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define EP0(x) (ROTR(x,2)^ROTR(x,13)^ROTR(x,22))
#define EP1(x) (ROTR(x,6)^ROTR(x,11)^ROTR(x,25))
#define SIG0(x) (ROTR(x,7)^ROTR(x,18)^((x)>>3))
#define SIG1(x) (ROTR(x,17)^ROTR(x,19)^((x)>>10))

typedef struct {
    uint32_t state[8];
    uint32_t count;
    uint8_t buf[SHA256_BLOCK_SIZE];
} sha256_ctx_t;

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t *block)
{
    uint32_t w[64], a,b,c,d,e,f,g,h,t1,t2;
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[4*i]<<24)|(block[4*i+1]<<16)|(block[4*i+2]<<8)|block[4*i+3];
    for (int i = 16; i < 64; i++)
        w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (int i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e,f,g) + sha256_k[i] + w[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx)
{
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        ctx->buf[ctx->count++] = data[i];
        if (ctx->count == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->buf);
            ctx->count = 0;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t *digest)
{
    uint64_t bits = ctx->count * 8;
    ctx->buf[ctx->count++] = 0x80;
    if (ctx->count > 56) {
        while (ctx->count < SHA256_BLOCK_SIZE)
            ctx->buf[ctx->count++] = 0;
        sha256_transform(ctx, ctx->buf);
        ctx->count = 0;
    }
    while (ctx->count < 56)
        ctx->buf[ctx->count++] = 0;
    for (int i = 0; i < 8; i++)
        ctx->buf[56+i] = (bits >> (56 - i*8)) & 0xFF;
    sha256_transform(ctx, ctx->buf);
    for (int i = 0; i < 8; i++) {
        digest[4*i]   = (ctx->state[i] >> 24) & 0xFF;
        digest[4*i+1] = (ctx->state[i] >> 16) & 0xFF;
        digest[4*i+2] = (ctx->state[i] >> 8) & 0xFF;
        digest[4*i+3] = ctx->state[i] & 0xFF;
    }
}

static void to_hex(const uint8_t *bin, uint32_t len, char *hex)
{
    static const char hextab[] = "0123456789abcdef";
    for (uint32_t i = 0; i < len; i++) {
        hex[2*i]   = hextab[(bin[i] >> 4) & 0xF];
        hex[2*i+1] = hextab[bin[i] & 0xF];
    }
    hex[2*len] = '\0';
}

static int from_hex(const char *hex, uint8_t *bin, uint32_t max)
{
    uint32_t len = 0;
    while (hex[0] && hex[1] && len < max) {
        uint8_t hi = 0, lo = 0;
        char c = hex[0];
        if (c >= '0' && c <= '9') hi = c - '0';
        else if (c >= 'a' && c <= 'f') hi = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') hi = c - 'A' + 10;
        else return -1;
        c = hex[1];
        if (c >= '0' && c <= '9') lo = c - '0';
        else if (c >= 'a' && c <= 'f') lo = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') lo = c - 'A' + 10;
        else return -1;
        bin[len++] = (hi << 4) | lo;
        hex += 2;
    }
    return (int)len;
}

#define SALT_SIZE 16

/* 内部：使用指定盐值计算 SHA-256 哈希，输出纯哈希十六进制（不带盐前缀） */
static void hash_with_salt(const char *password, const uint8_t *salt, char *hash_hex)
{
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)password, musr_strlen(password));
    sha256_update(&ctx, salt, SALT_SIZE);
    uint8_t digest[SHA256_DIGEST_SIZE];
    sha256_final(&ctx, digest);
    to_hex(digest, SHA256_DIGEST_SIZE, hash_hex);
}

void musr_hash_password(const char *password, const uint8_t *salt, char *out)
{
    /* 输出格式：salt_hex$hash_hex */
    to_hex(salt, SALT_SIZE, out);
    out[2 * SALT_SIZE] = '$';
    hash_with_salt(password, salt, out + 2 * SALT_SIZE + 1);
}

int musr_verify_password(const char *password, const char *stored_hash)
{
    /* 期望格式长度：32(salt_hex) + 1('$') + 64(hash_hex) = 97 */
    if (musr_strlen(stored_hash) != 2 * SALT_SIZE + 1 + 64)
        return 0;
    uint8_t salt[SALT_SIZE];
    if (from_hex(stored_hash, salt, SALT_SIZE) != SALT_SIZE)
        return 0;
    char computed[65];
    hash_with_salt(password, salt, computed);
    return musr_strcmp(stored_hash + 2 * SALT_SIZE + 1, computed) == 0;
}

void musr_make_password_hash(const char *password, char *hash_out)
{
    uint8_t salt[SALT_SIZE];
    uint32_t t = (uint32_t)musr_sc_time();
    uint32_t u = musr_sc_uptime();
    uint32_t a = (uint32_t)(uintptr_t)&hash_out;
    for (int i = 0; i < SALT_SIZE; i++) {
        salt[i] = (uint8_t)((t ^ u ^ a ^ (t << (i & 7)) ^ (u >> (i & 7))) & 0xFF);
        t = t * 1103515245u + 12345u;
        u = u * 1664525u + 1013904223u;
    }
    musr_hash_password(password, salt, hash_out);
}

/* ── passwd.db entry ── */

#define MAX_LINE 512

static int parse_passwd_line(const char *line, passwd_entry_t *entry)
{
    const char *p = line;
    char *out = entry->username;
    int field = 0, oi = 0;
    int max_len[] = {64, 0, 0, 128, 64, 128, 256};
    char *targets[] = {entry->username, NULL, NULL, entry->home, entry->shell, entry->gecos, entry->password_hash};
    uint32_t *uints[] = {NULL, &entry->uid, &entry->gid, NULL, NULL, NULL, NULL};

    while (*p) {
        if (*p == ':') {
            if (targets[field] && targets[field] != entry->username)
                targets[field][oi] = '\0';
            field++;
            oi = 0;
            p++;
            if (field > 6) return -1;
            continue;
        }
        if (field == 0 && oi < max_len[field] - 1) {
            out[oi++] = *p;
            out[oi] = '\0';
        } else if (field == 1 && *p >= '0' && *p <= '9') {
            entry->uid = entry->uid * 10 + (*p - '0');
        } else if (field == 2 && *p >= '0' && *p <= '9') {
            entry->gid = entry->gid * 10 + (*p - '0');
        } else if (field >= 3 && field <= 6 && targets[field] && oi < max_len[field] - 1) {
            targets[field][oi++] = *p;
            targets[field][oi] = '\0';
        }
        p++;
    }
    return 0;
}

int musr_read_passwd_db(passwd_entry_t *entries, int max)
{
    int fd = musr_sc_open("/export/cfg/passwd.db", 0);
    if (fd < 0)
        return 0;
    char buf[2048];
    int n = musr_sc_read(fd, buf, sizeof(buf) - 1);
    musr_sc_close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';

    int count = 0;
    char line[MAX_LINE];
    int li = 0;
    for (int i = 0; i < n && count < max; i++) {
        if (buf[i] == '\n') {
            line[li] = '\0';
                if (li > 0 && line[0] != '#') {
                passwd_entry_t *e = &entries[count];
                memset(e, 0, sizeof(passwd_entry_t));
                if (parse_passwd_line(line, e) == 0)
                    count++;
            }
            li = 0;
        } else if (li < MAX_LINE - 1) {
            line[li++] = buf[i];
        }
    }
    if (count == 0)
        return 0;
    return count;
}

int musr_getpwnam(const char *name, passwd_entry_t *out)
{
    passwd_entry_t entries[32];
    int n = musr_read_passwd_db(entries, 32);
    for (int i = 0; i < n; i++) {
        if (musr_strcmp(entries[i].username, name) == 0) {
            *out = entries[i];
            return 0;
        }
    }
    return -1;
}

int musr_getpwuid(uint32_t uid, passwd_entry_t *out)
{
    passwd_entry_t entries[32];
    int n = musr_read_passwd_db(entries, 32);
    for (int i = 0; i < n; i++) {
        if (entries[i].uid == uid) {
            *out = entries[i];
            return 0;
        }
    }
    return -1;
}

int musr_update_passwd_db(const passwd_entry_t *entries, int count)
{
    char buf[4096];
    int pos = 0;
    for (int i = 0; i < count; i++) {
        pos += musr_strlen(entries[i].username);
        buf[pos-1] = ':'; /* will fix below */
        /* simpler: use snprintf equivalent */
    }
    /* Manual format */
    pos = 0;
    for (int i = 0; i < count; i++) {
        const passwd_entry_t *e = &entries[i];
        char uid_str[16], gid_str[16];
        int ui = 0, gi = 0;
        uint32_t tuid = e->uid, tgid = e->gid;
        do { uid_str[ui++] = '0' + (tuid % 10); tuid /= 10; } while (tuid);
        do { gid_str[gi++] = '0' + (tgid % 10); tgid /= 10; } while (tgid);
        for (int j = 0; j < ui/2; j++) { char t = uid_str[j]; uid_str[j] = uid_str[ui-1-j]; uid_str[ui-1-j] = t; }
        for (int j = 0; j < gi/2; j++) { char t = gid_str[j]; gid_str[j] = gid_str[gi-1-j]; gid_str[gi-1-j] = t; }
        uid_str[ui] = '\0';
        gid_str[gi] = '\0';

        /* username:uid:gid:home:shell:gecos:password_hash */
        int left = sizeof(buf) - pos - 1;
        if (left < 10) break;
        for (int k = 0; e->username[k] && k < 63; k++) buf[pos++] = e->username[k];
        buf[pos++] = ':';
        for (int k = 0; uid_str[k]; k++) buf[pos++] = uid_str[k];
        buf[pos++] = ':';
        for (int k = 0; gid_str[k]; k++) buf[pos++] = gid_str[k];
        buf[pos++] = ':';
        for (int k = 0; e->home[k] && k < 127; k++) buf[pos++] = e->home[k];
        buf[pos++] = ':';
        for (int k = 0; e->shell[k] && k < 63; k++) buf[pos++] = e->shell[k];
        buf[pos++] = ':';
        for (int k = 0; e->gecos[k] && k < 127; k++) buf[pos++] = e->gecos[k];
        buf[pos++] = ':';
        for (int k = 0; e->password_hash[k] && k < 255; k++) buf[pos++] = e->password_hash[k];
        buf[pos++] = '\n';
    }
    if (pos == 0) return -1;

    int fd = musr_sc_open("/export/cfg/passwd.db", 0x102); /* O_WRONLY|O_CREAT|O_TRUNC */
    if (fd < 0) fd = musr_sc_open("/export/cfg/passwd.db", 0x01); /* O_WRONLY */
    if (fd < 0) return -1;
    int written = musr_sc_write(fd, buf, pos);
    musr_sc_close(fd);
    return (written == pos) ? 0 : -1;
}

/* ── login.conf parser (§6.10) ── */

static uint64_t parse_size(const char *s)
{
    uint64_t v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    if (*s == 'K' || *s == 'k') v *= 1024;
    else if (*s == 'M' || *s == 'm') v *= 1024*1024;
    else if (*s == 'G' || *s == 'g') v *= 1024*1024*1024;
    return v;
}

int musr_parse_login_conf(const char *username, login_class_t *out)
{
    (void)username;
    /* Set defaults per §6.10.2 */
    memset(out, 0, sizeof(login_class_t));
    musr_strncpy(out->class_name, "default", sizeof(out->class_name)-1);
    out->cputime = ~0ULL;
    out->datasize = 256 * 1024 * 1024ULL;
    out->stacksize = 8 * 1024 * 1024ULL;
    out->maxproc = 100;
    out->openfiles = 64;

    int fd = musr_sc_open("/export/login.conf", 0);
    if (fd < 0) return 0;

    char buf[2048];
    int n = musr_sc_read(fd, buf, sizeof(buf) - 1);
    musr_sc_close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';

    char line[256];
    int li = 0;
    for (int i = 0; i <= n; i++) {
        if (buf[i] == '\n' || buf[i] == '\0' || li >= 254) {
            line[li] = '\0';
            if (li > 0 && line[0] != '#') {
                if (line[li-1] == '\\') {
                    /* continuation - skip for now */
                }
                const char *p = line;
                if (p[0] != ':' && p[0] != '\t') {
                    /* class name line */
                    char cls[32];
                    int ci = 0;
                    while (*p && *p != ':' && *p != '\\' && ci < 31)
                        cls[ci++] = *p++;
                    cls[ci] = '\0';
                    /* parse subsequent :key=value: entries */
                    while (*p) {
                        if (*p == ':') {
                            p++;
                            char key[32], val[64];
                            int ki = 0, vi = 0;
                            while (*p && *p != '=' && ki < 31)
                                key[ki++] = *p++;
                            if (*p == '=') p++;
                            while (*p && *p != ':' && vi < 63)
                                val[vi++] = *p++;
                            key[ki] = '\0'; val[vi] = '\0';
                            if (musr_strcmp(key, "cputime") == 0) {
                                out->cputime = (musr_strcmp(val, "infinity") == 0) ? ~0ULL : parse_size(val);
                            } else if (musr_strcmp(key, "datasize") == 0) {
                                out->datasize = parse_size(val);
                            } else if (musr_strcmp(key, "stacksize") == 0) {
                                out->stacksize = parse_size(val);
                            } else if (musr_strcmp(key, "maxproc") == 0) {
                                out->maxproc = (uint32_t)parse_size(val);
                            } else if (musr_strcmp(key, "openfiles") == 0) {
                                out->openfiles = (uint32_t)parse_size(val);
                            }
                        } else p++;
                    }
                }
            }
            li = 0;
        } else {
            line[li++] = buf[i];
        }
    }
    return 0;
}
