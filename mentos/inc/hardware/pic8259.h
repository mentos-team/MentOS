///                MentOS, The Mentoring Operating system project
/// @file pic8259.h
/// @brief Pic8259 definitions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "idt.h"
#include "kheap.h"
#include "debug.h"
#include "stddef.h"
#include "kernel.h"
#include "bitops.h"
#include "port_io.h"
#include "irqflags.h"
#include "scheduler.h"

/// The total number of IRQs.
#define IRQ_NUM 16

/// @defgroup irqs Interrupt Requests (IRQs).
/// @brief This is the list of interrupt requests.
/// @{

/// @brief System timer.
#define IRQ_TIMER 0

/// @brief Keyboard controller.
#define IRQ_KEYBOARD 1

/// @brief cascaded signals from IRQs 8–15 (any devices configured to use IRQ
///        2 will actually be using IRQ 9)
#define IRQ_TO_SLAVE_PIC 2

/// @brief Serial port controller for serial port 2 (and 4).
#define IRQ_COM2_4 3

/// @brief Serial port controller for serial port 1 (and 3).
#define IRQ_COM1_3 4

/// @brief Parallel port 2 and 3 (or sound card).
#define IRQ_LPT2 5

/// @brief Floppy disk controller.
#define IRQ_FLOPPY 6

/// @brief Parallel port 1.
#define IRQ_LPT1 7

/// @brief Real-time clock (RTC).
#define IRQ_REAL_TIME_CLOCK 8

/// @brief Advanced Configuration and Power Interface (ACPI)
///        system control interrupt on Intel chipsets.[1] Other chipset
///        manufacturers might use another interrupt for this purpose, or
///        make it available for the use of peripherals (any devices configured
///        to use IRQ 2 will actually be using IRQ 9)
#define IRQ_AVAILABLE_1 9

/// @brief The Interrupt is left open for the use of
///        peripherals (open interrupt/available, SCSI or NIC).
#define IRQ_AVAILABLE_2 10

/// @brief The Interrupt is left open for the use of
///        peripherals (open interrupt/available, SCSI or NIC).
#define IRQ_AVAILABLE_3 11

/// @brief Mouse on PS/2 connector.
#define IRQ_MOUSE 12

/// @brief CPU co-processor or integrated floating point unit
///        or inter-processor interrupt (use depends on OS).
#define IRQ_MATH_CPU 13

/// @brief Primary ATA channel (ATA interface usually serves
///        hard disk drives and CD drives).
#define IRQ_FIRST_HD 14

/// @brief Secondary ATA channel.
#define IRQ_SECOND_HD 15

/// @}

/// @brief Function that initializes the processor pic 8259 that will manage the
///        interruptions.
void pic8259_init_irq();

/// @brief     This function, enable irqs on the pic.
/// @details   This function provide a tool for enabling irq from the pic
///            processor.
/// @param irq Number of irq to enable.
/// @return 0  If all OK, -1 on errors.
int pic8259_irq_enable(uint32_t irq);

/// @brief     This function, disable irqs on the pic.
/// @details   This function provide a tool for enabling irq from the pic
///            processor.
/// @param irq Number of irq to enable.
/// @return 0  If all OK, -1 on errors.
int pic8259_irq_disable(uint32_t irq);

/// @brief     This is issued to the PIC chips at the end of an IRQ-based
///            interrupt routine.
/// @param irq The interrupt number.
void pic8259_send_eoi(uint32_t irq);

/// @brief  This Function return the number of current IRQ Request.
/// @return Number of IRQ + 1 currently serving. If 0 there are no IRQ.
//int pic8259_irq_get_current();
