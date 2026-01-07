/*
 * Copyright (c) 2026 Paolo Oliveira. All rights reserved.
 * Licensed under the MIT License. See LICENSE file in the project root for details.
 * pulse_port_msp430.h - MSP430 port for pulse.h
 */

#ifndef PULSE_PORT_MSP430_H
#define PULSE_PORT_MSP430_H

#include <stdint.h>
#include <msp430.h>

#ifndef PULSE_MSP430_TICK_HZ
#error "Define PULSE_MSP430_TICK_HZ (e.g., 32768 for ACLK, 1000000 for 1MHz SMCLK)"
#endif

#define PULSE_PORT_DISABLE_GLOBAL_IRQ() do { __disable_interrupt(); } while (0)
#define PULSE_PORT_ENABLE_GLOBAL_IRQ()  do { __enable_interrupt(); } while (0)

#define PULSE_PORT_ENTER_CRITICAL()     do { __disable_interrupt(); } while (0)
#define PULSE_PORT_EXIT_CRITICAL()      do { __enable_interrupt(); } while (0)

#ifndef PULSE_PORT_IDLE_HOOK
#define PULSE_PORT_IDLE_HOOK() do { } while (0)
#endif

#ifndef PULSE_MSP430_TIMER_SRC
#define PULSE_MSP430_TIMER_SRC TASSEL__ACLK
#endif

static inline void pulse_port_msp430_timer_init(uint32_t tick_ms)
{
    uint32_t counts_per_ms;
    uint32_t ccr0;

    TA0CTL = MC__STOP;
    TA0R = 0u;

    counts_per_ms = (uint32_t)(PULSE_MSP430_TICK_HZ / 1000u);
    ccr0 = (tick_ms * counts_per_ms);

    if (ccr0 > 0u)
    {
        ccr0 -= 1u;
    }

    TA0CCR0 = (uint16_t)ccr0;
    TA0CCTL0 = CCIE;

    TA0CTL = (uint16_t)(PULSE_MSP430_TIMER_SRC | MC__UP | TACLR);
}

#define PULSE_PORT_TIMER_INIT(tick_ms) do { pulse_port_msp430_timer_init((tick_ms)); } while (0)

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER0_A0_VECTOR
__interrupt void pulse_timer0_a0_isr(void)
{
    extern void pulse_tick_isr(void);
    pulse_tick_isr();
}
#else
void __attribute__((interrupt(TIMER0_A0_VECTOR))) pulse_timer0_a0_isr(void)
{
    extern void pulse_tick_isr(void);
    pulse_tick_isr();
}
#endif

#endif /* PULSE_PORT_MSP430_H */
