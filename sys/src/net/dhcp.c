/*
 * M4KK1 4P1 - dhcp.c
 * Description: Basic DHCP client implementation.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <string.h>
#include "net.h"

typedef struct {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t cookie;
} __attribute__((packed)) dhcp_header_t;

typedef enum {
    DHCP_INIT,
    DHCP_SELECTING,
    DHCP_REQUESTING,
    DHCP_BOUND,
    DHCP_RENEWING,
    DHCP_REBINDING
} dhcp_state_t;

static dhcp_state_t eDhcpState = DHCP_INIT;
static uint32_t u32DhcpServerIp = 0;
static uint32_t u32OfferedIp = 0;
static uint32_t u32LeaseTime = 0;

void
mkrn_dhcp_init(void)
{
    eDhcpState = DHCP_INIT;
    u32DhcpServerIp = 0;
    u32OfferedIp = 0;
    u32LeaseTime = 0;
}

int
mkrn_dhcp_discover(void)
{
    eDhcpState = DHCP_SELECTING;
    return 0;
}

int
mkrn_dhcp_request(uint32_t u32RequestedIp)
{
    (void)u32RequestedIp;
    eDhcpState = DHCP_REQUESTING;
    return 0;
}

void
mkrn_dhcp_process_offer(uint32_t u32ServerIp,
                        uint32_t u32OfferedIpAddr,
                        uint32_t u32Lease)
{
    u32DhcpServerIp = u32ServerIp;
    u32OfferedIp = u32OfferedIpAddr;
    u32LeaseTime = u32Lease;
    mkrn_dhcp_request(u32OfferedIp);
}

void
mkrn_dhcp_process_ack(uint32_t u32AssignedIp,
                      uint32_t u32Lease)
{
    (void)u32AssignedIp;
    eDhcpState = DHCP_BOUND;
    u32LeaseTime = u32Lease;
}

uint32_t
mkrn_dhcp_get_ip(void)
{
    if (eDhcpState == DHCP_BOUND)
        return u32OfferedIp;
    return 0;
}
