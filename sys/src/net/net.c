/*
 * M4KK1 4P1 - net.c
 * Description: Network protocol stack core — device
 *              registration, IP send/receive, checksum.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/net.h"
#include "../../include/console.h"
#include "../../include/memory.h"
#include "../../include/string.h"
#include <stdint.h>
#include <stdbool.h>

static net_device_t *pNetDevices = NULL;
static int net_device_count = 0;
static net_protocol_handler_t
    protocol_handlers[256];

static struct {
    uint64_t packets_received;
    uint64_t packets_sent;
    uint64_t bytes_received;
    uint64_t bytes_sent;
    uint64_t packets_dropped;
} net_stats;

void
mkrn_net_get_mac_string(uint8_t *pMac, char *pBuffer)
{
    snprintf(pBuffer, 18,
            "%02x:%02x:%02x:%02x:%02x:%02x",
            pMac[0], pMac[1], pMac[2], pMac[3],
            pMac[4], pMac[5]);
}

uint32_t
mkrn_net_string_to_ip(const char *pString)
{
    uint32_t u32Ip = 0;
    uint32_t u32Octet = 0;
    int dots = 0;

    while (*pString) {
        if (*pString >= '0' && *pString <= '9')
            u32Octet =
                u32Octet * 10 + (*pString - '0');
        else if (*pString == '.') {
            u32Ip = (u32Ip << 8) | u32Octet;
            u32Octet = 0;
            dots++;
        } else
            return 0;
        pString++;
    }

    if (dots == 3) {
        u32Ip = (u32Ip << 8) | u32Octet;
        return u32Ip;
    }
    return 0;
}

uint32_t
mkrn_net_ip_to_string(uint32_t u32Ip, char *pBuffer)
{
    return (uint32_t)snprintf(
        pBuffer, 16, "%d.%d.%d.%d",
        (u32Ip >> 24) & 0xFF, (u32Ip >> 16) & 0xFF,
        (u32Ip >> 8) & 0xFF, u32Ip & 0xFF);
}

uint16_t
mkrn_net_checksum(uint16_t *pData, uint16_t u16Len)
{
    uint32_t u32Sum = 0;

    while (u16Len > 1) {
        u32Sum += *pData++;
        u16Len -= 2;
    }

    if (u16Len > 0)
        u32Sum += *(uint8_t *)pData;

    while (u32Sum >> 16)
        u32Sum = (u32Sum & 0xFFFF) + (u32Sum >> 16);

    return (uint16_t)~u32Sum;
}

int
mkrn_net_init(void)
{
    mkrn_console_write(
        "Initializing network protocol stack...\n");

    pNetDevices = NULL;
    net_device_count = 0;
    mkrn_memset(protocol_handlers, 0,
           sizeof(protocol_handlers));
    mkrn_memset(&net_stats, 0, sizeof(net_stats));

    mkrn_console_write(
        "Network protocol stack initialized\n");
    return 0;
}

int
mkrn_net_device_register(net_device_t *pDev)
{
    if (!pDev || !pDev->name[0])
        return -1;

    net_device_t *pDevice = pNetDevices;
    while (pDevice) {
        if (mkrn_strcmp(pDevice->name, pDev->name) == 0)
            return -1;
        pDevice = (net_device_t *)pDevice->priv;
    }

    pDev->priv = pNetDevices;
    pNetDevices = pDev;
    net_device_count++;

    mkrn_console_write(
        "Network device registered: ");
    mkrn_console_write(pDev->name);
    mkrn_console_write("\n");

    if (pDev->init)
        pDev->init(pDev);

    return 0;
}

int
mkrn_net_device_unregister(const char *pName)
{
    if (!pName)
        return -1;

    net_device_t *pDevice = pNetDevices;
    net_device_t *pPrev = NULL;

    while (pDevice) {
        if (mkrn_strcmp(pDevice->name, pName) == 0) {
            if (pPrev)
                pPrev->priv = pDevice->priv;
            else
                pNetDevices = pDevice->priv;
            net_device_count--;
            return 0;
        }
        pPrev = pDevice;
        pDevice = pDevice->priv;
    }
    return -1;
}

net_device_t *
mkrn_net_device_find(const char *pName)
{
    if (!pName)
        return NULL;

    net_device_t *pDevice = pNetDevices;
    while (pDevice) {
        if (mkrn_strcmp(pDevice->name, pName) == 0)
            return pDevice;
        pDevice = pDevice->priv;
    }
    return NULL;
}

int
mkrn_net_protocol_register(
    uint8_t u8Protocol,
    net_protocol_handler_t handler)
{
    if (u8Protocol >= 256 || !handler)
        return -1;

    protocol_handlers[u8Protocol] = handler;

    mkrn_console_write(
        "Protocol handler registered for "
        "protocol 0x");
    mkrn_console_write_hex(u8Protocol);
    mkrn_console_write("\n");

    return 0;
}

int
mkrn_net_protocol_unregister(uint8_t u8Protocol)
{
    if (u8Protocol >= 256)
        return -1;

    protocol_handlers[u8Protocol] = NULL;
    return 0;
}

int
mkrn_net_send_packet(uint32_t u32DstIp,
                     uint8_t u8Protocol,
                     uint8_t *pData, uint16_t u16Len)
{
    if (!pData || u16Len == 0)
        return -1;

    ip_header_t ip_header;
    mkrn_memset(&ip_header, 0, sizeof(ip_header));

    ip_header.ver_ihl = 0x45;
    ip_header.tos = 0;
    ip_header.total_len =
        sizeof(ip_header) + u16Len;
    ip_header.id = 0;
    ip_header.frag_offset = 0;
    ip_header.ttl = 64;
    ip_header.protocol = u8Protocol;
    ip_header.src_ip = 0x0A000001;
    ip_header.dst_ip = u32DstIp;

    ip_header.checksum = mkrn_net_checksum(
        (uint16_t *)&ip_header,
        sizeof(ip_header));

    net_device_t *pDev = pNetDevices;
    if (pDev && pDev->transmit) {
        uint8_t packet[sizeof(ip_header) + u16Len];
        mkrn_memcpy(packet, &ip_header,
               sizeof(ip_header));
        mkrn_memcpy(packet + sizeof(ip_header), pData,
               u16Len);

        pDev->transmit(pDev, packet,
                       sizeof(packet));
        net_stats.packets_sent++;
        net_stats.bytes_sent += sizeof(packet);

        return (int)sizeof(packet);
    }

    return -1;
}

int
mkrn_net_send_ethernet(uint8_t *pDstMac,
                       uint16_t u16Type,
                       uint8_t *pData, uint16_t u16Len)
{
    if (!pDstMac || !pData || u16Len == 0)
        return -1;

    eth_header_t eth_header;
    mkrn_memcpy(eth_header.dst_mac, pDstMac, 6);
    mkrn_memset(eth_header.src_mac, 0, 6);
    eth_header.type = u16Type;

    net_device_t *pDev = pNetDevices;
    if (pDev && pDev->transmit) {
        uint8_t packet[
            sizeof(eth_header) + u16Len];
        mkrn_memcpy(packet, &eth_header,
               sizeof(eth_header));
        mkrn_memcpy(packet + sizeof(eth_header), pData,
               u16Len);

        pDev->transmit(pDev, packet,
                       sizeof(packet));
        net_stats.packets_sent++;
        net_stats.bytes_sent += sizeof(packet);

        return (int)sizeof(packet);
    }

    return -1;
}

void
mkrn_net_poll(void)
{
    net_device_t *pDev = pNetDevices;

    while (pDev) {
        if (pDev->poll)
            pDev->poll(pDev);
        pDev = pDev->priv;
    }
}

void
mkrn_net_receive_packet(net_device_t *pDev,
                        uint8_t *pPacket,
                        uint16_t u16Len)
{
    if (!pDev || !pPacket || u16Len == 0)
        return;

    net_stats.packets_received++;
    net_stats.bytes_received += u16Len;

    if (u16Len < sizeof(eth_header_t))
        return;

    eth_header_t *pEthHeader =
        (eth_header_t *)pPacket;

    if (pEthHeader->type == ETH_TYPE_IP) {
        if (u16Len < sizeof(eth_header_t)
                         + sizeof(ip_header_t))
            return;

        ip_header_t *pIpHeader =
            (ip_header_t *)(pPacket
                            + sizeof(eth_header_t));

        if ((pIpHeader->ver_ihl >> 4) != 4)
            return;

        uint16_t u16SavedChecksum =
            pIpHeader->checksum;
        pIpHeader->checksum = 0;
        uint16_t u16CalcChecksum =
            mkrn_net_checksum(
                (uint16_t *)pIpHeader,
                sizeof(ip_header_t));

        if (u16SavedChecksum != u16CalcChecksum) {
            mkrn_console_write(
                "IP checksum mismatch\n");
            return;
        }

        pIpHeader->checksum = u16SavedChecksum;

        if (pIpHeader->protocol < 256
            && protocol_handlers
                   [pIpHeader->protocol])
        {
            uint8_t *pData =
                pPacket + sizeof(eth_header_t)
                + sizeof(ip_header_t);
            uint16_t u16DataLen =
                u16Len - sizeof(eth_header_t)
                - sizeof(ip_header_t);

            protocol_handlers[pIpHeader->protocol](
                pData, u16DataLen,
                pIpHeader->src_ip,
                pIpHeader->dst_ip);
        }
    }
}

void
mkrn_net_print_packet(uint8_t *pPacket,
                      uint16_t u16Len)
{
    mkrn_console_write("Packet dump (");
    mkrn_console_write_dec(u16Len);
    mkrn_console_write(" bytes):\n");

    for (int i = 0; i < (int)u16Len; i += 16) {
        mkrn_console_write("  ");
        mkrn_console_write_hex(i);
        mkrn_console_write(": ");

        for (int j = 0; j < 16; j++) {
            if (i + j < (int)u16Len) {
                mkrn_console_write_hex(
                    pPacket[i + j]);
                mkrn_console_write(" ");
            } else
                mkrn_console_write("   ");
        }

        mkrn_console_write(" ");
        for (int j = 0; j < 16 && i + j < (int)u16Len;
             j++)
        {
            uint8_t u8C = pPacket[i + j];
            if (u8C >= 32 && u8C <= 126)
                mkrn_console_put_char((char)u8C);
            else
                mkrn_console_put_char('.');
        }

        mkrn_console_write("\n");
    }
}

void
mkrn_net_get_stats(uint64_t *pPacketsReceived,
                   uint64_t *pPacketsSent,
                   uint64_t *pBytesReceived,
                   uint64_t *pBytesSent)
{
    if (pPacketsReceived)
        *pPacketsReceived =
            net_stats.packets_received;
    if (pPacketsSent)
        *pPacketsSent = net_stats.packets_sent;
    if (pBytesReceived)
        *pBytesReceived =
            net_stats.bytes_received;
    if (pBytesSent)
        *pBytesSent = net_stats.bytes_sent;
}

void
mkrn_net_print_stats(void)
{
    mkrn_console_write(
        "=== Network Statistics ===\n");
    mkrn_console_write("Packets received: ");
    mkrn_console_write_dec(
        net_stats.packets_received);
    mkrn_console_write("\n");
    mkrn_console_write("Packets sent: ");
    mkrn_console_write_dec(net_stats.packets_sent);
    mkrn_console_write("\n");
    mkrn_console_write("Bytes received: ");
    mkrn_console_write_dec(
        net_stats.bytes_received);
    mkrn_console_write("\n");
    mkrn_console_write("Bytes sent: ");
    mkrn_console_write_dec(net_stats.bytes_sent);
    mkrn_console_write("\n");
    mkrn_console_write("Packets dropped: ");
    mkrn_console_write_dec(
        net_stats.packets_dropped);
    mkrn_console_write("\n");
    mkrn_console_write(
        "===========================\n");
}
