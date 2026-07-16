/*
 * M4KK1 4P1 - arp.c
 * Description: Basic ARP protocol implementation.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <string.h>
#include "net.h"

typedef struct {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t  hardware_len;
    uint8_t  protocol_len;
    uint16_t operation;
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
} __attribute__((packed)) arp_header_t;

typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    int      valid;
} arp_entry_t;

static arp_entry_t arp_cache[16];
static int arp_cache_count = 0;

void
mkrn_arp_init(void)
{
    mkrn_memset(arp_cache, 0, sizeof(arp_cache));
    arp_cache_count = 0;
}

void
mkrn_arp_add_entry(uint32_t u32Ip,
                   const uint8_t *pMac)
{
    if (arp_cache_count >= 16)
        return;

    arp_entry_t *pEntry =
        &arp_cache[arp_cache_count++];
    pEntry->ip = u32Ip;
    mkrn_memcpy(pEntry->mac, pMac, 6);
    pEntry->valid = 1;
}

int
mkrn_arp_lookup(uint32_t u32Ip, uint8_t *pMac)
{
    for (int i = 0; i < arp_cache_count; i++) {
        if (arp_cache[i].valid
            && arp_cache[i].ip == u32Ip)
        {
            mkrn_memcpy(pMac, arp_cache[i].mac, 6);
            return 0;
        }
    }
    return -1;
}

int
mkrn_arp_request(uint32_t u32TargetIp)
{
    (void)u32TargetIp;
    return 0;
}

void
mkrn_arp_process_reply(const uint8_t *pPacket,
                       size_t len)
{
    if (len < sizeof(arp_header_t))
        return;

    const arp_header_t *pArp =
        (const arp_header_t *)pPacket;
    mkrn_arp_add_entry(pArp->sender_ip,
                       pArp->sender_mac);
}
