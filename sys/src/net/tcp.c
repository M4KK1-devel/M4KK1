/*
 * M4KK1 4P1 - tcp.c
 * Description: Basic TCP protocol implementation.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <string.h>
#include "net.h"

typedef enum {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT
} tcp_state_t;

typedef struct {
    uint16_t source_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

typedef struct {
    tcp_state_t state;
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t seq_num;
    uint32_t ack_num;
} tcp_connection_t;

static tcp_connection_t tcp_connections[16];
static int tcp_connection_count = 0;

void
mkrn_tcp_init(void)
{
    mkrn_memset(tcp_connections, 0,
           sizeof(tcp_connections));
    tcp_connection_count = 0;
}

int
mkrn_tcp_connect(uint32_t u32RemoteIp,
                 uint16_t u16RemotePort)
{
    if (tcp_connection_count >= 16)
        return -1;

    tcp_connection_t *pConn =
        &tcp_connections[tcp_connection_count++];
    pConn->state = TCP_CLOSED;
    pConn->remote_ip = u32RemoteIp;
    pConn->remote_port = u16RemotePort;
    pConn->local_port =
        (uint16_t)(1024 + tcp_connection_count);
    pConn->seq_num = 0;
    pConn->ack_num = 0;
    pConn->state = TCP_SYN_SENT;

    return tcp_connection_count - 1;
}

int
mkrn_tcp_send(int conn_id, const void *pData,
              size_t len)
{
    if (conn_id < 0
        || conn_id >= tcp_connection_count)
        return -1;

    tcp_connection_t *pConn =
        &tcp_connections[conn_id];
    if (pConn->state != TCP_ESTABLISHED)
        return -1;

    (void)pData;
    return (int)len;
}

int
mkrn_tcp_receive(int conn_id, void *pBuffer,
                 size_t len)
{
    if (conn_id < 0
        || conn_id >= tcp_connection_count)
        return -1;

    tcp_connection_t *pConn =
        &tcp_connections[conn_id];
    if (pConn->state != TCP_ESTABLISHED)
        return -1;

    (void)pBuffer;
    (void)len;
    return 0;
}

int
mkrn_tcp_close(int conn_id)
{
    if (conn_id < 0
        || conn_id >= tcp_connection_count)
        return -1;

    tcp_connection_t *pConn =
        &tcp_connections[conn_id];
    pConn->state = TCP_FIN_WAIT_1;
    return 0;
}
