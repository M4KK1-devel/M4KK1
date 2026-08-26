#!/usr/bin/env python3
"""Emit sys/src/kernel/syscall/syscall_net.c atomically."""
import os

BODY = r'''/*
 * M4KK1 4P1 - syscall_net.c
 * Description: Network system-call surface for userland tools
 * (ifconfig / ping / wget).  Thin wrappers over the in-kernel stack.
 *
 *   0x90 NETINFO  (out_ip, out_mask, outgw, mac_out) -> up flag
 *   0x91 PING     (dst_ip, timeout_ms) -> 0 reply / -1 timeout
 *   0x92 NETTCP_CONNECT (dst_ip, port) -> conn id / -1
 *   0x93 NETTCP_SEND    (id, buf, len) -> sent / -1
 *   0x94 NETTCP_RECV    (id, buf, maxlen) -> n / -1
 *   0x95 NETTCP_CLOSE   (id) -> 0 / -1
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include "kernel.h"
#include "syscall.h"
#include "net.h"

/* ping wait state (single waiter: the ping syscall is synchronous) */
static volatile int ping_got_reply = 0;
static volatile uint32_t ping_from_ip = 0;

void mkrn_ping_on_reply(uint32_t src_ip)
{
    ping_from_ip = src_ip;
    ping_got_reply = 1;
}

static u32 mkrn_syscall_netinfo_impl(
    u32 uArg1, u32 uArg2, u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg4;
    (void)uArg5;
    uint32_t *ip   = (uint32_t *)uArg1;
    uint32_t *mask = (uint32_t *)uArg2;
    uint32_t *gw   = (uint32_t *)uArg3;
    mkrn_net_device_t *dev = mkrn_net_device_find("eth0");
    if (ip)
        *ip = mkrn_net_get_ip();
    if (mask)
        *mask = mkrn_net_get_netmask();
    if (gw)
        *gw = mkrn_net_get_gateway();
    return dev && dev->up ? 1 : 0;
}

static u32 mkrn_syscall_ping_impl(
    u32 uArg1, u32 uArg2, u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg3; (void)uArg4; (void)uArg5;
    uint32_t dst = uArg1;
    uint32_t timeout_ms = uArg2 ? uArg2 : 3000;
    uint32_t waited = 0;

    ping_got_reply = 0;
    if (mkrn_icmp_send_echo_request(dst, 0x1234, 1) != 0)
        return (u32)-1;

    /* Coarse wait: timer ticks are 1ms; use a simple halt-poll loop.
     * Interrupts drive RX polling via the timer hook, and m4k sleeps
     * are unavailable inside a syscall. */
    while (!ping_got_reply && waited < timeout_ms) {
        for (volatile int i = 0; i < 20000; i++)
            ;
        waited++;
    }
    if (!ping_got_reply)
        return (u32)-1;
    return ping_from_ip;
}

static u32 mkrn_syscall_tcp_connect_impl(
    u32 uArg1, u32 uArg2, u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg3; (void)uArg4; (void)uArg5;
    int id = mkrn_tcp_connect(uArg1, (uint16_t)uArg2);
    if (id < 0)
        return (u32)-1;
    /* wait briefly for ESTABLISHED */
    for (int i = 0; i < 400; i++) {
        if (mkrn_tcp_state(id) == 2)   /* TS_ESTABLISHED */
            return (u32)id;
        for (volatile int d = 0; d < 20000; d++)
            ;
    }
    return (u32)-1;
}

static u32 mkrn_syscall_tcp_send_impl(
    u32 uArg1, u32 uArg2, u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg4; (void)uArg5;
    int n = mkrn_tcp_send((int)uArg1, (const uint8_t *)uArg2,
                          (int)uArg3);
    return n < 0 ? (u32)-1 : (u32)n;
}

static u32 mkrn_syscall_tcp_recv_impl(
    u32 uArg1, u32 uArg2, u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg4; (void)uArg5;
    int n = mkrn_tcp_recv((int)uArg1, (uint8_t *)uArg2, (int)uArg3);
    return n < 0 ? (u32)-1 : (u32)n;
}

static u32 mkrn_syscall_tcp_close_impl(
    u32 uArg1, u32 uArg2, u32 uArg3, u32 uArg4, u32 uArg5)
{
    (void)uArg2; (void)uArg3; (void)uArg4; (void)uArg5;
    return mkrn_tcp_close((int)uArg1) < 0 ? (u32)-1 : 0;
}

void mkrn_syscall_net_init(void)
{
    mkrn_syscall_register(M4K_SC_NETINFO, mkrn_syscall_netinfo_impl);
    mkrn_syscall_register(M4K_SC_PING, mkrn_syscall_ping_impl);
    mkrn_syscall_register(M4K_SC_NETTCP_CONNECT, mkrn_syscall_tcp_connect_impl);
    mkrn_syscall_register(M4K_SC_NETTCP_SEND, mkrn_syscall_tcp_send_impl);
    mkrn_syscall_register(M4K_SC_NETTCP_RECV, mkrn_syscall_tcp_recv_impl);
    mkrn_syscall_register(M4K_SC_NETTCP_CLOSE, mkrn_syscall_tcp_close_impl);
}
'''

out = r"F:\M4KK1\sys\src\kernel\syscall\syscall_net.c"
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "w", newline="\n") as f:
    f.write(BODY)
print("wrote", out, os.path.getsize(out), "bytes")
