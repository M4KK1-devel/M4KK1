#!/usr/bin/env python3
"""Emit sys/src/net/net.c (protocol stack core) atomically."""
import os

BODY = r'''/*
 * M4KK1 4P1 - net.c
 * Description: Network stack core: device registry, Ethernet/ARP/IP
 * dispatch, ICMP echo, and the e1000 backend glue.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include "kernel.h"
#include "net.h"
#include "net_impl.h"

/* ── Device registry ── */
#define NET_MAX_DEVS 4
static mkrn_net_device_t *net_devs[NET_MAX_DEVS];
static int net_dev_count = 0;
static mkrn_net_device_t *net_default_dev = 0;

/* Static IP config (no DHCP yet; QEMU user-net defaults) */
static u32 net_local_ip  = 0x0A00020F;   /* 10.0.2.15 */
static u32 net_netmask   = 0xFFFFFF00;   /* 255.255.255.0 */
static u32 net_gateway   = 0x0A000202;   /* 10.0.2.2 */
static u8  net_gate_mac[6] = {0,0,0,0,0,0};

/* ── Checksum ── */
u16 mkrn_net_checksum(u16 *data, u16 len)
{
    u32 sum = 0;
    for (u16 i = 0; i < len / 2; i++)
        sum += data[i];
    if (len & 1)
        sum += ((u8 *)data)[len - 1];
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)(~sum);
}

u32 mkrn_net_ip_to_string(u32 ip, char *buffer)
{
    u8 *o = (u8 *)&ip;
    u32 n = 0;
    for (int i = 0; i < 4; i++) {
        int v = o[i];
        if (v >= 100) { buffer[n++] = '0' + v / 100; v %= 100; }
        if (v >= 10 || o[i] >= 100) buffer[n++] = '0' + v / 10;
        buffer[n++] = '0' + v % 10;
        if (i < 3) buffer[n++] = '.';
    }
    buffer[n] = 0;
    return n;
}

u32 mkrn_net_string_to_ip(const char *s)
{
    u32 ip = 0;
    int parts = 0;
    while (parts < 4) {
        int v = 0, digits = 0;
        while (*s >= '0' && *s <= '9') {
            v = v * 10 + (*s - '0');
            s++;
            digits++;
            if (digits > 3)
                return 0;
        }
        if (v > 255 || digits == 0)
            return 0;
        ip = (ip << 8) | (u32)v;
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

/* ── Ethernet send (IP frames) ── */
static int net_send_ip_eth(u32 dst_ip, u8 *pkt, u16 len)
{
    if (!net_default_dev || !net_default_dev->up)
        return -1;
    u8 dst_mac[6];
    /* gateway when off-subnet */
    u32 next_hop = ((dst_ip & net_netmask) == (net_local_ip & net_netmask))
                   ? dst_ip : net_gateway;
    if (mkrn_arp_resolve(next_hop, dst_mac) != 0) {
        /* broadcast fallback: send ARP request first, then drop;
         * caller retries after the reply lands in the cache. */
        mkrn_arp_send_request(next_hop);
        return -1;
    }
    return mkrn_net_send_ethernet(dst_mac, M4K_ETH_TYPE_IP, pkt, len);
}

int mkrn_net_send_packet(u32 dst_ip, u8 protocol, u8 *data, u16 len)
{
    /* data points at the transport payload; build the IP header in
     * front of it inside a scratch frame. */
    static u8 frame[1600];
    if (len > 1500 - 20)
        return -1;
    mkrn_ip_header_t *ip = (mkrn_ip_header_t *)frame;
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = htons(len + 20);
    ip->id = htons(net_ip_id++);
    ip->frag_offset = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = net_local_ip;
    ip->dst_ip = dst_ip;
    ip->checksum = mkrn_net_checksum((u16 *)ip, 20);
    for (u16 i = 0; i < len; i++)
        frame[20 + i] = data[i];
    return net_send_ip_eth(dst_ip, frame, len + 20);
}

/* ── Frame ingress (called by e1000 RX) ── */
void mkrn_net_receive_frame(u8 *frame, u16 len)
{
    if (len < sizeof(mkrn_eth_header_t))
        return;
    mkrn_eth_header_t *eth = (mkrn_eth_header_t *)frame;
    u16 etype = ntohs(eth->type);
    if (etype == M4K_ETH_TYPE_ARP) {
        mkrn_arp_handle_packet(frame + 14, len - 14);
        return;
    }
    if (etype != M4K_ETH_TYPE_IP)
        return;
    u8 *pkt = frame + 14;
    if (len < 34)
        return;
    mkrn_ip_header_t *ip = (mkrn_ip_header_t *)pkt;
    if ((ip->ver_ihl >> 4) != 4)
        return;
    u16 ihl = (ip->ver_ihl & 0x0F) * 4;
    if (ihl < 20 || ntohs(ip->total_len) < ihl)
        return;
    u8 *payload = pkt + ihl;
    u16 plen = ntohs(ip->total_len) - ihl;
    if (ip->protocol == M4K_IP_PROTOCOL_ICMP)
        mkrn_icmp_handle_packet(payload, plen, ip->src_ip, ip->dst_ip);
    else if (ip->protocol == M4K_IP_PROTOCOL_TCP)
        mkrn_tcp_handle_packet(payload, plen, ip->src_ip, ip->dst_ip);
    else if (ip->protocol == M4K_IP_PROTOCOL_UDP)
        mkrn_udp_handle_packet(payload, plen, ip->src_ip, net_local_ip);
}

/* ── e1000 backend ops ── */
static int net_e1000_init(mkrn_net_device_t *dev)
{
    extern int mkrn_e1000_init(void);
    return mkrn_e1000_init();
}

static int net_e1000_transmit(mkrn_net_device_t *dev, u8 *data, u32 len)
{
    (void)dev;
    extern int mkrn_e1000_transmit(const u8 *, u32);
    return mkrn_e1000_transmit(data, len);
}

static void net_e1000_poll(mkrn_net_device_t *dev)
{
    (void)dev;
    extern void mkrn_e1000_poll(void);
    mkrn_e1000_poll();
}

static mkrn_net_device_t e1000_dev = {
    "eth0", M4K_NET_DEV_ETHERNET, {0}, 0, 0, 0, 0,
    net_e1000_init, net_e1000_transmit, 0, net_e1000_poll, 0
};

/* ── Public registry API ── */
int mkrn_net_device_register(mkrn_net_device_t *dev)
{
    if (!dev || net_dev_count >= NET_MAX_DEVS)
        return -1;
    net_devs[net_dev_count++] = dev;
    if (!net_default_dev)
        net_default_dev = dev;
    return 0;
}

mkrn_net_device_t *mkrn_net_device_find(const char *name)
{
    for (int i = 0; i < net_dev_count; i++) {
        const char *a = name, *b = net_devs[i]->name;
        while (*a && *b && *a == *b) { a++; b++; }
        if (!*a && !*b)
            return net_devs[i];
    }
    return 0;
}

int mkrn_net_send_ethernet(u8 *dst_mac, u16 type, u8 *data, u16 len)
{
    static u8 ethframe[1600];
    if (!net_default_dev || !net_default_dev->up || len > 1500 - 14)
        return -1;
    mkrn_eth_header_t *eth = (mkrn_eth_header_t *)ethframe;
    for (int i = 0; i < 6; i++) {
        eth->dst_mac[i] = dst_mac[i];
        eth->src_mac[i] = net_default_dev->mac_addr[i];
    }
    eth->type = htons(type);
    for (u16 i = 0; i < len; i++)
        ethframe[14 + i] = data[i];
    return net_default_dev->transmit(net_default_dev, ethframe,
                                     len + 14u);
}

/* Timer-tick polling hook: timer.c calls this every tick. */
void mkrn_net_poll(void)
{
    for (int i = 0; i < net_dev_count; i++)
        if (net_devs[i]->poll)
            net_devs[i]->poll(net_devs[i]);
}

/* ICMP echo request transmit (used by ping) */
int mkrn_icmp_send_echo_request(u32 dst_ip, u16 id, u16 seq)
{
    static u8 buf[64];
    mkrn_icmp_header_t *ic = (mkrn_icmp_header_t *)buf;
    ic->type = 8;
    ic->code = 0;
    ic->checksum = 0;
    ic->data = ((u32)id << 16) | seq;
    ic->checksum = mkrn_net_checksum((u16 *)buf, 8);
    return mkrn_net_send_packet(dst_ip, M4K_IP_PROTOCOL_ICMP, buf, 8);
}

/* ICMP ingress */
void mkrn_icmp_handle_packet(u8 *packet, u16 len, u32 src_ip, u32 dst_ip)
{
    (void)dst_ip;
    if (len < 8)
        return;
    mkrn_icmp_header_t *ic = (mkrn_icmp_header_t *)packet;
    if (ic->type == 8 && ic->code == 0) {
        /* echo request: reply */
        static u8 rbuf[64];
        mkrn_icmp_header_t *rc = (mkrn_icmp_header_t *)rbuf;
        u16 rlen = len > 64 ? 64 : len;
        for (u16 i = 0; i < rlen; i++)
            rbuf[i] = packet[i];
        rc->type = 0;
        rc->checksum = 0;
        rc->checksum = mkrn_net_checksum((u16 *)rbuf, rlen);
        mkrn_net_send_packet(src_ip, M4K_IP_PROTOCOL_ICMP, rbuf, rlen);
    } else if (ic->type == 0) {
        /* echo reply: notify listeners (ping waiters) */
        extern void mkrn_ping_on_reply(u32 src);
        mkrn_ping_on_reply(src_ip);
    }
}

/* ── Stack init ── */
static u16 net_ip_id = 1;

int mkrn_net_init(void)
{
    net_dev_count = 0;
    net_default_dev = 0;
    net_ip_id = 1;
    mkrn_arp_init();
    mkrn_net_device_register(&e1000_dev);
    if (net_default_dev->init(net_default_dev) == 0) {
        net_default_dev->up = 1;
        const u8 *mac = 0;
        extern const u8 *mkrn_e1000_get_mac(void);
        mac = mkrn_e1000_get_mac();
        for (int i = 0; i < 6; i++)
            net_default_dev->mac_addr[i] = mac[i];
        return 0;
    }
    net_default_dev = 0;
    return -1;
}

u32 mkrn_net_get_ip(void) { return net_local_ip; }
u32 mkrn_net_get_gateway(void) { return net_gateway; }
u32 mkrn_net_get_netmask(void) { return net_netmask; }
void mkrn_net_set_ip(u32 ip, u32 mask, u32 gw)
{
    net_local_ip = ip;
    net_netmask = mask;
    net_gateway = gw;
}
'''

out = r"F:\M4KK1\sys\src\net\net.c"
with open(out, "w", newline="\n") as f:
    f.write(BODY)
print("wrote", out, len(BODY), "chars")
