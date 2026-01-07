/*
 * Copyright (c) 2026 Paolo Oliveira. All rights reserved.
 * Licensed under the MIT License. See LICENSE file in the project root for details.
 * pulse_port_avr.h - AVR (ATmega-class) port for pulse.h
 */

#ifndef PULSE_PORT_AVR_H
#define PULSE_PORT_AVR_H

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define PULSE_PORT_DISABLE_GLOBAL_IRQ() do { cli(); } while (0)
#define PULSE_PORT_ENABLE_GLOBAL_IRQ()  do { sei(); } while (0)

/* NOTE: In serious MISRA contexts you may want to save/restore SREG properly.
 * This simple wrapper is adequate for many bare-metal patterns.
 */
#define PULSE_PORT_ENTER_CRITICAL()     do { cli(); } while (0)
#define PULSE_PORT_EXIT_CRITICAL()      do { sei(); } while (0)

#ifndef PULSE_PORT_IDLE_HOOK
#define PULSE_PORT_IDLE_HOOK() do { } while (0)
#endif

static inline void pulse_port_avr_timer_init(uint32_t tick_ms)
{
    uint32_t ocr;
    uint32_t ticks_per_ms;

    TCCR1A = 0u;
    TCCR1B = 0u;
    TCNT1  = 0u;

    ticks_per_ms = (uint32_t)((uint32_t)F_CPU / 64u / 1000u);
    ocr = (tick_ms * ticks_per_ms);

    if (ocr > 0u)
    {
        ocr -= 1u;
    }

    TCCR1B |= (uint8_t)(1u << WGM12);
    TCCR1B |= (uint8_t)((1u << CS11) | (1u << CS10));

    OCR1A = (uint16_t)ocr;

    TIMSK1 |= (uint8_t)(1u << OCIE1A);
}

#define PULSE_PORT_TIMER_INIT(tick_ms) do { pulse_port_avr_timer_init((tick_ms)); } while (0)

ISR(TIMER1_COMPA_vect)
{
    extern void pulse_tick_isr(void);
    pulse_tick_isr();
}

#endif /* PULSE_PORT_AVR_H */
