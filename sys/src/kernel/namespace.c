#include <namespace.h>
#include <kernel.h>
#include <console.h>
#include <string.h>

void mkrn_ns_init(m4k_namespace_t *ns)
{
    if (!ns) return;
    mkrn_memset(ns, 0, sizeof(m4k_namespace_t));
}

int mkrn_ns_set(m4k_namespace_t *ns, const char *path,
                const char *target, uint32_t flags)
{
    if (!ns || !path || !target)
        return -1;
    if (ns->entry_count >= M4K_NS_ENTRIES)
        return -1;

    m4k_ns_entry_t *e = &ns->entries[ns->entry_count];
    mkrn_strncpy(e->mnt_path, path, M4K_NS_PATH_MAX - 1);
    e->mnt_path[M4K_NS_PATH_MAX - 1] = '\0';
    mkrn_strncpy(e->target_path, target, M4K_NS_PATH_MAX - 1);
    e->target_path[M4K_NS_PATH_MAX - 1] = '\0';
    e->mnt_flags = flags;
    ns->entry_count++;

    return 0;
}

int mkrn_ns_resolve(m4k_namespace_t *ns, const char *path,
                    char *out, uint32_t out_sz)
{
    if (!ns || !path || !out || out_sz == 0)
        return -1;

    /* Check per-process namespace entries first */
    for (uint32_t i = 0; i < ns->entry_count; i++) {
        m4k_ns_entry_t *e = &ns->entries[i];
        size_t mlen = mkrn_strlen(e->mnt_path);

        /* Match mount point as prefix (must match whole path component) */
        if (mkrn_strncmp(path, e->mnt_path, mlen) == 0) {
            char rest[256];
            rest[0] = '\0';
            if (path[mlen] == '/') {
                mkrn_strncpy(rest, path + mlen, sizeof(rest) - 1);
            } else if (path[mlen] == '\0') {
                rest[0] = '\0';
            } else {
                continue;
            }

            mkrn_strncpy(out, e->target_path, out_sz - 1);
            out[out_sz - 1] = '\0';

            if (rest[0] != '\0') {
                size_t olen = mkrn_strlen(out);
                size_t rlen = mkrn_strlen(rest);
                if (olen + rlen + 1 < out_sz) {
                    if (out[olen - 1] == '/')
                        out[olen - 1] = '\0';
                    mkrn_strcat(out, rest);
                }
            }
            return 0;
        }
    }

    return -1; /* no namespace entry matched */
}

void mkrn_ns_copy(m4k_namespace_t *dst, const m4k_namespace_t *src)
{
    if (!dst || !src) return;
    mkrn_memcpy(dst, src, sizeof(m4k_namespace_t));
}
