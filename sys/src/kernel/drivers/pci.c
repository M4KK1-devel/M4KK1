/*
 * M4KK1 4P1 - pci.c
 * Description: PCI bus enumeration and configuration
 *              space access via ports 0xCF8/0xCFC.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "pci.h"
#include <console.h>
#include <kernel.h>

#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC
#define PCI_CONFIG_ENABLE   0x80000000

#define PCI_VENDOR_NONE     0xFFFF

static mkrn_pci_device_t pci_devices[M4K_PCI_MAX_DEVICES];
static int pci_device_count = 0;
static bool pci_scanned = false;

/* ── I/O port helpers ── */

static inline void
outl(uint16_t u16Port, uint32_t u32Value)
{
    __asm__ volatile("outl %0, %1"
                     :
                     : "a"(u32Value), "Nd"(u16Port));
}

static inline uint32_t
inl(uint16_t u16Port)
{
    uint32_t u32Value;
    __asm__ volatile("inl %1, %0"
                     : "=a"(u32Value)
                     : "Nd"(u16Port));
    return u32Value;
}

/* ── Known QEMU/standard device names ── */

typedef struct {
    uint16_t    vendor_id;
    uint16_t    device_id;
    const char *name;
} pci_known_id_t;

static const pci_known_id_t pci_known_ids[] = {
    { 0x8086, 0x1237, "i440FX Host Bridge" },
    { 0x8086, 0x7000, "PIIX3 ISA Bridge" },
    { 0x8086, 0x7010, "IDE controller" },
    { 0x8086, 0x7112, "USB controller (UHCI)" },
    { 0x8086, 0x7113, "PIIX3 ACPI/Power Management" },
    { 0x1234, 0x1111, "QEMU VGA Display" },
    { 0x10EC, 0x8139, "RTL8139 Ethernet" },
    { 0x8086, 0x100E, "Intel 82540EM Ethernet (e1000)" },
    { 0x8086, 0x2415, "ICH AC'97 Audio" },
    { 0x1013, 0x00B8, "Cirrus Logic GD5446 VGA" },
};

#define PCI_KNOWN_ID_COUNT \
    (int)(sizeof(pci_known_ids) / sizeof(pci_known_ids[0]))

/* ── Class code names ── */

static const char *pci_class_names[16] = {
    "Legacy device",        /* 0x0 */
    "Mass storage controller",  /* 0x1 */
    "Network controller",   /* 0x2 */
    "Display controller",   /* 0x3 */
    "Multimedia device",    /* 0x4 */
    "Memory controller",    /* 0x5 */
    "Bridge device",        /* 0x6 */
    "Communication controller", /* 0x7 */
    "System peripheral",    /* 0x8 */
    "Input device",         /* 0x9 */
    "Docking station",      /* 0xA */
    "Processor",            /* 0xB */
    "Serial bus controller",    /* 0xC */
    "Wireless controller",  /* 0xD */
    "I/O device",           /* 0xE */
    "Satellite comm"        /* 0xF */
};

/**
 * pci_lookup_name - Resolve device name from known-ID table
 *
 * Return: Name string, or class-based fallback
 */
static const char *
pci_lookup_name(uint16_t u16Vendor, uint16_t u16Device,
                uint8_t u8Class, bool *pbKnown)
{
    for (int i = 0; i < PCI_KNOWN_ID_COUNT; i++) {
        if (pci_known_ids[i].vendor_id == u16Vendor &&
            pci_known_ids[i].device_id == u16Device) {
            if (pbKnown)
                *pbKnown = true;
            return pci_known_ids[i].name;
        }
    }
    if (pbKnown)
        *pbKnown = false;
    return pci_class_names[u8Class & 0x0F];
}

/**
 * pci_print_hex2 - Print value as at least 2 hex digits
 */
static void
pci_print_hex2(uint32_t u32Value)
{
    if (u32Value < 0x10)
        mkrn_console_write("0");
    mkrn_console_write_hex(u32Value);
}

/**
 * pci_print_hex4 - Print value as at least 4 hex digits
 */
static void
pci_print_hex4(uint32_t u32Value)
{
    char buffer[5];
    for (int i = 3; i >= 0; i--) {
        uint8_t u8Digit = (u32Value >> (i * 4)) & 0xF;
        buffer[3 - i] = (u8Digit < 10)
                            ? (char)('0' + u8Digit)
                            : (char)('A' + u8Digit - 10);
    }
    buffer[4] = '\0';
    mkrn_console_write(buffer);
}

uint32_t
mkrn_pci_read_config(uint8_t u8Bus, uint8_t u8Slot,
                     uint8_t u8Func, uint8_t u8Offset)
{
    uint32_t u32Address = PCI_CONFIG_ENABLE
        | ((uint32_t)u8Bus << 16)
        | ((uint32_t)(u8Slot & 0x1F) << 11)
        | ((uint32_t)(u8Func & 0x07) << 8)
        | ((uint32_t)u8Offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, u32Address);
    return inl(PCI_CONFIG_DATA);
}

void
mkrn_pci_write_config(uint8_t u8Bus, uint8_t u8Slot,
                      uint8_t u8Func, uint8_t u8Offset,
                      uint32_t u32Value)
{
    uint32_t u32Address = PCI_CONFIG_ENABLE
        | ((uint32_t)u8Bus << 16)
        | ((uint32_t)(u8Slot & 0x1F) << 11)
        | ((uint32_t)(u8Func & 0x07) << 8)
        | ((uint32_t)u8Offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, u32Address);
    outl(PCI_CONFIG_DATA, u32Value);
}

/**
 * pci_probe_function - Probe one bus/slot/function
 *
 * Return: 1 if a device was recorded, 0 otherwise
 */
static int
pci_probe_function(uint8_t u8Bus, uint8_t u8Slot,
                   uint8_t u8Func)
{
    uint32_t u32VendorDev =
        mkrn_pci_read_config(u8Bus, u8Slot, u8Func,
                             M4K_PCI_REG_VENDOR);
    uint16_t u16Vendor = (uint16_t)(u32VendorDev & 0xFFFF);

    if (u16Vendor == PCI_VENDOR_NONE || u16Vendor == 0)
        return 0;

    if (pci_device_count >= M4K_PCI_MAX_DEVICES)
        return 0;

    uint32_t u32ClassRev =
        mkrn_pci_read_config(u8Bus, u8Slot, u8Func,
                             M4K_PCI_REG_REVISION);
    uint32_t u32Header =
        mkrn_pci_read_config(u8Bus, u8Slot, u8Func,
                             M4K_PCI_REG_HEADER_TYPE);
    uint32_t u32Irq =
        mkrn_pci_read_config(u8Bus, u8Slot, u8Func,
                             M4K_PCI_REG_IRQ_LINE);

    mkrn_pci_device_t *dev = &pci_devices[pci_device_count];
    dev->bus = u8Bus;
    dev->slot = u8Slot;
    dev->func = u8Func;
    dev->vendor_id = u16Vendor;
    dev->device_id = (uint16_t)(u32VendorDev >> 16);
    dev->revision = (uint8_t)(u32ClassRev & 0xFF);
    dev->prog_if = (uint8_t)((u32ClassRev >> 8) & 0xFF);
    dev->subclass = (uint8_t)((u32ClassRev >> 16) & 0xFF);
    dev->class_code = (uint8_t)((u32ClassRev >> 24) & 0xFF);
    dev->header_type = (uint8_t)((u32Header >> 16) & 0xFF);
    dev->irq_line = (uint8_t)(u32Irq & 0xFF);
    dev->irq_pin = (uint8_t)((u32Irq >> 8) & 0xFF);

    for (int i = 0; i < 6; i++) {
        dev->bar[i] = mkrn_pci_read_config(
            u8Bus, u8Slot, u8Func,
            M4K_PCI_REG_BAR0 + (uint8_t)(i * 4));
    }

    dev->name = pci_lookup_name(dev->vendor_id,
                                dev->device_id,
                                dev->class_code,
                                &dev->known);
    pci_device_count++;

    /* [PCI] Found device 8086:7010 (IDE controller) */
    mkrn_console_write("[PCI] Found device ");
    pci_print_hex4(dev->vendor_id);
    mkrn_console_write(":");
    pci_print_hex4(dev->device_id);
    mkrn_console_write(" (");
    mkrn_console_write(dev->name);
    mkrn_console_write(") at ");
    pci_print_hex2(u8Bus);
    mkrn_console_write(":");
    pci_print_hex2(u8Slot);
    mkrn_console_write(".");
    mkrn_console_write_dec(u8Func);
    mkrn_console_write(" class ");
    pci_print_hex2(dev->class_code);
    mkrn_console_write(":");
    pci_print_hex2(dev->subclass);
    if (dev->known)
        mkrn_console_write(" [known]");
    mkrn_console_write("\n");

    return 1;
}

int
mkrn_pci_scan_bus(void)
{
    pci_device_count = 0;

    mkrn_console_write("[PCI] Scanning PCI bus...\n");

    for (uint32_t u32Bus = 0; u32Bus < M4K_PCI_MAX_BUSES;
         u32Bus++) {
        for (uint32_t u32Slot = 0;
             u32Slot < M4K_PCI_MAX_SLOTS; u32Slot++) {
            uint8_t u8Bus = (uint8_t)u32Bus;
            uint8_t u8Slot = (uint8_t)u32Slot;

            uint32_t u32VendorDev = mkrn_pci_read_config(
                u8Bus, u8Slot, 0, M4K_PCI_REG_VENDOR);
            uint16_t u16Vendor =
                (uint16_t)(u32VendorDev & 0xFFFF);
            if (u16Vendor == PCI_VENDOR_NONE ||
                u16Vendor == 0)
                continue;

            pci_probe_function(u8Bus, u8Slot, 0);

            uint32_t u32Header = mkrn_pci_read_config(
                u8Bus, u8Slot, 0, M4K_PCI_REG_HEADER_TYPE);
            uint8_t u8HeaderType =
                (uint8_t)((u32Header >> 16) & 0xFF);
            if (!(u8HeaderType & M4K_PCI_HDR_MULTIFUNC))
                continue;

            for (uint32_t u32Func = 1;
                 u32Func < M4K_PCI_MAX_FUNCS; u32Func++) {
                pci_probe_function(u8Bus, u8Slot,
                                   (uint8_t)u32Func);
            }
        }
    }

    pci_scanned = true;

    mkrn_console_write("[PCI] Scan complete: ");
    mkrn_console_write_dec((uint32_t)pci_device_count);
    mkrn_console_write(" device(s) found\n");

    return pci_device_count;
}

int
mkrn_pci_get_device_count(void)
{
    return pci_device_count;
}

const mkrn_pci_device_t *
mkrn_pci_get_device(int index)
{
    if (index < 0 || index >= pci_device_count)
        return NULL;
    return &pci_devices[index];
}

int
mkrn_pci_find_device(uint16_t u16Vendor, uint16_t u16Device,
                     mkrn_pci_device_t *out)
{
    if (!pci_scanned)
        mkrn_pci_scan_bus();

    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == u16Vendor &&
            pci_devices[i].device_id == u16Device) {
            if (out)
                *out = pci_devices[i];
            return 0;
        }
    }
    return -M4K_ENODEV;
}

int
mkrn_pci_find_class(uint8_t u8Class, uint8_t u8Subclass,
                    mkrn_pci_device_t *out)
{
    if (!pci_scanned)
        mkrn_pci_scan_bus();

    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == u8Class &&
            pci_devices[i].subclass == u8Subclass) {
            if (out)
                *out = pci_devices[i];
            return 0;
        }
    }
    return -M4K_ENODEV;
}

void
mkrn_pci_enable_bus_master(const mkrn_pci_device_t *dev)
{
    if (dev == NULL)
        return;
    uint32_t u32Cmd = mkrn_pci_read_config(
        dev->bus, dev->slot, dev->func,
        M4K_PCI_REG_COMMAND);
    u32Cmd |= M4K_PCI_CMD_IO_SPACE
            | M4K_PCI_CMD_MEM_SPACE
            | M4K_PCI_CMD_BUS_MASTER;
    mkrn_pci_write_config(dev->bus, dev->slot, dev->func,
                          M4K_PCI_REG_COMMAND, u32Cmd);
}
