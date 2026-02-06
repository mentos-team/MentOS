/// @file   rtc.c
/// @brief  Real Time Clock (RTC) driver.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// @addtogroup rtc
/// @{

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[RTC   ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "descriptor_tables/isr.h"
#include "drivers/rtc.h"
#include "hardware/pic8259.h"
#include "io/port_io.h"
#include "kernel.h"
#include "proc_access.h"
#include "string.h"

// ============================================================================
// RTC Port Definitions
// ============================================================================

#define CMOS_ADDR        0x70 ///< I/O port for CMOS address selection.
#define CMOS_DATA        0x71 ///< I/O port for CMOS data read/write.
#define CMOS_NMI_DISABLE 0x80 ///< Disable NMI when selecting CMOS register.
#define CMOS_IOWAIT_PORT 0x80 ///< I/O wait port used for short delays.

// ============================================================================
// RTC Module Variables
// ============================================================================

/// Current global time updated by RTC interrupt handler.
tm_t global_time          = {0};
/// Previous global time used for consistency detection during initialization.
tm_t previous_global_time = {0};
/// Data type flag: 1 if BCD format, 0 if binary format.
int is_bcd;

// ============================================================================
// RTC Condition and Wait Functions
// ============================================================================

/// @brief Short I/O wait to let CMOS address/data lines settle.
static inline void __rtc_io_wait(void)
{
    outportb(CMOS_IOWAIT_PORT, 0);
    outportb(CMOS_IOWAIT_PORT, 0);
    outportb(CMOS_IOWAIT_PORT, 0);
    outportb(CMOS_IOWAIT_PORT, 0);
}

/// @brief Check if RTC is currently updating (UIP flag set).
/// @return Non-zero if updating, 0 if ready to read.
static inline unsigned int __rtc_is_updating(void)
{
    outportb(CMOS_ADDR, (unsigned char)(CMOS_NMI_DISABLE | 0x0A));
    __rtc_io_wait();
    unsigned char status = inportb(CMOS_DATA);
    __asm__ __volatile__("" ::: "memory");
    return (status & 0x80);
}

/// @brief Checks if two time values are identical.
/// @param t0 First time value to compare.
/// @param t1 Second time value to compare.
/// @return 1 if identical, 0 if different.
static inline unsigned int __rtc_times_match(tm_t *t0, tm_t *t1)
{
    return (t0->tm_sec == t1->tm_sec) &&
           (t0->tm_min == t1->tm_min) &&
           (t0->tm_hour == t1->tm_hour) &&
           (t0->tm_mon == t1->tm_mon) &&
           (t0->tm_year == t1->tm_year) &&
           (t0->tm_wday == t1->tm_wday) &&
           (t0->tm_mday == t1->tm_mday);
}

// ============================================================================
// RTC I/O Functions
// ============================================================================

/// @brief Reads a CMOS register using inline assembly to prevent compiler optimization.
/// @param reg The CMOS register number (0x00-0x0F for RTC, higher for extension).
/// @return The value read from the CMOS register.
/// @details Uses direct inline assembly to ensure the I/O operations cannot be
/// optimized away by aggressive compiler optimizations in Release mode. Sets NMI
/// disable bit (0x80) during access, performs I/O wait cycles, and enforces
/// memory barriers to guarantee correct execution order.
__attribute__((noinline)) static unsigned char __rtc_read_cmos_direct(unsigned char reg)
{
    volatile unsigned char value;
    // Direct inline assembly prevents any compiler optimization in Release mode.
    // This is critical for CMOS/RTC reads which have hardware timing requirements.
    __asm__ __volatile__(
        "movb %1, %%al\n\t"                            // Load register number with NMI disabled
        "outb %%al, $0x70\n\t"                         // Select CMOS register (port 0x70)
        "outb %%al, $0x80\n\t"                         // I/O wait cycle (port 0x80 is diagnostic port)
        "outb %%al, $0x80\n\t"                         // Second I/O wait (~400ns total)
        "inb $0x71, %%al\n\t"                          // Read CMOS data (port 0x71)
        "movb %%al, %0"                                // Store result
        : "=m"(value)                                  // Output: value
        : "r"((unsigned char)(CMOS_NMI_DISABLE | reg)) // Input: register with NMI disabled
        : "al", "memory"                               // Clobbered: AL register, all memory
    );
    return value;
}

/// @brief Writes a value to a CMOS register.
/// @param reg The register address to write to.
/// @param value The value to write.
/// @details Disables NMI during write, performs I/O wait for hardware timing.
static inline void write_register(unsigned char reg, unsigned char value)
{
    outportb(CMOS_ADDR, (unsigned char)(CMOS_NMI_DISABLE | reg));
    __rtc_io_wait();
    outportb(CMOS_DATA, value);
}

/// @brief Converts a Binary-Coded Decimal (BCD) value to binary.
/// @param bcd The BCD value to convert.
/// @return The binary (decimal) equivalent.
static inline unsigned char bcd2bin(unsigned char bcd) { return ((bcd >> 4U) * 10) + (bcd & 0x0FU); }

/// @brief Reads the current datetime value from the RTC.
/// @details Reads all time fields (seconds, minutes, hours, month, year, weekday, monthday)
/// from CMOS registers and stores them in the global_time structure. Handles both
/// BCD and binary formats based on the control register configuration.
/// @note Uses direct assembly CMOS reads to prevent compiler optimization in Release mode.
__attribute__((noinline)) static void rtc_read_datetime(void)
{
    // Wait until RTC update cycle completes (UIP bit clears).
    // This ensures we read a consistent snapshot of the time registers.
    volatile unsigned int timeout = 10000;
    while (__rtc_is_updating() && timeout--) {
        pause();
    }

    // Warn if UIP flag never cleared (hardware issue or extreme timing problem).
    if (timeout == 0) {
        unsigned char status_a = __rtc_read_cmos_direct(0x0A);
        unsigned char status_b = __rtc_read_cmos_direct(0x0B);
        unsigned char status_c = __rtc_read_cmos_direct(0x0C);
        pr_warning("rtc_read_datetime: UIP timeout (A=0x%02x B=0x%02x C=0x%02x)\n", status_a, status_b, status_c);
    }

    // Read all RTC time/date registers using optimized direct assembly reads.
    // Using the unified __rtc_read_cmos_direct() ensures no compiler optimization.
    unsigned char sec  = __rtc_read_cmos_direct(0x00);
    unsigned char min  = __rtc_read_cmos_direct(0x02);
    unsigned char hour = __rtc_read_cmos_direct(0x04);
    unsigned char mon  = __rtc_read_cmos_direct(0x08);
    unsigned char year = __rtc_read_cmos_direct(0x09);
    unsigned char wday = __rtc_read_cmos_direct(0x06);
    unsigned char mday = __rtc_read_cmos_direct(0x07);

    // Debug output for troubleshooting.
    pr_debug("Raw RTC: sec=%u min=%u hour=%u mon=%u year=%u wday=%u mday=%u (BCD=%d)\n", sec, min, hour, mon, year, wday, mday, is_bcd);

    // Diagnostic checks for known hardware failure modes (one-shot warnings).
    if (sec == 0 && min == 0 && hour == 0 && mon == 0 && year == 0 && wday == 0 && mday == 0) {
        static int warned_zero = 0;
        if (!warned_zero) {
            warned_zero = 1;
            pr_warning("rtc_read_datetime: all-zero read (hardware not initialized or QEMU issue)\n");
        }
    }
    if (sec == 0xFF && min == 0xFF && hour == 0xFF && mon == 0xFF &&
        year == 0xFF && wday == 0xFF && mday == 0xFF) {
        static int warned_ff = 0;
        if (!warned_ff) {
            warned_ff = 1;
            pr_warning("rtc_read_datetime: all-0xFF read (CMOS bus floating or disconnected)\n");
        }
    }

    // Check for mirrored register indices (data port echoing address instead of data).
    if (sec == (0x80 | 0x00) && min == (0x80 | 0x02) && hour == (0x80 | 0x04) &&
        wday == (0x80 | 0x06) && mday == (0x80 | 0x07) && mon == (0x80 | 0x08) &&
        year == (0x80 | 0x09)) {
        static int warned_mirror = 0;
        if (!warned_mirror) {
            warned_mirror          = 1;
            unsigned char status_a = __rtc_read_cmos_direct(0x0A);
            unsigned char status_b = __rtc_read_cmos_direct(0x0B);
            unsigned char status_c = __rtc_read_cmos_direct(0x0C);
            pr_warning("rtc_read_datetime: mirrored index values (A=0x%02x B=0x%02x C=0x%02x)\n", status_a, status_b, status_c);
        }
    }

    // Convert and store the datetime values.
    if (is_bcd) {
        // BCD format: each nibble is a decimal digit (e.g., 0x59 = 59).
        global_time.tm_sec  = bcd2bin(sec);
        global_time.tm_min  = bcd2bin(min);
        global_time.tm_hour = bcd2bin(hour);
        global_time.tm_mon  = bcd2bin(mon);
        global_time.tm_year = bcd2bin(year) + 2000;
        global_time.tm_wday = bcd2bin(wday);
        global_time.tm_mday = bcd2bin(mday);
    } else {
        // Binary format: direct values.
        global_time.tm_sec  = sec;
        global_time.tm_min  = min;
        global_time.tm_hour = hour;
        global_time.tm_mon  = mon;
        global_time.tm_year = year + 2000;
        global_time.tm_wday = wday;
        global_time.tm_mday = mday;
    }

    // Force memory barrier to ensure writes complete
    __asm__ __volatile__("" ::: "memory");
}

// ============================================================================
// RTC Core Driver Functions
// ============================================================================

/// @brief Updates the global datetime by reading from the RTC controller.
/// @details Safely reads the current time from the RTC using timeout protection
/// to prevent infinite loops. On initial boot, performs a double-read to ensure
/// the value has stabilized (i.e., detect a change since the last read interval).
/// Uses the unified __wait_for_condition() helper with volatile semantics to
/// ensure the compiler cannot optimize away timing-critical wait loops.
static inline void rtc_update_datetime(void)
{
    // Read until we get two consecutive identical reads, confirming stability.
    // This OSDev-recommended approach ensures we didn't catch the RTC mid-update.
    volatile unsigned int timeout = 10000;
    while (timeout--) {
        // First read.
        rtc_read_datetime();
        previous_global_time = global_time;

        // Second read.
        rtc_read_datetime();

        // If both reads match, we have a stable value.
        if (__rtc_times_match(&previous_global_time, &global_time)) {
            return;
        }
    }
    // If we timeout, use the last read value anyway.
    pr_warning("rtc_update_datetime: timeout waiting for stable read\n");
}

// ============================================================================
// RTC Controller Initialization
// ============================================================================

/// @brief Interrupt service routine for RTC events.
/// @param f Pointer to the saved processor state at interrupt time.
/// @details Called by the interrupt handler when the RTC generates an interrupt
/// (typically on update-ended interrupt). Updates the global time structure.
static inline void rtc_handler_isr(pt_regs_t *f) { rtc_update_datetime(); }

void gettime(tm_t *time)
{
    // Copy the current global time to the provided buffer.
    memcpy(time, &global_time, sizeof(tm_t));
}

/// @brief Initializes the Real-Time Clock driver.
/// @return 0 on success, -1 on failure.
/// @details Configures the RTC for 24-hour mode and update-ended interrupts,
/// installs the interrupt handler, and performs an initial time read.
int rtc_initialize(void)
{
    unsigned char status;

    // Read the control register B to modify interrupt configuration.
    status = __rtc_read_cmos_direct(0x0B);
    // Enable 24-hour mode (bit 1).
    status |= 0x02U;
    // Enable update-ended interrupt (bit 4) to get notified when time changes.
    status |= 0x10U;
    // Disable alarm interrupts (bit 5).
    status &= ~0x20U;
    // Disable periodic interrupt (bit 6).
    status &= ~0x40U;
    // Check the data format: BCD (bit 2 = 0) or binary (bit 2 = 1).
    is_bcd = !(status & 0x04U);
    // Write the updated configuration back.
    write_register(0x0B, status);

    // Clear any pending interrupts by reading register C.
    __rtc_read_cmos_direct(0x0C);

    // Install the RTC interrupt handler for the real-time clock IRQ.
    irq_install_handler(IRQ_REAL_TIME_CLOCK, rtc_handler_isr, "Real Time Clock (RTC)");
    // Enable the RTC IRQ at the PIC level.
    pic8259_irq_enable(IRQ_REAL_TIME_CLOCK);

    // Perform initial time synchronization.
    rtc_update_datetime();

    // Debug print the initialized time.
    pr_debug("RTC initialized: %04d-%02d-%02d %02d:%02d:%02d (BCD: %s)\n", global_time.tm_year, global_time.tm_mon, global_time.tm_mday, global_time.tm_hour, global_time.tm_min, global_time.tm_sec, is_bcd ? "Yes" : "No");
    return 0;
}

/// @brief Finalizes the Real-Time Clock driver.
/// @return 0 on success.
/// @details Uninstalls the interrupt handler and disables the RTC IRQ.
int rtc_finalize(void)
{
    // Uninstall the IRQ.
    irq_uninstall_handler(IRQ_REAL_TIME_CLOCK, rtc_handler_isr);
    // Disable the IRQ.
    pic8259_irq_disable(IRQ_REAL_TIME_CLOCK);
    return 0;
}

/// @}
