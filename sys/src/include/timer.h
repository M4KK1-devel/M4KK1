/*
 * M4KK1 4P1 - timer.h
 * Description: Timer and clock management function declarations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>

#define M4K_TIMER_FREQ 1193180

/* PIT I/O ports */
#define M4K_PIT_CHANNEL_0   0x40
#define M4K_PIT_CHANNEL_1   0x41
#define M4K_PIT_CHANNEL_2   0x42
#define M4K_PIT_COMMAND     0x43

/* PIT command: select channel */
#define M4K_PIT_SC0         0x00
#define M4K_PIT_SC1         0x40
#define M4K_PIT_SC2         0x80

#define M4K_PIT_BINARY      0x00
#define M4K_PIT_BCD         0x01
#define M4K_PIT_MODE_0      0x00
#define M4K_PIT_MODE_1      0x02
#define M4K_PIT_MODE_2      0x04
#define M4K_PIT_MODE_3      0x06
#define M4K_PIT_MODE_4      0x08
#define M4K_PIT_MODE_5      0x0A
#define M4K_PIT_LATCH       0x00
#define M4K_PIT_LOW         0x10
#define M4K_PIT_HIGH        0x20
#define M4K_PIT_BOTH        0x30

/* RTC registers */
#define M4K_RTC_SECONDS     0x00
#define M4K_RTC_SECONDS_ALARM 0x01
#define M4K_RTC_MINUTES     0x02
#define M4K_RTC_MINUTES_ALARM 0x03
#define M4K_RTC_HOURS       0x04
#define M4K_RTC_HOURS_ALARM 0x05
#define M4K_RTC_WEEKDAY     0x06
#define M4K_RTC_DAY         0x07
#define M4K_RTC_MONTH       0x08
#define M4K_RTC_YEAR        0x09
#define M4K_RTC_STATUS_A    0x0A
#define M4K_RTC_STATUS_B    0x0B
#define M4K_RTC_STATUS_C    0x0C
#define M4K_RTC_STATUS_D    0x0D

/* RTC status B flags */
#define M4K_RTC_CLOCK_24H   0x02
#define M4K_RTC_BINARY_MODE 0x04
#define M4K_RTC_SQUARE_WAVE 0x08
#define M4K_RTC_UPDATE_INT  0x10
#define M4K_RTC_ALARM_INT   0x20
#define M4K_RTC_PERIODIC_INT 0x40
#define M4K_RTC_UPDATE_END_INT 0x80

typedef struct {
    u8 second;
    u8 minute;
    u8 hour;
    u8 day;
    u8 month;
    u32 year;
} mkrn_time_t;

typedef struct {
    u32 id;
    u32 interval_ms;
    u32 remaining_ms;
    u8 active;
    void (*callback)(void);
} mkrn_timer_alarm_t;

/**
 * mkrn_timer_init - Initialize the timer
 * @frequency: Timer frequency in Hz
 *
 * Return: void
 */
void mkrn_timer_init(u32 frequency);

/**
 * mkrn_timer_set_frequency - Set PIT frequency
 * @frequency: Frequency in Hz
 *
 * Return: void
 */
void mkrn_timer_set_frequency(u32 frequency);

/**
 * mkrn_timer_get_ticks - Get timer tick count
 *
 * Return: Current tick count
 */
u32 mkrn_timer_get_ticks(void);

/**
 * mkrn_timer_wait - Wait for specified milliseconds
 * @milliseconds: Duration in milliseconds
 *
 * Return: void
 */
void mkrn_timer_wait(u32 milliseconds);

/**
 * mkrn_timer_get_uptime - Get system uptime
 *
 * Return: Uptime in milliseconds
 */
u32 mkrn_timer_get_uptime(void);

/**
 * mkrn_timer_read_rtc - Read RTC time
 * @time: Structure to fill with time data
 *
 * Return: void
 */
void mkrn_timer_read_rtc(mkrn_time_t *time);

/**
 * mkrn_timer_set_rtc - Set RTC time
 * @time: Structure with time data
 *
 * Return: void
 */
void mkrn_timer_set_rtc(mkrn_time_t *time);

/**
 * mkrn_timer_handler - Timer interrupt handler
 *
 * Return: void
 */
void mkrn_timer_handler(void);

/**
 * mkrn_timer_usleep - Sleep for specified microseconds
 * @microseconds: Duration in microseconds
 *
 * Return: void
 */
void mkrn_timer_usleep(u32 microseconds);

/**
 * mkrn_timer_sleep - Sleep for specified seconds
 * @seconds: Duration in seconds
 *
 * Return: void
 */
void mkrn_timer_sleep(u32 seconds);

/**
 * mkrn_timer_get_cpu_frequency - Get CPU frequency
 *
 * Return: CPU frequency in MHz
 */
u32 mkrn_timer_get_cpu_frequency(void);

/**
 * mkrn_timer_calibrate - Calibrate the timer
 *
 * Return: void
 */
void mkrn_timer_calibrate(void);

/**
 * mkrn_timer_register_handler - Register timer interrupt handler
 * @handler: Handler function pointer
 *
 * Return: void
 */
void mkrn_timer_register_handler(void (*handler)(void));

/**
 * mkrn_timer_create_alarm - Create a timer alarm
 * @interval_ms: Alarm interval in milliseconds
 * @callback: Callback function
 *
 * Return: Alarm ID, 0 on failure
 */
u32 mkrn_timer_create_alarm(u32 interval_ms, void (*callback)(void));

/**
 * mkrn_timer_destroy_alarm - Destroy a timer alarm
 * @alarm_id: Alarm ID
 *
 * Return: 0 on success, -1 on failure
 */
s32 mkrn_timer_destroy_alarm(u32 alarm_id);

/**
 * mkrn_timer_get_active_count - Get active alarm count
 *
 * Return: Number of active alarms
 */
u32 mkrn_timer_get_active_count(void);

/**
 * mkrn_timer_get_nanoseconds - Get nanosecond timestamp
 *
 * Return: Current nanosecond timestamp
 */
u64 mkrn_timer_get_nanoseconds(void);

/**
 * mkrn_timer_nsleep - Sleep for specified nanoseconds
 * @nanoseconds: Duration in nanoseconds
 *
 * Return: void
 */
void mkrn_timer_nsleep(u64 nanoseconds);
