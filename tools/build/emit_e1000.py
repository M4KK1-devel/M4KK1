#!/usr/bin/env python3
"""Emit the M4KK1 e1000 driver (sys/src/net/e1000.c) atomically."""
import os

BODY = r'''/*
 * M4KK1 4P1 - e1000.c
 * Description: Intel e1000 NIC driver (QEMU e1000 model, i440fx).
 *
 * MMIO BAR0 registers, legacy TX/RX descriptor rings, MAC from the
 * Receive Address registers (RAL0/RAH0), RX via IRQ (PIC line) with a
 * timer-tick polling fallback.  Physical addresses equal virtual
 * addresses in this flat, non-paged kernel.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include "kernel.h"
#include "pci.h"
#include "idt.h"
#include "memory.h"
#include "net.h"

/* e1000 MMIO register offsets (subset used by this driver) */
#define E1000_CTRL      0x0000
#define E1000_STATUS    0x0008
#define E1000_ICR       0x00C0
#define E1000_IMS       0x00D0
#define E1000_IMC       0x00D8
#define E1000_RCTL      0x0100
#define E1000_TCTL      0x0400
#define E1000_RDBAL     0x02800
#define E1000_RDLEN     0x02808
#define E1000_RDH       0x02810
#define E1000_RDT       0x02818
#define E1000_TDBAL     0x03800
#define E1000_TDLEN     0x03808
#define E1000_TDH       0x03810
#define E1000_TDT       0x03818
#define E1000_RA        0x05400

/* CTRL bits */
#define E1000_CTRL_RST  (1U << 26)
#define E1000_CTRL_SLU  (1U << 6)
#define E1000_CTRL_ASDE (1U << 5)

/* RCTL bits */
#define E1000_RCTL_EN   (1U << 1)
#define E1000_RCTL_MPE  (1U << 4)
#define E1000_RCTL_LPE  (1U << 5)
#define E1000_RCTL_BAM  (1U << 15)
#define E1000_RCTL_SECRC (1U << 26)

/* TCTL bits */
#define E1000_TCTL_EN   (1U << 1)
#define E1000_TCTL_PSP  (1U << 3)

/* Interrupt cause bits */
#define E1000_ICS_RXT0  (1U << 7)

#define E1000_NUM_RX    16
#define E1000_NUM_TX    16
#define E1000_BUF_SIZE  1600

/* Legacy descriptors */
typedef struct {
    uint64_t addr;
    uint16_t length;
    uint16_t csum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  sta;
    uint8_t  special;
} __attribute__((packed)) e1000_tx_desc_t;

/* Device state */
static uint32_t e1000_base = 0;
static mkrn_pci_device_t e1000_pci;
static uint8_t  e1000_irq_line = 11;
static uint8_t  e1000_mac[6];
static int      e1000_ready = 0;

/* Descriptor rings + buffers (flat kernel: phys == virt) */
static e1000_rx_desc_t rx_desc[E1000_NUM_RX] __attribute__((aligned(16)));
static e1000_tx_desc_t tx_desc[E1000_NUM_TX] __attribute__((aligned(16)));
static uint8_t rx_buf[E1000_NUM_RX][E1000_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buf[E1000_NUM_TX][E1000_BUF_SIZE] __attribute__((aligned(16)));

static volatile uint32_t rx_tail = 0;
static volatile uint32_t tx_tail = 0;

/* MMIO accessors */
static uint32_t e1000_read(uint32_t reg)
{
    return *(volatile uint32_t *)(e1000_base + reg);
}

static void e1000_write(uint32_t reg, uint32_t val)
{
    *(volatile uint32_t *)(e1000_base + reg) = val;
}

/* Walk done RX descriptors, hand frames to the stack */
static void e1000_rx_poll(void)
{
    int guard = 0;
    while (guard++ < E1000_NUM_RX) {
        e1000_rx_desc_t *d = &rx_desc[rx_tail];
        if (!(d->status & 0x01))
            break;
        uint16_t len = d->length;
        if (len > E1000_BUF_SIZE)
            len = E1000_BUF_SIZE;
        mkrn_net_receive_frame(rx_buf[rx_tail], len);
        d->status = 0;
        rx_tail = (rx_tail + 1) % E1000_NUM_RX;
        e1000_write(E1000_RDT, rx_tail);
    }
}

int mkrn_e1000_transmit(const uint8_t *data, uint32_t len)
{
    if (!e1000_ready || !data || len == 0 || len >= E1000_BUF_SIZE)
        return -1;

    e1000_tx_desc_t *d = &tx_desc[tx_tail];
    if (d->sta & 0x01) {
        d->sta = 0;
    } else {
        int spin = 0;
        while (!(d->sta & 0x01) && spin++ < 2000000)
            ;
        if (!(d->sta & 0x01))
            return -1;
    }
    for (uint32_t i = 0; i < len; i++)
        tx_buf[tx_tail][i] = data[i];
    d->addr = (uint32_t)(uintptr_t)tx_buf[tx_tail];
    d->length = (uint16_t)len;
    d->cmd = 0x0B;   /* EOP | IFCS | RS */
    d->sta = 0;
    __asm__ volatile("" ::: "memory");
    tx_tail = (tx_tail + 1) % E1000_NUM_TX;
    e1000_write(E1000_TDT, tx_tail);
    return (int)len;
}

void mkrn_e1000_poll(void)
{
    if (e1000_ready)
        e1000_rx_poll();
}

static void e1000_isr(void)
{
    if (!e1000_ready)
        return;
    (void)e1000_read(E1000_ICR);   /* clear-on-read */
    e1000_rx_poll();
}

const uint8_t *mkrn_e1000_get_mac(void)
{
    return e1000_mac;
}

int mkrn_e1000_init(void)
{
    if (mkrn_pci_find_device(0x8086, 0x100E, &e1000_pci) != 0)
        return -1;

    e1000_base = e1000_pci.bar[0] & ~0xFU;
    if (e1000_base == 0 || (e1000_base & 1U))
        return -1;
    e1000_irq_line = e1000_pci.irq_line ? e1000_pci.irq_line : 11;

    mkrn_pci_enable_bus_master(&e1000_pci);

    /* Reset and bring the link up */
    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_RST);
    for (int i = 0; i < 100000; i++) {
        if (!(e1000_read(E1000_CTRL) & E1000_CTRL_RST))
            break;
    }
    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL)
                | E1000_CTRL_SLU | E1000_CTRL_ASDE);

    /* Interrupts masked: RX is driven by timer polling for now */
    e1000_write(E1000_IMC, 0xFFFFFFFF);

    /* MAC from RAL0/RAH0 */
    {
        uint32_t ral = e1000_read(E1000_RA);
        uint32_t rah = e1000_read(E1000_RA + 4);
        e1000_mac[0] = (uint8_t)(ral);
        e1000_mac[1] = (uint8_t)(ral >> 8);
        e1000_mac[2] = (uint8_t)(ral >> 16);
        e1000_mac[3] = (uint8_t)(ral >> 24);
        e1000_mac[4] = (uint8_t)(rah);
        e1000_mac[5] = (uint8_t)(rah >> 8);
    }

    /* RX ring */
    for (int i = 0; i < E1000_NUM_RX; i++) {
        rx_desc[i].addr = (uint32_t)(uintptr_t)rx_buf[i];
        rx_desc[i].status = 0;
    }
    e1000_write(E1000_RDBAL, (uint32_t)(uintptr_t)rx_desc);
    e1000_write(E1000_RDLEN, E1000_NUM_RX * sizeof(e1000_rx_desc_t));
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_NUM_RX - 1);
    e1000_write(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM
                | E1000_RCTL_SECRC | E1000_RCTL_MPE);

    /* TX ring */
    for (int i = 0; i < E1000_NUM_TX; i++) {
        tx_desc[i].addr = (uint32_t)(uintptr_t)tx_buf[i];
        tx_desc[i].sta = 0x01;   /* pre-mark done so the first xmit passes */
        tx_desc[i].cmd = 0;
        tx_desc[i].length = 0;
    }
    e1000_write(E1000_TDBAL, (uint32_t)(uintptr_t)tx_desc);
    e1000_write(E1000_TDLEN, E1000_NUM_TX * sizeof(e1000_tx_desc_t));
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);
    e1000_write(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP);

    e1000_ready = 1;

    /* IRQ route: vector 0x20+line, PIC unmask.  The ISR keeps working
     * even if the line is shared; polling covers RX regardless. */
    mkrn_idt_register_handler(0x20 + e1000_irq_line,
                              (mkrn_int_handler_t)e1000_isr);
    {
        extern void pic_unmask_irq(uint32_t);
        pic_unmask_irq(e1000_irq_line);
    }

    mkrn_console_write("   e1000 NIC up, MAC ");
    for (int i = 0; e1000_ready && i < 6; i++) {
        mkrn_console_write_hex(e1000_mac[i]);
        if (i < 5)
            mkrn_console_write(":");
    }
    mkrn_console_write("\n");
    return 0;
}
'''

out = r"F:\M4KK1\sys\src\net\e1000.c"
with open(out, "w", newline="\n") as f:
    f.write(BODY)
print("wrote", out, os.path.getsize(out), "bytes")
