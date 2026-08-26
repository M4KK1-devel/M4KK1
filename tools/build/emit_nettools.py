#!/usr/bin/env python3
"""Emit usr/src/cmd/ping.c, wget.c, ifconfig.c atomically."""
import os

PING = r'''/*
 * M4KK1 4P1 - ping.c
 * Description: ICMP echo tool (network stack verification).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

static void ip_str(uint32_t ip, char *out)
{
    uint8_t *o = (uint8_t *)&ip;
    int n = 0;
    for (int i = 0; i < 4; i++) {
        int v = o[i];
        if (v >= 100) { out[n++] = '0' + v / 100; v %= 100; }
        if (v >= 10 || o[i] >= 100) out[n++] = '0' + v / 10;
        out[n++] = '0' + v % 10;
        if (i < 3) out[n++] = '.';
    }
    out[n] = 0;
}

static uint32_t parse_ip(const char *s)
{
    uint32_t ip = 0;
    int parts = 0;
    while (parts < 4) {
        int v = 0, digits = 0;
        while (*s >= '0' && *s <= '9') {
            v = v * 10 + (*s - '0');
            s++;
            digits++;
        }
        if (v > 255 || digits == 0 || digits > 3)
            return 0;
        ip = (ip << 8) | (uint32_t)v;
        parts++;
        if (parts < 4) {
            if (*s != '.')
                return 0;
            s++;
        }
    }
    if (*s)
        return 0;
    return ip;
}

int musr_cmd_ping(int argc, char **argv)
{
    if (argc < 2) {
        musr_puts("usage: ping <ip>\n");
        return 1;
    }
    uint32_t dst = parse_ip(argv[1]);
    if (!dst) {
        musr_puts("ping: invalid address\n");
        return 1;
    }
    char bufs[16];
    ip_str(dst, bufs);
    musr_puts("PING ");
    musr_puts(bufs);
    musr_puts(" (");
    musr_puts(argv[1]);
    musr_puts(")\n");

    uint32_t r = musr_sc2(S_PING, dst, 3000);
    if (r == 0xFFFFFFFFu) {
        musr_puts("no reply (timeout)\n");
        return 1;
    }
    musr_puts("reply from ");
    ip_str(r, bufs);
    musr_puts(bufs);
    musr_puts(": time=---ms\n");
    return 0;
}
'''

WGET = r'''/*
 * M4KK1 4P1 - wget.c
 * Description: Minimal HTTP GET client over the kernel TCP stack.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

static uint32_t parse_ip(const char *s)
{
    uint32_t ip = 0;
    int parts = 0;
    while (parts < 4) {
        int v = 0, digits = 0;
        while (*s >= '0' && *s <= '9') {
            v = v * 10 + (*s - '0');
            s++;
            digits++;
        }
        if (v > 255 || digits == 0 || digits > 3)
            return 0;
        ip = (ip << 8) | (uint32_t)v;
        parts++;
        if (parts < 4) {
            if (*s != '.')
                return 0;
            s++;
        }
    }
    if (*s)
        return 0;
    return ip;
}

int musr_cmd_wget(int argc, char **argv)
{
    if (argc < 2) {
        musr_puts("usage: wget http://<ip>[:port]/<path> [outfile]\n");
        return 1;
    }
    const char *url = argv[1];
    if (url[0]!='h'||url[1]!='t'||url[2]!='t'||url[3]!='p'
        ||url[4]!=':'||url[5]!='/'||url[6]!='/') {
        musr_puts("wget: only http:// supported\n");
        return 1;
    }
    const char *host = url + 7;
    uint32_t ip = 0;
    uint16_t port = 80;
    /* host part: up to ':' or '/' */
    char hostbuf[24];
    int hn = 0;
    while (*host && *host != ':' && *host != '/' && hn < 23)
        hostbuf[hn++] = *host++;
    hostbuf[hn] = 0;
    if (*host == ':') {
        host++;
        int p = 0;
        while (*host >= '0' && *host <= '9') {
            p = p * 10 + (*host - '0');
            host++;
        }
        if (p > 0 && p < 65536)
            port = (uint16_t)p;
    }
    const char *path = "/";
    if (*host == '/')
        path = host;
    ip = parse_ip(hostbuf);
    if (!ip) {
        musr_puts("wget: host must be a literal IP\n");
        return 1;
    }

    musr_puts("wget: connecting ");
    musr_puts(hostbuf);
    musr_puts("...\n");

    uint32_t cid = musr_sc2(S_TCPCONN, ip, port);
    if (cid == 0xFFFFFFFFu) {
        musr_puts("wget: connect failed\n");
        return 1;
    }

    static char req[512];
    static char buf[2048];
    int rn = 0;
    req[rn++]='G';req[rn++]='E';req[rn++]='T';req[rn++]=' ';
    while (*path) req[rn++] = *path++;
    req[rn++]=' ';req[rn++]='H';req[rn++]='T';req[rn++]='T';
    req[rn++]='P';req[rn++]='/';req[rn++]='1';req[rn++]='.';
    req[rn++]='0';req[rn++]]='\r';req[rn++]='\n';
    const char *hl = "Host: ";
    while (*hl) req[rn++] = *hl++;
    for (int i = 0; hostbuf[i]; i++) req[rn++] = hostbuf[i];
    req[rn++]='\r';req[rn++]='\n';
    const char *cl = "Connection: close\r\n\r\n";
    while (*cl) req[rn++] = *cl++;

    uint32_t sn = musr_sc3(S_TCPSEND, cid, (uint32_t)req, rn);
    if (sn == 0xFFFFFFFFu) {
        musr_puts("wget: send failed\n");
        musr_sc1(S_TCPCLOSE, cid);
        return 1;
    }

    int total = 0, header_done = 0, body_bytes = 0;
    int out_fd = -1;
    const char *outfile = argc >= 3 ? argv[2] : "index.html";
    for (;;) {
        uint32_t n = musr_sc3(S_TCPRECV, cid, (uint32_t)buf, 2048);
        if (n == 0xFFFFFFFFu)
            break;
        if (n == 0) {
            /* no data yet: spin a bit; give up after quiet period */
            static int idle;
            for (volatile int d = 0; d < 500000; d++)
                ;
            if (++idle > 40)
                break;
            continue;
        }
        int off = 0;
        if (!header_done) {
            for (int i = 0; i + 3 < (int)n; i++) {
                if (buf[i]=='\r' && buf[i+1]=='\n'
                    && buf[i+2]=='\r' && buf[i+3]=='\n') {
                    header_done = 1;
                    off = i + 4;
                    break;
                }
            }
            if (!header_done)
                continue;   /* header not complete yet */
        }
        if (out_fd < 0) {
            out_fd = musr_sc_open((char *)outfile, 0x101 /*O_WRONLY|O_CREAT*/);
            if (out_fd < 0) {
                musr_puts("wget: cannot create ");
                musr_puts((char *)outfile);
                musr_puts("\n");
                break;
            }
        }
        int w = musr_sc_write(out_fd, buf + off, n - off);
        if (w < 0)
            break;
        body_bytes += w;
        total += n;
        musr_puts(".");
    }
    if (out_fd >= 0)
        musr_sc_close(out_fd);
    musr_sc1(S_TCPCLOSE, cid);
    musr_puts("\nwget: saved ");
    char num[12];
    int q = body_bytes, nd = 0;
    char tmp[12];
    if (q == 0) tmp[nd++] = '0';
    while (q > 0) { tmp[nd++] = '0' + q % 10; q /= 10; }
    for (int i = 0; i < nd; i++) num[i] = tmp[nd - 1 - i];
    num[nd] = 0;
    musr_puts(num);
    musr_puts(" bytes to ");
    musr_puts((char *)outfile);
    musr_puts("\n");
    return body_bytes > 0 ? 0 : 1;
}
'''

IFCFG = r'''/*
 * M4KK1 4P1 - ifconfig.c
 * Description: Show network interface configuration.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

static void ip_str(uint32_t ip, char *out)
{
    uint8_t *o = (uint8_t *)&ip;
    int n = 0;
    for (int i = 0; i < 4; i++) {
        int v = o[i];
        if (v >= 100) { out[n++] = '0' + v / 100; v %= 100; }
        if (v >= 10 || o[i] >= 100) out[n++] = '0' + v / 10;
        out[n++] = '0' + v % 10;
        if (i < 3) out[n++] = '.';
    }
    out[n] = 0;
}

int musr_cmd_ifconfig(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    uint32_t ip, mask, gw;
    uint32_t up = musr_sc3(S_NETINFO, (uint32_t)&ip,
                           (uint32_t)&mask, (uint32_t)&gw);
    char b[16];
    musr_puts("eth0: ");
    if (!up) {
        musr_puts("down\n");
        return 1;
    }
    ip_str(ip, b);
    musr_puts("inet ");
    musr_puts(b);
    musr_puts(" netmask ");
    ip_str(mask, b);
    musr_puts(b);
    musr_puts(" gateway ");
    ip_str(gw, b);
    musr_puts(b);
    musr_puts("\n");
    return 0;
}
'''

base = r"F:\M4KK1\usr\src\cmd"
for name, body in (("ping", PING), ("wget", WGET), ("ifconfig", IFCFG)):
    p = os.path.join(base, name + ".c")
    with open(p, "w", newline="\n") as f:
        f.write(body)
    print("wrote", p, os.path.getsize(p))
