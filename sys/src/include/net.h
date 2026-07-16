/*
 * M4KK1 4P1 - net.h
 * Description: Network protocol stack interface declarations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Network device types */
#define M4K_NET_DEV_ETHERNET    1
#define M4K_NET_DEV_WIFI        2
#define M4K_NET_DEV_LOOPBACK    3

/* IP protocol types */
#define M4K_IP_PROTOCOL_ICMP    1
#define M4K_IP_PROTOCOL_TCP     6
#define M4K_IP_PROTOCOL_UDP     17

/* Ethernet protocol types */
#define M4K_ETH_TYPE_IP         0x0800
#define M4K_ETH_TYPE_ARP        0x0806
#define M4K_ETH_TYPE_IPV6       0x86DD

typedef struct {
    u8 dst_mac[6];
    u8 src_mac[6];
    u16 type;
} __attribute__((packed)) mkrn_eth_header_t;

typedef struct {
    u8 ver_ihl;
    u8 tos;
    u16 total_len;
    u16 id;
    u16 frag_offset;
    u8 ttl;
    u8 protocol;
    u16 checksum;
    u32 src_ip;
    u32 dst_ip;
} __attribute__((packed)) mkrn_ip_header_t;

typedef struct {
    u16 src_port;
    u16 dst_port;
    u16 length;
    u16 checksum;
} __attribute__((packed)) mkrn_udp_header_t;

typedef struct {
    u8 type;
    u8 code;
    u16 checksum;
    u32 data;
} __attribute__((packed)) mkrn_icmp_header_t;

typedef struct mkrn_net_device {
    char name[16];
    u32 type;
    u8 mac_addr[6];
    u32 ip_addr;
    u32 netmask;
    u32 gateway;
    b up;
    int (*init)(struct mkrn_net_device *dev);
    int (*transmit)(struct mkrn_net_device *dev, u8 *data, u32 len);
    int (*receive)(struct mkrn_net_device *dev, u8 *buffer, u32 len);
    void (*poll)(struct mkrn_net_device *dev);
    void *priv;
} mkrn_net_device_t;

typedef void (*mkrn_net_protocol_handler_t)(u8 *packet, u16 len, u32 src_ip, u32 dst_ip);

/**
 * mkrn_net_init - Initialize the network stack
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_net_init(void);

/**
 * mkrn_net_device_register - Register a network device
 * @dev: Device to register
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_net_device_register(mkrn_net_device_t *dev);

/**
 * mkrn_net_device_unregister - Unregister a network device
 * @name: Device name
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_net_device_unregister(const char *name);

/**
 * mkrn_net_device_find - Find a network device by name
 * @name: Device name
 *
 * Return: Device pointer, NULL if not found
 */
mkrn_net_device_t *mkrn_net_device_find(const char *name);

/**
 * mkrn_net_protocol_register - Register a protocol handler
 * @protocol: Protocol number
 * @handler: Handler function
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_net_protocol_register(u8 protocol, mkrn_net_protocol_handler_t handler);

/**
 * mkrn_net_protocol_unregister - Unregister a protocol handler
 * @protocol: Protocol number
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_net_protocol_unregister(u8 protocol);

/**
 * mkrn_net_send_packet - Send an IP packet
 * @dst_ip: Destination IP address
 * @protocol: Protocol number
 * @data: Packet data
 * @len: Data length
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_net_send_packet(u32 dst_ip, u8 protocol, u8 *data, u16 len);

/**
 * mkrn_net_send_ethernet - Send an Ethernet frame
 * @dst_mac: Destination MAC address
 * @type: Ethernet type
 * @data: Frame data
 * @len: Data length
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_net_send_ethernet(u8 *dst_mac, u16 type, u8 *data, u16 len);

/**
 * mkrn_net_poll - Poll all network devices
 *
 * Return: void
 */
void mkrn_net_poll(void);

/* TCP functions */
void mkrn_tcp_init(void);
int mkrn_tcp_listen(u32 local_ip, u16 local_port);
int mkrn_tcp_connect(u32 local_ip, u16 local_port, u32 remote_ip, u16 remote_port);
int mkrn_tcp_send(tcp_pcb_t *pcb, u8 *data, u16 len);
int mkrn_tcp_close(tcp_pcb_t *pcb);
void mkrn_tcp_handle_packet(u8 *packet, u16 len, u32 src_ip, u32 dst_ip);

/* UDP functions */
int mkrn_udp_send(u32 src_ip, u16 src_port, u32 dst_ip, u16 dst_port, u8 *data, u16 len);
void mkrn_udp_handle_packet(u8 *packet, u16 len, u32 src_ip, u32 dst_ip);

/* ICMP functions */
void mkrn_icmp_handle_packet(u8 *packet, u16 len, u32 src_ip, u32 dst_ip);
int mkrn_icmp_send_echo_request(u32 dst_ip, u16 id, u16 seq);
int mkrn_icmp_send_echo_reply(u32 dst_ip, u16 id, u16 seq);

/* ARP functions */
void mkrn_arp_handle_packet(u8 *packet, u16 len);
int mkrn_arp_resolve(u32 ip_addr, u8 *mac_addr);
int mkrn_arp_send_request(u32 target_ip);
int mkrn_arp_send_reply(u32 dst_ip, u8 *dst_mac);

/* Network utility functions */
u16 mkrn_net_checksum(u16 *data, u16 len);
u32 mkrn_net_ip_to_string(u32 ip, char *buffer);
u32 mkrn_net_string_to_ip(const char *string);
void mkrn_net_print_packet(u8 *packet, u16 len);
