/*
 * M4KK1 4P1 - pci.h
 * Description: PCI bus enumeration and configuration
 *              space access interface (0xCF8/0xCFC).
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define M4K_PCI_MAX_BUSES       256
#define M4K_PCI_MAX_SLOTS       32
#define M4K_PCI_MAX_FUNCS       8
#define M4K_PCI_MAX_DEVICES     32

/* Configuration space register offsets */
#define M4K_PCI_REG_VENDOR      0x00
#define M4K_PCI_REG_DEVICE      0x02
#define M4K_PCI_REG_COMMAND     0x04
#define M4K_PCI_REG_STATUS      0x06
#define M4K_PCI_REG_REVISION    0x08
#define M4K_PCI_REG_PROG_IF     0x09
#define M4K_PCI_REG_SUBCLASS    0x0A
#define M4K_PCI_REG_CLASS       0x0B
#define M4K_PCI_REG_CACHELINE   0x0C
#define M4K_PCI_REG_LATENCY     0x0D
#define M4K_PCI_REG_HEADER_TYPE 0x0E
#define M4K_PCI_REG_BIST        0x0F
#define M4K_PCI_REG_BAR0        0x10
#define M4K_PCI_REG_BAR1        0x14
#define M4K_PCI_REG_BAR2        0x18
#define M4K_PCI_REG_BAR3        0x1C
#define M4K_PCI_REG_BAR4        0x20
#define M4K_PCI_REG_BAR5        0x24
#define M4K_PCI_REG_SUBSYS_VENDOR 0x2C
#define M4K_PCI_REG_SUBSYS_ID   0x2E
#define M4K_PCI_REG_ROM_BAR     0x30
#define M4K_PCI_REG_CAP_PTR     0x34
#define M4K_PCI_REG_IRQ_LINE    0x3C
#define M4K_PCI_REG_IRQ_PIN     0x3D

/* Command register bits */
#define M4K_PCI_CMD_IO_SPACE    0x0001
#define M4K_PCI_CMD_MEM_SPACE   0x0002
#define M4K_PCI_CMD_BUS_MASTER  0x0004

/* Header type bits */
#define M4K_PCI_HDR_MULTIFUNC  0x80
#define M4K_PCI_HDR_TYPE_MASK  0x7F

/* Class codes */
#define M4K_PCI_CLASS_STORAGE       0x01
#define M4K_PCI_CLASS_NETWORK       0x02
#define M4K_PCI_CLASS_DISPLAY       0x03
#define M4K_PCI_CLASS_MULTIMEDIA    0x04
#define M4K_PCI_CLASS_MEMORY        0x05
#define M4K_PCI_CLASS_BRIDGE        0x06
#define M4K_PCI_CLASS_COMM          0x07
#define M4K_PCI_CLASS_SYSTEM        0x08
#define M4K_PCI_CLASS_INPUT         0x09
#define M4K_PCI_CLASS_SERIAL_BUS    0x0C

/* Subclasses used by M4KK1 drivers */
#define M4K_PCI_SUBCLASS_IDE        0x01  /* storage */
#define M4K_PCI_SUBCLASS_ETHERNET   0x00  /* network */
#define M4K_PCI_SUBCLASS_VGA        0x00  /* display */
#define M4K_PCI_SUBCLASS_AUDIO      0x01  /* multimedia */
#define M4K_PCI_SUBCLASS_USB        0x03  /* serial bus */

/* QEMU standard device IDs */
#define M4K_PCI_VENDOR_INTEL        0x8086
#define M4K_PCI_DEV_I440FX_HOST     0x1237
#define M4K_PCI_DEV_PIIX3_ISA       0x7000
#define M4K_PCI_DEV_PIIX3_IDE       0x7010
#define M4K_PCI_DEV_PIIX3_USB_UHCI  0x7112
#define M4K_PCI_DEV_PIIX3_ACPI      0x7113
#define M4K_PCI_DEV_QEMU_VGA        0x1111
#define M4K_PCI_VENDOR_QEMU         0x1234

typedef struct mkrn_pci_device {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision;
    uint8_t  header_type;
    uint8_t  irq_line;
    uint8_t  irq_pin;
    uint32_t bar[6];
    const char *name;
    bool     known;
} mkrn_pci_device_t;

/**
 * mkrn_pci_read_config - Read 32-bit config space register
 * @bus: PCI bus number (0-255)
 * @slot: Device/slot number (0-31)
 * @func: Function number (0-7)
 * @offset: Register offset (DWORD-aligned, 0-252)
 *
 * Return: 32-bit config value (0xFFFFFFFF if absent)
 */
uint32_t mkrn_pci_read_config(uint8_t bus, uint8_t slot,
                              uint8_t func, uint8_t offset);

/**
 * mkrn_pci_write_config - Write 32-bit config space register
 * @bus: PCI bus number (0-255)
 * @slot: Device/slot number (0-31)
 * @func: Function number (0-7)
 * @offset: Register offset (DWORD-aligned, 0-252)
 * @value: Value to write
 *
 * Return: void
 */
void mkrn_pci_write_config(uint8_t bus, uint8_t slot,
                           uint8_t func, uint8_t offset,
                           uint32_t value);

/**
 * mkrn_pci_scan_bus - Enumerate all PCI buses/devices
 *
 * Scans all 256 buses x 32 slots x 8 functions, records
 * present devices, and prints each one to the console
 * (mirrored to serial COM1). QEMU standard devices are
 * marked as known with a descriptive name.
 *
 * Return: Number of devices found (>= 0)
 */
int mkrn_pci_scan_bus(void);

/**
 * mkrn_pci_get_device_count - Number of discovered devices
 *
 * Return: Device count from last scan
 */
int mkrn_pci_get_device_count(void);

/**
 * mkrn_pci_get_device - Get discovered device by index
 * @index: Index into device table (0 .. count-1)
 *
 * Return: Pointer to device descriptor, NULL if invalid
 */
const mkrn_pci_device_t *mkrn_pci_get_device(int index);

/**
 * mkrn_pci_find_device - Locate device by vendor/device ID
 * @vendor_id: Vendor ID to match
 * @device_id: Device ID to match
 * @out: Output descriptor (copied), may be NULL
 *
 * Return: 0 on success, -M4K_ENODEV if not found
 */
int mkrn_pci_find_device(uint16_t vendor_id,
                         uint16_t device_id,
                         mkrn_pci_device_t *out);

/**
 * mkrn_pci_find_class - Locate device by class/subclass
 * @class_code: Class code to match
 * @subclass: Subclass code to match
 * @out: Output descriptor (copied), may be NULL
 *
 * Return: 0 on success, -M4K_ENODEV if not found
 */
int mkrn_pci_find_class(uint8_t class_code,
                        uint8_t subclass,
                        mkrn_pci_device_t *out);

/**
 * mkrn_pci_enable_bus_master - Set bus-master bit in command
 * @dev: Device descriptor (bus/slot/func used)
 *
 * Return: void
 */
void mkrn_pci_enable_bus_master(const mkrn_pci_device_t *dev);
