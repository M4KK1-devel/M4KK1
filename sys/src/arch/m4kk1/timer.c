/*
 * M4KK1 4P1 - timer.c
 * Description: Timer subsystem — PIT-based tick,
 *              RTC read/write, alarm management,
 *              calibration, and sleep functions.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../../include/timer.h"
#include "../../include/idt.h"
#include "../../include/stdint.h"
#include "../../include/console.h"
#include "../../include/kernel.h"
#include "../../include/process.h"
#include "../../include/string.h"

static void
outb(uint16_t u16Port, uint8_t u8Value)
{
    __asm__ volatile(
        "outb %0, %1"
        : : "a"(u8Value), "Nd"(u16Port));
}

static uint8_t
inb(uint16_t u16Port)
{
    uint8_t u8Value;
    __asm__ volatile(
        "inb %1, %0"
        : "=a"(u8Value) : "Nd"(u16Port));
    return u8Value;
}

static uint32_t u32TimerTicks = 0;
static uint32_t u32TimerFrequency = 1000;
static uint64_t u64TimerNanoseconds = 0;
static void (*pfnTimerCallback)(void) = NULL;

#define MAX_ALARMS 256
static mkrn_timer_alarm_t alarms[MAX_ALARMS];
static uint32_t u32NextAlarmId = 1;
static uint32_t u32ActiveAlarms = 0;

static uint32_t u32CpuFrequencyMhz = 0;

static uint8_t
cmos_read(uint8_t u8Reg)
{
    outb(0x70, u8Reg);
    return inb(0x71);
}

static void
cmos_write(uint8_t u8Reg, uint8_t u8Value)
{
    outb(0x70, u8Reg);
    outb(0x71, u8Value);
}

/**
 * @brief  Read the current RTC time.
 */
void
mkrn_timer_read_rtc(mkrn_time_t *pTime)
{
    uint8_t u8StatusB;

    u8StatusB = cmos_read(M4K_RTC_STATUS_B);

    while (cmos_read(M4K_RTC_STATUS_A) & 0x80) {}

    pTime->second = cmos_read(M4K_RTC_SECONDS);
    pTime->minute = cmos_read(M4K_RTC_MINUTES);
    pTime->hour = cmos_read(M4K_RTC_HOURS);
    pTime->day = cmos_read(M4K_RTC_DAY);
    pTime->month = cmos_read(M4K_RTC_MONTH);
    pTime->year = cmos_read(M4K_RTC_YEAR);

    if (!(u8StatusB & M4K_RTC_BINARY_MODE)) {
        pTime->second =
            (pTime->second >> 4) * 10
            + (pTime->second & 0x0F);
        pTime->minute =
            (pTime->minute >> 4) * 10
            + (pTime->minute & 0x0F);
        pTime->hour =
            (pTime->hour >> 4) * 10
            + (pTime->hour & 0x0F);
        pTime->day =
            (pTime->day >> 4) * 10
            + (pTime->day & 0x0F);
        pTime->month =
            (pTime->month >> 4) * 10
            + (pTime->month & 0x0F);
        pTime->year =
            (pTime->year >> 4) * 10
            + (pTime->year & 0x0F);
    }
    pTime->year += 2000;
}

/**
 * @brief  Set the RTC time.
 */
void
mkrn_timer_set_rtc(mkrn_time_t *pTime)
{
    uint8_t u8StatusB;

    u8StatusB = cmos_read(M4K_RTC_STATUS_B);

    while (cmos_read(M4K_RTC_STATUS_A) & 0x80) {}

    if (!(u8StatusB & M4K_RTC_BINARY_MODE)) {
        pTime->second =
            ((pTime->second / 10) << 4)
            | (pTime->second % 10);
        pTime->minute =
            ((pTime->minute / 10) << 4)
            | (pTime->minute % 10);
        pTime->hour =
            ((pTime->hour / 10) << 4)
            | (pTime->hour % 10);
        pTime->day =
            ((pTime->day / 10) << 4)
            | (pTime->day % 10);
        pTime->month =
            ((pTime->month / 10) << 4)
            | (pTime->month % 10);
        pTime->year =
            (((pTime->year % 100) / 10) << 4)
            | (pTime->year % 10);
    } else {
        pTime->year %= 100;
    }

    cmos_write(M4K_RTC_SECONDS, pTime->second);
    cmos_write(M4K_RTC_MINUTES, pTime->minute);
    cmos_write(M4K_RTC_HOURS, pTime->hour);
    cmos_write(M4K_RTC_DAY, pTime->day);
    cmos_write(M4K_RTC_MONTH, pTime->month);
    cmos_write(M4K_RTC_YEAR, pTime->year);
}

/**
 * @brief  Initialize the timer system.
 */
void
mkrn_timer_init(uint32_t u32Frequency)
{
    uint32_t u32Divisor;

    u32TimerFrequency = u32Frequency;

    M4K_LOG_INFO(
        "Initializing timer system...");

    mkrn_memset(alarms, 0, sizeof(alarms));

    u32Divisor = M4K_TIMER_FREQ / u32Frequency;

    __asm__ volatile("cli");
    outb(M4K_PIT_COMMAND,
         M4K_PIT_SC0 | M4K_PIT_BOTH
             | M4K_PIT_MODE_3 | M4K_PIT_BINARY);
    outb(M4K_PIT_CHANNEL_0,
         (uint8_t)(u32Divisor & 0xFF));
    outb(M4K_PIT_CHANNEL_0,
         (uint8_t)((u32Divisor >> 8) & 0xFF));
    outb(0x21, 0x00);
    __asm__ volatile("sti");

    M4K_LOG_INFO("RTC/CMOS skipped\n");
}

/**
 * @brief  Set the PIT frequency.
 */
void
mkrn_timer_set_frequency(uint32_t u32Frequency)
{
    uint32_t u32Divisor;

    u32TimerFrequency = u32Frequency;
    u32Divisor = M4K_TIMER_FREQ / u32Frequency;

    outb(M4K_PIT_COMMAND,
         M4K_PIT_SC0 | M4K_PIT_BOTH
             | M4K_PIT_MODE_3 | M4K_PIT_BINARY);
    outb(M4K_PIT_CHANNEL_0,
         (uint8_t)(u32Divisor & 0xFF));
    outb(M4K_PIT_CHANNEL_0,
         (uint8_t)((u32Divisor >> 8) & 0xFF));

    M4K_LOG_INFO("Timer frequency set to: ");
    mkrn_console_write_dec(u32Frequency);
    mkrn_console_write(" Hz\n");
}

/**
 * @brief  Get the current tick count.
 */
uint32_t
mkrn_timer_get_ticks(void)
{
    return u32TimerTicks;
}

/**
 * @brief  Busy-wait for a given number of
 *         milliseconds.
 */
void
mkrn_timer_wait(uint32_t u32Milliseconds)
{
    uint32_t u32Start = u32TimerTicks;
    uint32_t u32TicksToWait =
        u32Milliseconds * u32TimerFrequency
        / 1000;

    while ((u32TimerTicks - u32Start)
           < u32TicksToWait)
    {
        __asm__ volatile("hlt");
    }
}

/**
 * @brief  Get the system uptime in milliseconds.
 */
uint32_t
mkrn_timer_get_uptime(void)
{
    return u32TimerTicks * 1000
           / u32TimerFrequency;
}

/**
 * @brief  Timer interrupt handler — called on each
 *         PIT tick.
 *
 * @param  frame  pointer to the interrupt frame pushed by
 *         irq_timer: [0..7] = pusha regs (edi,esi,ebp,esp,
 *         ebx,edx,ecx,eax), [8] = interrupted eip,
 *         [9] = interrupted cs, [10] = eflags.
 */
void
mkrn_timer_handler(uint32_t *frame)
{
    u32TimerTicks++;
    u64TimerNanoseconds +=
        (1000000000ULL / u32TimerFrequency);

    for (uint32_t i = 0; i < MAX_ALARMS; i++) {
        if (alarms[i].active
            && alarms[i].callback)
        {
            alarms[i].remaining_ms--;

            if (alarms[i].remaining_ms == 0) {
                alarms[i].callback();

                if (alarms[i].interval_ms > 0)
                    alarms[i].remaining_ms =
                        alarms[i].interval_ms;
                else {
                    alarms[i].active = 0;
                    u32ActiveAlarms--;
                }
            }
        }
    }

    if (pfnTimerCallback)
        pfnTimerCallback();
}

/**
 * @brief  Sleep for a given number of microseconds.
 */
void
mkrn_timer_usleep(uint32_t u32Microseconds)
{
    uint64_t u64Start = u64TimerNanoseconds;
    uint64_t u64Nsecs =
        (uint64_t)u32Microseconds * 1000;

    while ((u64TimerNanoseconds - u64Start)
           < u64Nsecs)
    {
        __asm__ volatile("hlt");
    }
}

/**
 * @brief  Sleep for a given number of seconds.
 */
void
mkrn_timer_sleep(uint32_t u32Seconds)
{
    mkrn_timer_wait(u32Seconds * 1000);
}

/**
 * @brief  Get the calibrated CPU frequency in MHz.
 */
uint32_t
mkrn_timer_get_cpu_frequency(void)
{
    return u32CpuFrequencyMhz;
}

/**
 * @brief  Get the current timer frequency in Hz.
 */
uint32_t
mkrn_timer_get_frequency(void)
{
    return u32TimerFrequency;
}

/**
 * @brief  Calibrate the timer to measure CPU
 *         frequency.
 */
void
mkrn_timer_calibrate(void)
{
    uint64_t u64StartTsc;
    uint64_t u64EndTsc;

    M4K_LOG_INFO("Calibrating timer...");

    uint32_t u32StartTicks =
        mkrn_timer_get_ticks();

    __asm__ volatile(
        "rdtsc" : "=A"(u64StartTsc));

    mkrn_timer_wait(100);

    uint32_t u32EndTicks =
        mkrn_timer_get_ticks();
    __asm__ volatile(
        "rdtsc" : "=A"(u64EndTsc));

    uint64_t u64TscDiff =
        u64EndTsc - u64StartTsc;
    uint32_t u32TicksDiff =
        u32EndTicks - u32StartTicks;
    uint32_t u32TimeMs =
        u32TicksDiff * 1000 / u32TimerFrequency;

    if (u32TimeMs > 0) {
        uint32_t u32TimeMs32 = u32TimeMs;
        uint32_t u32TscDiff32 =
            (uint32_t)u64TscDiff;

        if (u32TimeMs32 > 0 && u32TscDiff32 > 0)
            u32CpuFrequencyMhz =
                u32TscDiff32 / u32TimeMs32;
        else
            u32CpuFrequencyMhz = 1000;
    } else
        u32CpuFrequencyMhz = 1000;

    M4K_LOG_INFO(
        "CPU frequency calibrated: ");
    mkrn_console_write_dec(
        u32CpuFrequencyMhz);
    mkrn_console_write(" MHz\n");
}

/**
 * @brief  Register a timer interrupt callback.
 */
void
mkrn_timer_register_handler(
    void (*pHandler)(void))
{
    pfnTimerCallback = pHandler;

    M4K_LOG_INFO(
        "Timer handler registered");
}

/**
 * @brief  Create a one-shot or periodic alarm.
 */
uint32_t
mkrn_timer_create_alarm(
    uint32_t u32IntervalMs,
    void (*pCallback)(void))
{
    if (!pCallback || u32IntervalMs == 0)
        return 0;

    for (uint32_t i = 0; i < MAX_ALARMS; i++) {
        if (!alarms[i].active) {
            alarms[i].id = u32NextAlarmId++;
            alarms[i].interval_ms = u32IntervalMs;
            alarms[i].remaining_ms =
                u32IntervalMs;
            alarms[i].active = 1;
            alarms[i].callback = pCallback;
            u32ActiveAlarms++;

            M4K_LOG_INFO("Alarm created: ID=");
            mkrn_console_write_dec(
                alarms[i].id);
            mkrn_console_write(
                ", Interval=");
            mkrn_console_write_dec(
                u32IntervalMs);
            mkrn_console_write("ms\n");

            return alarms[i].id;
        }
    }

    M4K_LOG_WARN(
        "Maximum alarms reached, cannot create "
        "more alarms");
    return 0;
}

/**
 * @brief  Destroy an alarm by ID.
 */
int32_t
mkrn_timer_destroy_alarm(uint32_t u32AlarmId)
{
    for (uint32_t i = 0; i < MAX_ALARMS; i++) {
        if (alarms[i].active
            && alarms[i].id == u32AlarmId)
        {
            alarms[i].active = 0;
            alarms[i].callback = NULL;
            u32ActiveAlarms--;

            M4K_LOG_INFO(
                "Alarm destroyed: ID=");
            mkrn_console_write_dec(u32AlarmId);
            mkrn_console_write("\n");

            return 0;
        }
    }

    M4K_LOG_WARN("Alarm not found: ID=");
    mkrn_console_write_dec(u32AlarmId);
    mkrn_console_write("\n");

    return -1;
}

/**
 * @brief  Get the number of active alarms.
 */
uint32_t
mkrn_timer_get_active_count(void)
{
    return u32ActiveAlarms;
}

/**
 * @brief  Get the nanosecond timestamp.
 */
uint64_t
mkrn_timer_get_nanoseconds(void)
{
    return u64TimerNanoseconds;
}

/**
 * @brief  Sleep for a given number of nanoseconds.
 */
void
mkrn_timer_nsleep(uint64_t u64Nanoseconds)
{
    uint64_t u64Start = u64TimerNanoseconds;

    while ((u64TimerNanoseconds - u64Start)
           < u64Nanoseconds)
    {
        __asm__ volatile("hlt");
    }
}
