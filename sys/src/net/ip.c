/*
 * M4KK1 4P1 - ip.c
 * Description: Basic IP protocol implementation.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <string.h>
#include "net.h"

typedef struct {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_offset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t source_ip;
    uint32_t dest_ip;
} __attribute__((packed)) ip_header_t;

static uint32_t u32LocalIp = 0;
static uint32_t u32GatewayIp = 0;
static uint32_t u32SubnetMask = 0;

void
mkrn_ip_init(void)
{
    u32LocalIp = 0;
    u32GatewayIp = 0;
    u32SubnetMask = 0;
}

void
mkrn_ip_set_config(uint32_t u32Ip,
                   uint32_t u32Gateway,
                   uint32_t u32Mask)
{
    u32LocalIp = u32Ip;
    u32GatewayIp = u32Gateway;
    u32SubnetMask = u32Mask;
}

uint32_t
mkrn_ip_get_local(void)
{
    return u32LocalIp;
}

int
mkrn_ip_send(uint32_t u32DestIp, uint8_t u8Protocol,
             const void *pData, size_t len)
{
    (void)u32DestIp;
    (void)u8Protocol;
    (void)pData;
    return (int)len;
}

int
mkrn_ip_receive(uint32_t *pSourceIp,
                uint8_t *pProtocol, void *pBuffer,
                size_t len)
{
    (void)pSourceIp;
    (void)pProtocol;
    (void)pBuffer;
    (void)len;
    return 0;
}

uint16_t
mkrn_ip_checksum(const void *pData, size_t len)
{
    const uint16_t *pWords =
        (const uint16_t *)pData;
    uint32_t u32Sum = 0;

    for (size_t i = 0; i < len / 2; i++)
        u32Sum += pWords[i];

    if (len % 2)
        u32Sum += ((const uint8_t *)pData)[len - 1];

    while (u32Sum >> 16)
        u32Sum = (u32Sum & 0xFFFF) + (u32Sum >> 16);

    return (uint16_t)~u32Sum;
}
