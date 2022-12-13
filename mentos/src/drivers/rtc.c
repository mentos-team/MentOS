/// @file   rtc.c
/// @brief  Real Time Clock (RTC) driver.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup rtc
/// @{

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[RTC   ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "drivers/rtc.h"

#include "hardware/pic8259.h"
#include "string.h"
#include "io/port_io.h"
#include "kernel.h"
#include "descriptor_tables/isr.h"

#define CMOS_ADDR 0x70 ///< Addess where we need to write the Address.
#define CMOS_DATA 0x71 ///< Addess where we need to write the Data.

/// Current global time.
tm_t global_time;
/// Previous global time.
tm_t previous_global_time;
/// Data type is BCD.
int is_bcd;

static inline unsigned int rtc_are_different(tm_t *t0, tm_t *t1)
{
    if (t0->tm_sec != t1->tm_sec)
        return 1;
    if (t0->tm_min != t1->tm_min)
        return 1;
    if (t0->tm_hour != t1->tm_hour)
        return 1;
    if (t0->tm_mon != t1->tm_mon)
        return 1;
    if (t0->tm_year != t1->tm_year)
        return 1;
    if (t0->tm_wday != t1->tm_wday)
        return 1;
    if (t0->tm_mday != t1->tm_mday)
        return 1;
    return 0;
}

/// @brief Check if rtc is updating time currently.
static inline unsigned int is_updating_rtc()
{
    outportb(CMOS_ADDR, 0x0A);
    uint32_t status = inportb(CMOS_DATA);
    return (status & 0x80U);
}

static inline unsigned char read_register(unsigned char reg)
{
    outportb(CMOS_ADDR, reg);
    return inportb(CMOS_DATA);
}

static inline void write_register(unsigned char reg, unsigned char value)
{
    outportb(CMOS_ADDR, reg);
    outportb(CMOS_DATA, value);
}

static inline unsigned char bcd2bin(unsigned char bcd)
{
    return ((bcd >> 4u) * 10) + (bcd & 0x0Fu);
}

static inline void rtc_read_datetime()
{
    if (read_register(0x0Cu) & 0x10u) {
        if (is_bcd) {
            global_time.tm_sec  = bcd2bin(read_register(0x00));
            global_time.tm_min  = bcd2bin(read_register(0x02));
            global_time.tm_hour = bcd2bin(read_register(0x04)) + 2;
            global_time.tm_mon  = bcd2bin(read_register(0x08));
            global_time.tm_year = bcd2bin(read_register(0x09)) + 2000;
            global_time.tm_wday = bcd2bin(read_register(0x06));
            global_time.tm_mday = bcd2bin(read_register(0x07));
        } else {
            global_time.tm_sec  = read_register(0x00);
            global_time.tm_min  = read_register(0x02);
            global_time.tm_hour = read_register(0x04) + 2;
            global_time.tm_mon  = read_register(0x08);
            global_time.tm_year = read_register(0x09) + 2000;
            global_time.tm_wday = read_register(0x06);
            global_time.tm_mday = read_register(0x07);
        }
    }
}

static inline void rtc_update_datetime()
{
    static unsigned int first_update = 1;
    // Wait until rtc is not updating.
    while (is_updating_rtc()) {}
    // Read the values.
    rtc_read_datetime();
    if (first_update) {
        do {
            // Save the previous global time.
            previous_global_time = global_time;
            // Wait until rtc is not updating.
            while (is_updating_rtc()) {}
            // Read the values.
            rtc_read_datetime();
        } while (!rtc_are_different(&previous_global_time, &global_time));
        first_update = 0;
    }
}

static inline void rtc_handler_isr(pt_regs *f)
{
    rtc_update_datetime();
}

void gettime(tm_t *time)
{
    // Copy the update time.
    memcpy(time, &global_time, sizeof(tm_t));
}

int rtc_initialize()
{
    unsigned char status;

    status = read_register(0x0B);
    status |= 0x02u;            // 24 hour clock
    status |= 0x10u;            // update ended interrupts
    status &= ~0x20u;           // no alarm interrupts
    status &= ~0x40u;           // no periodic interrupt
    is_bcd = !(status & 0x04u); // check if data type is BCD
    write_register(0x0B, status);

    read_register(0x0C);

    // Install the IRQ.
    irq_install_handler(IRQ_REAL_TIME_CLOCK, rtc_handler_isr, "Real Time Clock (RTC)");
    // Enable the IRQ.
    pic8259_irq_enable(IRQ_REAL_TIME_CLOCK);
    // Wait until rtc is ready.
    rtc_update_datetime();
    return 0;
}

int rtc_finalize()
{
    // Uninstall the IRQ.
    irq_uninstall_handler(IRQ_REAL_TIME_CLOCK, rtc_handler_isr);
    // Disable the IRQ.
    pic8259_irq_disable(IRQ_REAL_TIME_CLOCK);
    return 0;
}

/// @}
