/*
 * M4KK1 4P1 - net_impl.h
 * Description: Internal network-stack helpers shared between the
 * net translation units (byte order + cross-file prototypes).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include "net.h"

static inline uint16_t htons(uint16_t v)
{
    return (uint16_t)((v << 8) | (v >> 8));
}

static inline uint16_t ntohs(uint16_t v)
{
    return (uint16_t)((v << 8) | (v >> 8));
}

static inline uint32_t htonl(uint32_t v)
{
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8)
           | ((v >> 8) & 0xFF00) | (v >> 24);
}

static inline uint32_t ntohl(uint32_t v)
{
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8)
           | ((v >> 8) & 0xFF00) | (v >> 24);
}

/* Frame ingress from the NIC driver (e1000.c) */
void mkrn_net_receive_frame(uint8_t *frame, uint16_t len);

/* ARP (arp.c) */
void mkrn_arp_init(void);
int  mkrn_arp_resolve(uint32_t ip, uint8_t *mac);
int  mkrn_arp_send_request(uint32_t target_ip);
void mkrn_arp_handle_packet(uint8_t *packet, uint16_t len);

/* TCP (tcp.c) */
void mkrn_tcp_handle_packet(uint8_t *packet, uint16_t len,
                            uint32_t src_ip, uint32_t dst_ip);

/* UDP (net.c) */
void mkrn_udp_handle_packet(uint8_t *packet, uint16_t len,
                            uint32_t src_ip, uint32_t dst_ip);

/* ICMP (net.c) */
void mkrn_icmp_handle_packet(uint8_t *packet, uint16_t len,
                             uint32_t src_ip, uint32_t dst_ip);

/* Ping waiter hook (implemented by ping wait/char device) */
void mkrn_ping_on_reply(uint32_t src_ip);

/* Config accessors (net.c) */
uint32_t mkrn_net_get_ip(void);
uint32_t mkrn_net_get_gateway(void);
uint32_t mkrn_net_get_netmask(void);
void     mkrn_net_set_ip(uint32_t ip, uint32_t mask, uint32_t gw);
