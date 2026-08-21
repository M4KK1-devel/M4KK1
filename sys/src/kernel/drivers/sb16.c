/*
 * M4KK1 4P1 - sb16.c
 * Description: Sound Blaster 16 (SB16) ISA audio driver.
 *              DSP reset/version detection, mixer setup, 8-bit
 *              single-cycle DMA playback and square-wave beeps.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "sb16.h"
#include <console.h>
#include <idt.h>
#include <kernel.h>
#include <string.h>

extern void pic_unmask_irq(uint32_t irq_num);

static bool sb16_present = false;
static bool sb16_rate_set = false;

static volatile int sb16_irq_pending = 0;

/* 32 KB in kernel .bss below 16 MB (kernel base 0x100000).  The
 * toolchain caps __attribute__((aligned)) at 8 KB, so boundary
 * safety is enforced at runtime in play_8bit instead. */
static uint8_t sb16_dma_buf[M4K_SB16_MAX_SINGLE] __attribute__((aligned(16)));

static inline void outb(uint16_t u16Port, uint8_t u8Value)
{
    __asm__ volatile("outb %0, %1" : : "a"(u8Value), "Nd"(u16Port));
}

static inline uint8_t inb(uint16_t u16Port)
{
    uint8_t u8Value;
    __asm__ volatile("inb %1, %0" : "=a"(u8Value) : "Nd"(u16Port));
    return u8Value;
}

static inline void sb16_io_wait(void)
{
    /* ~1 us delay via the ISA post port (safe on QEMU and real HW) */
    outb(0x80, 0x00);
}

static int
sb16_dsp_write(uint8_t u8Cmd)
{
    /* Bit 7 of 0x22C set means the write buffer is full */
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(M4K_SB16_DSP_WRITE) & 0x80) == 0) {
            outb(M4K_SB16_DSP_WRITE, u8Cmd);
            return 0;
        }
        sb16_io_wait();
    }
    return -1;
}

static int
sb16_dsp_read(uint8_t *pOut)
{
    /* Bit 7 of 0x22E set means read data is ready */
    for (uint32_t i = 0; i < 100000; i++) {
        if (inb(M4K_SB16_DSP_STATUS) & 0x80) {
            *pOut = inb(M4K_SB16_DSP_READ);
            return 0;
        }
        sb16_io_wait();
    }
    return -1;
}

static int
sb16_dsp_reset(void)
{
    outb(M4K_SB16_DSP_RESET, 0x01);
    sb16_io_wait();
    sb16_io_wait();
    outb(M4K_SB16_DSP_RESET, 0x00);

    uint8_t u8Byte;
    for (uint32_t i = 0; i < 100000; i++) {
        if (sb16_dsp_read(&u8Byte) == 0) {
            if (u8Byte == 0xAA)
                return 0;
            return -1;
        }
        sb16_io_wait();
    }
    return -1;
}

static int
sb16_set_mixer(uint8_t u8Reg, uint8_t u8Value)
{
    outb(M4K_SB16_MIXER_ADDR, u8Reg);
    sb16_io_wait();
    outb(M4K_SB16_MIXER_DATA, u8Value);
    return 0;
}

static void
mkrn_sb16_irq_handler(void)
{
    sb16_irq_pending = 1;
    /* Reading the DSP status register acknowledges the IRQ */
    (void)inb(M4K_SB16_DSP_STATUS);
}

int
mkrn_sb16_init(void)
{
    if (sb16_present)
        return 0;

    mkrn_console_write("[SB16] Probing base 0x220, IRQ 5, DMA 1...\n");

    if (sb16_dsp_reset() != 0) {
        mkrn_console_write("[SB16] No DSP response (not present).\n");
        return -1;
    }

    /* Ask the DSP for its version: 0xE1 returns major, minor */
    if (sb16_dsp_write(M4K_SB16_CMD_GET_VERSION) != 0) {
        mkrn_console_write("[SB16] DSP version read failed.\n");
        return -1;
    }

    uint8_t u8Major = 0, u8Minor = 0;
    if (sb16_dsp_read(&u8Major) != 0 || sb16_dsp_read(&u8Minor) != 0) {
        mkrn_console_write("[SB16] DSP version read failed.\n");
        return -1;
    }

    mkrn_console_write("[SB16] DSP detected, version ");
    mkrn_console_write_dec((uint32_t)u8Major);
    mkrn_console_write(".");
    if (u8Minor < 10) {
        mkrn_console_write("0");
        mkrn_console_write_dec((uint32_t)u8Minor);
    } else {
        mkrn_console_write_dec((uint32_t)u8Minor);
    }
    mkrn_console_write("\n");

    /* Master volume ~4/15 on both channels */
    sb16_set_mixer(M4K_SB16_MIX_MASTER_VOL, 0x44);

    mkrn_idt_register_handler(0x20 + M4K_SB16_IRQ,
                              (mkrn_int_handler_t)mkrn_sb16_irq_handler);
    pic_unmask_irq(M4K_SB16_IRQ);

    sb16_present = true;
    return 0;
}

bool
mkrn_sb16_available(void)
{
    return sb16_present;
}

int
mkrn_sb16_set_rate(uint32_t u32Hz)
{
    if (!sb16_present)
        return -1;

    if (sb16_dsp_write(M4K_SB16_CMD_SET_RATE) != 0)
        return -1;
    /* 0x41 takes a 16-bit sample rate, most significant byte first */
    if (sb16_dsp_write((uint8_t)((u32Hz >> 8) & 0xFF)) != 0)
        return -1;
    if (sb16_dsp_write((uint8_t)(u32Hz & 0xFF)) != 0)
        return -1;

    sb16_rate_set = true;
    return 0;
}

static void
sb16_program_dma_ch1(uint32_t u32Phys, uint32_t u32Count)
{
    /* Mask channel 1 (bit2=1 mask, bits1-0=channel) */
    outb(M4K_DMA_MASK_REG, 0x05);
    /* Clear the address/count byte flip-flop */
    outb(M4K_DMA_FF_CLEAR, 0x00);
    /* Mode: single transfer, address increment, write, channel 1 */
    outb(M4K_DMA_MODE_REG, 0x45);
    /* 16-bit address, low byte then high byte */
    outb(M4K_DMA_ADDR_1, (uint8_t)(u32Phys & 0xFF));
    outb(M4K_DMA_ADDR_1, (uint8_t)((u32Phys >> 8) & 0xFF));
    /* Page register (bits 23-16 of the physical address) */
    outb(M4K_DMA_PAGE_1, (uint8_t)((u32Phys >> 16) & 0xFF));
    /* Transfer count = length - 1, low byte then high byte */
    outb(M4K_DMA_COUNT_1, (uint8_t)(u32Count & 0xFF));
    outb(M4K_DMA_COUNT_1, (uint8_t)((u32Count >> 8) & 0xFF));
    /* Unmask channel 1 */
    outb(M4K_DMA_MASK_REG, 0x01);
}

int
mkrn_sb16_play_8bit(const uint8_t *pBuf, uint32_t u32Len)
{
    if (!sb16_present)
        return -1;
    if (pBuf == NULL || u32Len == 0 || u32Len > M4K_SB16_MAX_SINGLE)
        return -1;

    if (!sb16_rate_set) {
        if (mkrn_sb16_set_rate(M4K_SB16_SAMPLE_RATE) != 0)
            return -1;
    }

    /* Flat address space: virtual == physical.  ISA DMA channel 1 is
     * limited to a 24-bit address (< 16 MB) and must not cross a 64 KB
     * page boundary.  A caller buffer failing either check would be
     * silently aliased to the wrong physical page; bounce through the
     * static low-memory buffer instead. */
    uint32_t u32Phys = (uint32_t)(uintptr_t)pBuf;
    if (u32Phys >= 0x1000000u
        || ((u32Phys ^ (u32Phys + u32Len - 1)) & 0xFFFF0000u) != 0) {
        /* Bounce through the static low buffer.  It is only 16-aligned
         * (the toolchain caps __attribute__((aligned)) at 8 KB), so it
         * may itself straddle a 64 KB boundary — truncate to its first
         * page run (audio is fire-and-forget; a shortened tail beats
         * playing from the wrong physical page). */
        uint32_t u32Bounce = (uint32_t)(uintptr_t)sb16_dma_buf;
        uint32_t u32Run = 0x10000u - (u32Bounce & 0xFFFFu);
        if (u32Run > M4K_SB16_MAX_SINGLE)
            u32Run = M4K_SB16_MAX_SINGLE;
        if (u32Run == 0)
            return -1;      /* misconfigured .bss layout */
        if (u32Len > u32Run)
            u32Len = u32Run;
        mkrn_memcpy(sb16_dma_buf, pBuf, u32Len);
        pBuf = sb16_dma_buf;
        u32Phys = u32Bounce;
    }

    sb16_program_dma_ch1(u32Phys, u32Len - 1);

    sb16_irq_pending = 0;
    if (sb16_dsp_write(M4K_SB16_CMD_OUT8_SINGLE) != 0)
        return -1;
    /* 0x14 length bytes, most significant byte first */
    if (sb16_dsp_write((uint8_t)((u32Len >> 8) & 0xFF)) != 0)
        return -1;
    if (sb16_dsp_write((uint8_t)(u32Len & 0xFF)) != 0)
        return -1;

    /* Wait for the completion IRQ; timeout scales with audio duration.
     * When called from a syscall ISR (int 0x4D, interrupt gate) the
     * CPU runs with IF=0 and the IRQ can never fire, so fire-and-forget:
     * the DMA transfer still plays to completion. */
    uint32_t u32Eflags;
    __asm__ volatile("pushfl; pop %0" : "=g"(u32Eflags));
    if (u32Eflags & 0x200U) {
        uint32_t u32WaitMs = (u32Len * 1000U) / M4K_SB16_SAMPLE_RATE + 500U;
        volatile uint32_t u32Spin = u32WaitMs * 150000U;
        while (!sb16_irq_pending && u32Spin-- > 0)
            ;
        return sb16_irq_pending ? 0 : -1;
    }

    return 0;
}

int
mkrn_sb16_beep(uint32_t u32Hz, uint32_t u32Ms)
{
    if (!sb16_present)
        return -1;
    if (u32Hz == 0)
        u32Hz = 440;
    if (u32Ms == 0)
        u32Ms = 200;

    uint32_t u32Rate = M4K_SB16_SAMPLE_RATE;
    uint32_t u32Samples = (u32Ms * u32Rate) / 1000U;
    if (u32Samples > M4K_SB16_MAX_SINGLE)
        u32Samples = M4K_SB16_MAX_SINGLE;

    uint32_t u32HalfPeriod = u32Rate / (2U * u32Hz);
    if (u32HalfPeriod == 0)
        u32HalfPeriod = 1;

    for (uint32_t i = 0; i < u32Samples; i++) {
        uint8_t u8Sample = (((i / u32HalfPeriod) & 1U) != 0)
                               ? 0x20U
                               : 0xE0U;
        sb16_dma_buf[i] = u8Sample;
    }

    if (mkrn_sb16_set_rate(u32Rate) != 0)
        return -1;

    return mkrn_sb16_play_8bit(sb16_dma_buf, u32Samples);
}
