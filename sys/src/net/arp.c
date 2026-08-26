/*
 * M4KK1 4P1 - arp.c
 * Description: Address Resolution Protocol (IPv4 over Ethernet).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include "kernel.h"
#include "net.h"
#include "net_impl.h"

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
} __attribute__((packed)) arp_packet_t;

#define ARP_HTYPE_ETH  0x0001
#define ARP_OP_REQUEST 0x0001
#define ARP_OP_REPLY   0x0002

typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    uint8_t  valid;
} arp_entry_t;

#define ARP_CACHE_SIZE 16
static arp_entry_t arp_cache[ARP_CACHE_SIZE];
static int arp_cache_count = 0;

void mkrn_arp_init(void)
{
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
        arp_cache[i].valid = 0;
    arp_cache_count = 0;
}

static void arp_cache_update(uint32_t ip, const uint8_t *mac)
{
    /* refresh existing entry */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            for (int k = 0; k < 6; k++)
                arp_cache[i].mac[k] = mac[k];
            return;
        }
    }
    /* insert (round-robin when full) */
    int slot = arp_cache_count;
    if (slot >= ARP_CACHE_SIZE)
        slot = 0;
    else
        arp_cache_count++;
    arp_cache[slot].ip = ip;
    for (int k = 0; k < 6; k++)
        arp_cache[slot].mac[k] = mac[k];
    arp_cache[slot].valid = 1;
}

int mkrn_arp_resolve(uint32_t ip, uint8_t *mac)
{
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            for (int k = 0; k < 6; k++)
                mac[k] = arp_cache[i].mac[k];
            return 0;
        }
    }
    return -1;
}

int mkrn_arp_send_request(uint32_t target_ip)
{
    arp_packet_t req;
    mkrn_net_device_t *dev = mkrn_net_device_find("eth0");
    if (!dev || !dev->up)
        return -1;
    req.hardware_type = htons(ARP_HTYPE_ETH);
    req.protocol_type = htons(M4K_ETH_TYPE_IP);
    req.hardware_len = 6;
    req.protocol_len = 4;
    req.operation = htons(ARP_OP_REQUEST);
    for (int i = 0; i < 6; i++) {
        req.sender_mac[i] = dev->mac_addr[i];
        req.target_mac[i] = 0;
    }
    req.sender_ip = htonl(mkrn_net_get_ip());
    req.target_ip = htonl(target_ip);
    {
        uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        return mkrn_net_send_ethernet(bcast, M4K_ETH_TYPE_ARP,
                                      (uint8_t *)&req, sizeof(req));
    }
}

static int arp_send_reply(uint32_t dst_ip, const uint8_t *dst_mac)
{
    arp_packet_t rep;
    mkrn_net_device_t *dev = mkrn_net_device_find("eth0");
    if (!dev || !dev->up)
        return -1;
    rep.hardware_type = htons(ARP_HTYPE_ETH);
    rep.protocol_type = htons(M4K_ETH_TYPE_IP);
    rep.hardware_len = 6;
    rep.protocol_len = 4;
    rep.operation = htons(ARP_OP_REPLY);
    for (int i = 0; i < 6; i++) {
        rep.sender_mac[i] = dev->mac_addr[i];
        rep.target_mac[i] = dst_mac[i];
    }
    rep.sender_ip = htonl(mkrn_net_get_ip());
    rep.target_ip = htonl(dst_ip);
    return mkrn_net_send_ethernet((uint8_t *)dst_mac, M4K_ETH_TYPE_ARP,
                                  (uint8_t *)&rep, sizeof(rep));
}

void mkrn_arp_handle_packet(uint8_t *packet, uint16_t len)
{
    if (len < sizeof(arp_packet_t))
        return;
    arp_packet_t *arp = (arp_packet_t *)packet;
    if (ntohs(arp->hardware_type) != ARP_HTYPE_ETH)
        return;
    if (ntohs(arp->protocol_type) != M4K_ETH_TYPE_IP)
        return;
    if (arp->hardware_len != 6 || arp->protocol_len != 4)
        return;

    arp_cache_update(ntohl(arp->sender_ip), arp->sender_mac);

    uint16_t op = ntohs(arp->operation);
    if (op == ARP_OP_REQUEST
        && ntohl(arp->target_ip) == mkrn_net_get_ip())
        arp_send_reply(ntohl(arp->sender_ip), arp->sender_mac);
}
