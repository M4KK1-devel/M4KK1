/*
 * M4KK1 4P1 - sb16.h
 * Description: Sound Blaster 16 (SB16) ISA audio driver interface.
 *              DSP detection, mixer init, 8-bit single-cycle DMA
 *              playback and square-wave beep generation.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#ifndef _M4KK1_SB16_H_
#define _M4KK1_SB16_H_

#include <stdint.h>
#include <stdbool.h>

#define M4K_SB16_IRQ            5
#define M4K_SB16_DMA_CHANNEL    1

/* Classic ISA base port of the Sound Blaster 16 */
#define M4K_SB16_BASE           0x220
#define M4K_SB16_DSP_RESET      (M4K_SB16_BASE + 0x06)  /* 0x226 */
#define M4K_SB16_DSP_READ       (M4K_SB16_BASE + 0x0A)  /* 0x22A */
#define M4K_SB16_DSP_WRITE      (M4K_SB16_BASE + 0x0C)  /* 0x22C */
#define M4K_SB16_DSP_STATUS     (M4K_SB16_BASE + 0x0E)  /* 0x22E */
#define M4K_SB16_MIXER_ADDR     (M4K_SB16_BASE + 0x04)  /* 0x224 */
#define M4K_SB16_MIXER_DATA     (M4K_SB16_BASE + 0x05)  /* 0x225 */

/* 8237 DMA controller ports (channels 0-3, 8-bit) */
#define M4K_DMA_ADDR_1          0x02
#define M4K_DMA_COUNT_1         0x03
#define M4K_DMA_PAGE_1          0x83
#define M4K_DMA_MASK_REG        0x0A
#define M4K_DMA_MODE_REG        0x0B
#define M4K_DMA_FF_CLEAR        0x0C

/* DSP commands */
#define M4K_SB16_CMD_OUT8_SINGLE 0x14
#define M4K_SB16_CMD_SET_RATE    0x41
#define M4K_SB16_CMD_GET_VERSION 0xE1

/* Mixer registers */
#define M4K_SB16_MIX_MASTER_VOL 0x22

/* 0x14 single-cycle 8-bit transfers are limited to 0x8000 bytes */
#define M4K_SB16_MAX_SINGLE     0x8000

/* Default sample rate used for beep generation */
#define M4K_SB16_SAMPLE_RATE    22050

/**
 * mkrn_sb16_init - Reset and probe the SB16 DSP.
 *
 * Performs the DSP reset handshake, reads the DSP version and
 * initialises the mixer. Prints detection diagnostics via
 * mkrn_console_write.
 *
 * Return: 0 on success, -1 if no SB16 is present.
 */
int mkrn_sb16_init(void);

/**
 * mkrn_sb16_available - Whether a detected SB16 is usable.
 *
 * Return: true if mkrn_sb16_init succeeded.
 */
bool mkrn_sb16_available(void);

/**
 * mkrn_sb16_set_rate - Program the DSP output sample rate.
 *
 * Return: 0 on success, -1 on failure.
 */
int mkrn_sb16_set_rate(uint32_t u32Hz);

/**
 * mkrn_sb16_play_8bit - Play an 8-bit unsigned PCM buffer once.
 *
 * Uses 8-bit single-cycle DMA on channel 1. Blocks until the DMA
 * transfer completes (IRQ 5) or a time-based timeout expires.
 * Buffer must be DMA-addressable (below 16 MB); in this kernel the
 * flat address space makes any static buffer valid.
 *
 * Return: 0 on success, -1 on failure.
 */
int mkrn_sb16_play_8bit(const uint8_t *pBuf, uint32_t u32Len);

/**
 * mkrn_sb16_beep - Generate and play a square-wave beep.
 *
 * Return: 0 on success, -1 on failure.
 */
int mkrn_sb16_beep(uint32_t u32Hz, uint32_t u32Ms);

#endif /* _M4KK1_SB16_H_ */
