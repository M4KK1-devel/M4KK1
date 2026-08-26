#!/usr/bin/env python3
"""Emit sys/src/net/tcp.c (minimal TCP client) atomically."""
import os

BODY = r'''/*
 * M4KK1 4P1 - tcp.c
 * Description: Minimal TCP client-side implementation: SYN connect,
 * established send/receive with a single in-order byte stream per
 * connection, and FIN close.  Enough for HTTP GET (wget).
 *
 * No retransmission, no window scaling, no congestion control: this
 * runs over the QEMU user-mode backend where loss is not modelled.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include "kernel.h"
#include "net.h"
#include "net_impl.h"

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed)) tcp_header_t;

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

typedef enum {
    TS_CLOSED = 0,
    TS_SYN_SENT,
    TS_ESTABLISHED,
    TS_FIN_WAIT,
    TS_CLOSE_WAIT
} tcp_state_t;

#define TCP_MAX_CONNS 4
#define TCP_RX_BUF    4096

typedef struct {
    uint8_t  used;
    uint8_t  state;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t snd_nxt;
    uint32_t rcv_nxt;
    uint8_t  rbuf[TCP_RX_BUF];
    uint16_t rlen;
} tcp_conn_t;

static tcp_conn_t tcp_conns[TCP_MAX_CONNS];
static uint16_t tcp_next_port = 1024;

void mkrn_tcp_init(void)
{
    for (int i = 0; i < TCP_MAX_CONNS; i++)
        tcp_conns[i].used = 0;
    tcp_next_port = 1024;
}

/* IPv4 pseudo-header checksum for TCP */
static uint16_t tcp_checksum(uint32_t src, uint32_t dst,
                             uint8_t *payload, uint16_t len)
{
    uint32_t sum = 0;
    uint16_t *w = (uint16_t *)payload;
    for (uint16_t i = 0; i < len / 2; i++)
        sum += w[i];
    if (len & 1)
        sum += ((uint8_t *)payload)[len - 1];
    sum += (src >> 16) & 0xFFFF;
    sum += src & 0xFFFF;
    sum += (dst >> 16) & 0xFFFF;
    sum += dst & 0xFFFF;
    sum += 6;              /* protocol TCP */
    sum += len;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static int tcp_emit(tcp_conn_t *c, uint8_t flags,
                    const uint8_t *data, uint16_t len)
{
    static uint8_t pkt[TCP_RX_BUF + 64];
    tcp_header_t *h = (tcp_header_t *)pkt;
    if (20 + len > sizeof(pkt))
        return -1;
    h->src_port = htons(c->local_port);
    h->dst_port = htons(c->remote_port);
    h->seq = htonl(c->snd_nxt);
    h->ack = htonl(c->rcv_nxt);
    h->data_offset = (5 << 4);
    h->flags = flags;
    h->window = htons(TCP_RX_BUF);
    h->checksum = 0;
    h->urgent = 0;
    for (uint16_t i = 0; i < len; i++)
        pkt[20 + i] = data[i];
    h->checksum = tcp_checksum(mkrn_net_get_ip(), c->remote_ip, pkt,
                               20 + len);
    /* data consumes sequence space */
    if (flags & TCP_SYN)
        c->snd_nxt++;
    c->snd_nxt += len;
    if (flags & TCP_FIN)
        c->snd_nxt++;
    return mkrn_net_send_packet(c->remote_ip, M4K_IP_PROTOCOL_TCP,
                                pkt, 20 + len);
}

int mkrn_tcp_connect(uint32_t remote_ip, uint16_t remote_port)
{
    tcp_conn_t *c = 0;
    for (int i = 0; i < TCP_MAX_CONNS; i++) {
        if (!tcp_conns[i].used) {
            c = &tcp_conns[i];
            break;
        }
    }
    if (!c)
        return -1;
    c->used = 1;
    c->state = TS_SYN_SENT;
    c->remote_ip = remote_ip;
    c->remote_port = remote_port;
    c->local_port = tcp_next_port++;
    c->snd_nxt = 0x1000;
    c->rcv_nxt = 0;
    c->rlen = 0;
    if (tcp_emit(c, TCP_SYN, 0, 0) != 0) {
        c->used = 0;
        return -1;
    }
    return (int)(c - tcp_conns);
}

int mkrn_tcp_send(int id, const uint8_t *data, int len)
{
    if (id < 0 || id >= TCP_MAX_CONNS)
        return -1;
    tcp_conn_t *c = &tcp_conns[id];
    if (!c->used || c->state != TS_ESTABLISHED)
        return -1;
    int off = 0;
    while (off < len) {
        int chunk = len - off;
        if (chunk > 1000)
            chunk = 1000;
        if (tcp_emit(c, TCP_ACK | TCP_PSH, data + off,
                     (uint16_t)chunk) != 0)
            return -1;
        off += chunk;
    }
    return len;
}

int mkrn_tcp_recv(int id, uint8_t *buf, int maxlen)
{
    if (id < 0 || id >= TCP_MAX_CONNS)
        return -1;
    tcp_conn_t *c = &tcp_conns[id];
    if (!c->used)
        return -1;
    if (c->rlen == 0)
        return 0;
    int n = c->rlen > maxlen ? maxlen : c->rlen;
    for (int i = 0; i < n; i++)
        buf[i] = c->rbuf[i];
    if (c->rlen > n) {
        for (uint16_t i = n; i < c->rlen; i++)
            c->rbuf[i - n] = c->rbuf[i];
    }
    c->rlen -= n;
    return n;
}

int mkrn_tcp_state(int id)
{
    if (id < 0 || id >= TCP_MAX_CONNS || !tcp_conns[id].used)
        return -1;
    return tcp_conns[id].state;
}

int mkrn_tcp_close(int id)
{
    if (id < 0 || id >= TCP_MAX_CONNS || !tcp_conns[id].used)
        return -1;
    tcp_conn_t *c = &tcp_conns[id];
    if (c->state == TS_ESTABLISHED) {
        tcp_emit(c, TCP_FIN | TCP_ACK, 0, 0);
        c->state = TS_FIN_WAIT;
        return 0;
    }
    c->used = 0;
    return 0;
}

void mkrn_tcp_handle_packet(uint8_t *packet, uint16_t len,
                            uint32_t src_ip, uint32_t dst_ip)
{
    (void)dst_ip;
    if (len < 20)
        return;
    tcp_header_t *h = (tcp_header_t *)packet;
    uint16_t hdrlen = (h->data_offset >> 4) * 4;
    if (hdrlen < 20 || hdrlen > len)
        return;

    for (int i = 0; i < TCP_MAX_CONNS; i++) {
        tcp_conn_t *c = &tcp_conns[i];
        if (!c->used || c->remote_ip != src_ip)
            continue;
        if (ntohs(h->src_port) != c->remote_port ||
            ntohs(h->dst_port) != c->local_port)
            continue;

        if (c->state == TS_SYN_SENT && (h->flags & TCP_SYN)
            && (h->flags & TCP_ACK)) {
            c->rcv_nxt = ntohl(h->seq) + 1;
            tcp_emit(c, TCP_ACK, 0, 0);
            c->state = TS_ESTABLISHED;
            return;
        }
        if (h->flags & TCP_RST) {
            c->state = TS_CLOSED;
            c->used = 0;
            return;
        }
        if (c->state == TS_ESTABLISHED || c->state == TS_FIN_WAIT
            || c->state == TS_CLOSE_WAIT) {
            uint16_t dlen = len - hdrlen;
            if (dlen > 0) {
                uint32_t seq = ntohl(h->seq);
                if (seq == c->rcv_nxt) {
                    uint16_t space = TCP_RX_BUF - c->rlen;
                    if (dlen > space)
                        dlen = space;
                    for (uint16_t k = 0; k < dlen; k++)
                        c->rbuf[c->rlen + k] = packet[hdrlen + k];
                    c->rlen += dlen;
                    c->rcv_nxt = seq + dlen;
                    tcp_emit(c, TCP_ACK, 0, 0);
                } else {
                    /* out-of-order: ACK the expected seq (no retry) */
                    tcp_emit(c, TCP_ACK, 0, 0);
                }
            }
            if (h->flags & TCP_FIN) {
                c->rcv_nxt++;
                tcp_emit(c, TCP_ACK, 0, 0);
                if (c->state == TS_ESTABLISHED)
                    c->state = TS_CLOSE_WAIT;
            }
            return;
        }
    }
}
'''

out = r"F:\M4KK1\sys\src\net\tcp.c"
with open(out, "w", newline="\n") as f:
    f.write(BODY)
print("wrote", out, os.path.getsize(out), "bytes")
