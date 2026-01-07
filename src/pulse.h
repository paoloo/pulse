/* 
 * Copyright (c) 2026 Paolo Oliveira. All rights reserved.
 * Licensed under the MIT License. See LICENSE file in the project root for details.
 * pulse.h: A tiny cooperative real-time scheduler for microcontrollers.
 */

#ifndef PULSE_H
#define PULSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#include "pulse_version.h"

/* -------------------------- Configuration -------------------------- */

/* Maximum tasks supported (compile-time cap). Keep small for tiny MCUs. */
#ifndef PULSE_MAX_TASKS
#define PULSE_MAX_TASKS (16u)
#endif

/* If 1, scheduler will guard against NULL tick function pointers. */
#ifndef PULSE_CFG_NULL_TICK_GUARD
#define PULSE_CFG_NULL_TICK_GUARD (1u)
#endif

/* If 1, each task is marked ready immediately when added.
 * If 0, first run occurs after one full period elapses.
 */
#ifndef PULSE_CFG_RUN_IMMEDIATELY
#define PULSE_CFG_RUN_IMMEDIATELY (1u)
#endif

/* If 1, saturate elapsed_ticks at UINT32_MAX to avoid wrap. */
#ifndef PULSE_CFG_SATURATE_ELAPSED
#define PULSE_CFG_SATURATE_ELAPSED (1u)
#endif

/* -------------------------- Compile-time guards -------------------------- */

#if (PULSE_MAX_TASKS < 1u)
#error "PULSE_MAX_TASKS must be >= 1"
#endif

/* ready_mask uses 64-bit bitset */
#if (PULSE_MAX_TASKS > 64u)
#error "PULSE_MAX_TASKS too large for this kernel; pick <= 64"
#endif

#if ((PULSE_CFG_RUN_IMMEDIATELY != 0u) && (PULSE_CFG_RUN_IMMEDIATELY != 1u))
#error "PULSE_CFG_RUN_IMMEDIATELY must be 0 or 1"
#endif

#if ((PULSE_CFG_NULL_TICK_GUARD != 0u) && (PULSE_CFG_NULL_TICK_GUARD != 1u))
#error "PULSE_CFG_NULL_TICK_GUARD must be 0 or 1"
#endif

#if ((PULSE_CFG_SATURATE_ELAPSED != 0u) && (PULSE_CFG_SATURATE_ELAPSED != 1u))
#error "PULSE_CFG_SATURATE_ELAPSED must be 0 or 1"
#endif

/* -------------------------- Port contract -------------------------- */
/* A port header MUST define these. */
#ifndef PULSE_PORT_ENTER_CRITICAL
#error "Pulse port missing: PULSE_PORT_ENTER_CRITICAL()"
#endif
#ifndef PULSE_PORT_EXIT_CRITICAL
#error "Pulse port missing: PULSE_PORT_EXIT_CRITICAL()"
#endif
#ifndef PULSE_PORT_TIMER_INIT
#error "Pulse port missing: PULSE_PORT_TIMER_INIT(tick_ms)"
#endif
#ifndef PULSE_PORT_ENABLE_GLOBAL_IRQ
#error "Pulse port missing: PULSE_PORT_ENABLE_GLOBAL_IRQ()"
#endif
#ifndef PULSE_PORT_DISABLE_GLOBAL_IRQ
#error "Pulse port missing: PULSE_PORT_DISABLE_GLOBAL_IRQ()"
#endif

#ifndef PULSE_PORT_IDLE_HOOK
#define PULSE_PORT_IDLE_HOOK() do { } while (0)
#endif

/* -------------------------- Types -------------------------- */

typedef int32_t pulse_state_t;

typedef pulse_state_t (*pulse_tick_f)(pulse_state_t state);

typedef struct
{
    uint8_t       running;       /* 0 = not running, 1 = running */
    pulse_state_t state;         /* task state for state-machine style tasks */
    uint32_t      period_ticks;  /* task period in ticks (must be > 0) */
    uint32_t      elapsed_ticks; /* elapsed ticks since last run */
    pulse_tick_f  tick;          /* tick function */
} pulse_task_t;

typedef struct
{
    pulse_task_t tasks[PULSE_MAX_TASKS];
    uint8_t      task_count;

    /* Ready bitmask: bit i set => task i is ready to run.
     * ISR sets bits; pulse_poll() clears and runs tasks.
     */
    uint64_t     ready_mask;

    uint8_t      started;

    uint32_t     tick_ms;
} pulse_kernel_t;

/* -------------------------- API -------------------------- */

void pulse_init(uint32_t tick_ms);

int32_t pulse_add_task(pulse_state_t init_state,
                       uint32_t period_ticks,
                       pulse_tick_f tick);

void pulse_start(void);

/* Call from your timer ISR: marks tasks ready only. */
void pulse_tick_isr(void);

/* Run all ready tasks (highest priority first) in main/thread context. */
void pulse_poll(void);

uint8_t pulse_is_started(void);

uint32_t pulse_tick_period_ms(void);

/* -------------------------- Convenience macros -------------------------- */

#define PULSE_UNUSED(x) do { (void)(x); } while (0)

/* -------------------------- Implementation -------------------------- */

#ifdef PULSE_IMPLEMENTATION

static pulse_kernel_t pulse_kernel;

static uint64_t pulse_task_bit(uint8_t id)
{
    return (uint64_t)1u << (uint64_t)id;
}

static int32_t pulse_find_lowest_set_bit(uint64_t mask)
{
    uint8_t i;
    for (i = 0u; i < (uint8_t)PULSE_MAX_TASKS; i++)
    {
        if ((mask & pulse_task_bit(i)) != 0u)
        {
            return (int32_t)i;
        }
    }
    return -1;
}

void pulse_init(uint32_t tick_ms)
{
    uint8_t i;

    if (tick_ms == 0u)
    {
        tick_ms = 1u;
    }

    PULSE_PORT_DISABLE_GLOBAL_IRQ();

    pulse_kernel.task_count = 0u;
    pulse_kernel.started = 0u;
    pulse_kernel.tick_ms = tick_ms;
    pulse_kernel.ready_mask = 0u;

    for (i = 0u; i < (uint8_t)PULSE_MAX_TASKS; i++)
    {
        pulse_kernel.tasks[i].running = 0u;
        pulse_kernel.tasks[i].state = 0;
        pulse_kernel.tasks[i].period_ticks = 0u;
        pulse_kernel.tasks[i].elapsed_ticks = 0u;
        pulse_kernel.tasks[i].tick = (pulse_tick_f)0;
    }

    PULSE_PORT_DISABLE_GLOBAL_IRQ();
}

int32_t pulse_add_task(pulse_state_t init_state,
                       uint32_t period_ticks,
                       pulse_tick_f tick)
{
    uint8_t idx;

    if (period_ticks == 0u)
    {
        return -1;
    }

#if (PULSE_CFG_NULL_TICK_GUARD == 1u)
    if (tick == (pulse_tick_f)0)
    {
        return -2;
    }
#endif

    PULSE_PORT_ENTER_CRITICAL();

    if (pulse_kernel.task_count >= (uint8_t)PULSE_MAX_TASKS)
    {
        PULSE_PORT_EXIT_CRITICAL();
        return -3;
    }

    idx = pulse_kernel.task_count;

    pulse_kernel.tasks[idx].running = 0u;
    pulse_kernel.tasks[idx].state = init_state;
    pulse_kernel.tasks[idx].period_ticks = period_ticks;

#if (PULSE_CFG_RUN_IMMEDIATELY == 1u)
    /* allow an immediate release */
    pulse_kernel.tasks[idx].elapsed_ticks = period_ticks;
#else
    pulse_kernel.tasks[idx].elapsed_ticks = 0u;
#endif

    pulse_kernel.tasks[idx].tick = tick;

#if (PULSE_CFG_RUN_IMMEDIATELY == 1u)
    /* Mark ready immediately so tests/superloops can run without waiting a tick. */
    pulse_kernel.ready_mask |= pulse_task_bit(idx);
#endif

    pulse_kernel.task_count = (uint8_t)(pulse_kernel.task_count + 1u);

    PULSE_PORT_EXIT_CRITICAL();

    return 0;
}

uint8_t pulse_is_started(void)
{
    return pulse_kernel.started;
}

uint32_t pulse_tick_period_ms(void)
{
    return pulse_kernel.tick_ms;
}

void pulse_tick_isr(void)
{
    uint8_t i;

    for (i = 0u; i < pulse_kernel.task_count; i++)
    {
        pulse_task_t * const t = &pulse_kernel.tasks[i];

#if (PULSE_CFG_SATURATE_ELAPSED == 1u)
        if (t->elapsed_ticks < 0xFFFFFFFFu)
        {
            t->elapsed_ticks++;
        }
#else
        t->elapsed_ticks++;
#endif

        if (t->elapsed_ticks >= t->period_ticks)
        {
            if (t->running == 0u)
            {
                /* Do not reset elapsed_ticks here; reset when task actually runs.
                 * This avoids losing releases if polling is delayed.
                 */
                PULSE_PORT_ENTER_CRITICAL();
                pulse_kernel.ready_mask |= pulse_task_bit(i);
                PULSE_PORT_EXIT_CRITICAL();
            }
        }
    }
}

void pulse_poll(void)
{
    uint64_t mask;
    int32_t id;

    for (;;)
    {
        PULSE_PORT_ENTER_CRITICAL();
        mask = pulse_kernel.ready_mask;
        id = pulse_find_lowest_set_bit(mask);
        if (id >= 0)
        {
            pulse_kernel.ready_mask &= ~pulse_task_bit((uint8_t)id);
            pulse_kernel.tasks[(uint8_t)id].running = 1u;
            pulse_kernel.tasks[(uint8_t)id].elapsed_ticks = 0u;
        }
        PULSE_PORT_EXIT_CRITICAL();

        if (id < 0)
        {
            break;
        }

        {
            pulse_task_t * const t = &pulse_kernel.tasks[(uint8_t)id];

#if (PULSE_CFG_NULL_TICK_GUARD == 1u)
            if (t->tick != (pulse_tick_f)0)
#endif
            {
                t->state = t->tick(t->state);
            }

            PULSE_PORT_ENTER_CRITICAL();
            t->running = 0u;
            PULSE_PORT_EXIT_CRITICAL();
        }
    }
}

void pulse_start(void)
{
    if (pulse_kernel.started != 0u)
    {
        for (;;)
        {
            PULSE_PORT_IDLE_HOOK();
        }
    }

    pulse_kernel.started = 1u;

    PULSE_PORT_TIMER_INIT(pulse_kernel.tick_ms);
    PULSE_PORT_ENABLE_GLOBAL_IRQ();

    for (;;)
    {
        pulse_poll();
        PULSE_PORT_IDLE_HOOK();
    }
}

#endif /* PULSE_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* PULSE_H */
