/*
 * Copyright (c) 2026 Paolo Oliveira. All rights reserved.
 * Licensed under the MIT License. See LICENSE file in the project root for details. 
 * test_telemetry.c - Telemetry data-sharing pattern example test for Pulse (hosted)
 *
 * This test demonstrates an IPC-like pattern (snapshot + sequence counter) used
 * for downlink telemetry: producer tasks update a shared struct; a consumer task
 * reads a consistent snapshot and "transmits" it.
 *
 * The goal is to validate:
 *   1) Producer/consumer task wiring and scheduling order under Pulse.
 *   2) Snapshot consistency logic (seqlock-style) under simulated interference.
 *
 * Build notes:
 *   - This file is intended to be compiled alongside Pulse headers using the
 *     host port (pulse_port_host.h).
 *
 * MISRA/NASA style notes (pragmatic for tests):
 *   - No dynamic allocation. Explicit widths. Simple control flow.
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "pulse_port_host.h"
#include "pulse_version.h"

/* One TU must define PULSE_IMPLEMENTATION */
#define PULSE_IMPLEMENTATION
#define PULSE_MAX_TASKS (8u)
#include "pulse.h"

/* ---------------- Telemetry snapshot + seqlock ---------------- */

typedef struct
{
    uint32_t tick;
    int16_t  temp_c;
    uint16_t vbat_mv;
} telemetry_t;

static volatile uint32_t g_seq = 0u;
static telemetry_t       g_tlm;

/* Test-only hook used to simulate a writer "interrupting" a read. */
static uint8_t g_inject_once = 0u;

static void tlm_write_begin(void)
{
    g_seq++; /* odd => write in progress */
}

static void tlm_write_end(void)
{
    g_seq++; /* even => stable */
}

/* Returns 1 if snapshot is consistent. */
static uint8_t tlm_read_snapshot(telemetry_t * const out)
{
    uint32_t s0;
    uint32_t s1;

    if (out == (telemetry_t *)0)
    {
        return 0u;
    }

    for (;;)
    {
        s0 = g_seq;
        if ((s0 & 1u) != 0u)
        {
            continue; /* writer in progress */
        }

        /* Simulate interference exactly once: after seeing an even seq, a writer
         * starts and completes an update, forcing a retry.
         */
        if (g_inject_once != 0u)
        {
            g_inject_once = 0u;

            tlm_write_begin();
            g_tlm.temp_c = (int16_t)(g_tlm.temp_c + 1);
            tlm_write_end();
        }

        *out = g_tlm;

        s1 = g_seq;
        if ((s0 == s1) && ((s1 & 1u) == 0u))
        {
            break;
        }
    }

    return 1u;
}

/* ---------------- Test harness logging ---------------- */

typedef struct
{
    uint32_t tick;
    uint8_t  kind;     /* 0=sensor, 1=battery, 2=tx */
    telemetry_t snap;  /* only valid for tx */
} event_t;

static event_t g_log[64];
static uint32_t g_log_len = 0u;
static uint32_t g_now_tick = 0u;

static void log_sensor(void)
{
    if (g_log_len < (uint32_t)(sizeof(g_log) / sizeof(g_log[0])))
    {
        g_log[g_log_len].tick = g_now_tick;
        g_log[g_log_len].kind = 0u;
        (void)memset(&g_log[g_log_len].snap, 0, sizeof(g_log[g_log_len].snap));
        g_log_len++;
    }
}

static void log_battery(void)
{
    if (g_log_len < (uint32_t)(sizeof(g_log) / sizeof(g_log[0])))
    {
        g_log[g_log_len].tick = g_now_tick;
        g_log[g_log_len].kind = 1u;
        (void)memset(&g_log[g_log_len].snap, 0, sizeof(g_log[g_log_len].snap));
        g_log_len++;
    }
}

static void log_tx(const telemetry_t * const snap)
{
    if ((g_log_len < (uint32_t)(sizeof(g_log) / sizeof(g_log[0]))) && (snap != (const telemetry_t *)0))
    {
        g_log[g_log_len].tick = g_now_tick;
        g_log[g_log_len].kind = 2u;
        g_log[g_log_len].snap = *snap;
        g_log_len++;
    }
}

static void reset_log(void)
{
    g_log_len = 0u;
    g_now_tick = 0u;
}

/* Drive ticks: emulate timer ISR + main loop poll */
static void drive_ticks(uint32_t n_ticks)
{
    uint32_t i;
    for (i = 0u; i < n_ticks; i++)
    {
        g_now_tick = i + 1u;
        pulse_tick_isr();
        pulse_poll();
    }
}

/* ---------------- Tasks ---------------- */

static pulse_state_t task_sensor(pulse_state_t s)
{
    (void)s;

    tlm_write_begin();
    g_tlm.tick = g_now_tick;
    g_tlm.temp_c = (int16_t)(g_tlm.temp_c + 10);
    tlm_write_end();

    log_sensor();
    return 0;
}

static pulse_state_t task_battery(pulse_state_t s)
{
    (void)s;

    tlm_write_begin();
    g_tlm.tick = g_now_tick;
    g_tlm.vbat_mv = (uint16_t)(g_tlm.vbat_mv + 100);
    tlm_write_end();

    log_battery();
    return 0;
}

static pulse_state_t task_tx(pulse_state_t s)
{
    telemetry_t snap;

    (void)s;

    /* Inject a single interference during snapshot read in the first TX. */
    if (g_now_tick == 1u)
    {
        g_inject_once = 1u;
    }

    (void)memset(&snap, 0, sizeof(snap));
    assert(tlm_read_snapshot(&snap) != 0u);

    /* For a real radio downlink, snap would be serialized and sent. */
    log_tx(&snap);
    return 0;
}

/* ---------------- Test cases ---------------- */

static void test_telemetry_pipeline_basic(void)
{
    reset_log();

    /* Clear shared telemetry */
    g_seq = 0u;
    (void)memset(&g_tlm, 0, sizeof(g_tlm));

    pulse_init(1u);

    /* Periods chosen so all three run on tick 1, then sensor/batt keep updating.
     * Priority is by add order (index): sensor(0), batt(1), tx(2).
     */
    assert(pulse_add_task(0, 1u, task_sensor) == 0);
    assert(pulse_add_task(0, 1u, task_battery) == 0);
    assert(pulse_add_task(0, 1u, task_tx) == 0);

    drive_ticks(3u);

    /* Expect exactly 9 events: 3 tasks per tick for 3 ticks */
    assert(g_log_len == 9u);

    /* On each tick: sensor then battery then tx */
    assert(g_log[0].tick == 1u && g_log[0].kind == 0u);
    assert(g_log[1].tick == 1u && g_log[1].kind == 1u);
    assert(g_log[2].tick == 1u && g_log[2].kind == 2u);

    assert(g_log[3].tick == 2u && g_log[3].kind == 0u);
    assert(g_log[4].tick == 2u && g_log[4].kind == 1u);
    assert(g_log[5].tick == 2u && g_log[5].kind == 2u);

    assert(g_log[6].tick == 3u && g_log[6].kind == 0u);
    assert(g_log[7].tick == 3u && g_log[7].kind == 1u);
    assert(g_log[8].tick == 3u && g_log[8].kind == 2u);

    /* Validate that TX snapshots contain plausible updated values.
     * After tick1 sensor (+10) and batt (+100), tx should see at least those.
     * Note: g_inject_once causes an additional +1 temp_c update before the first
     * snapshot copy, so temp_c will be 11 (10 from sensor + 1 injected).
     */
    assert(g_log[2].snap.vbat_mv == 100u);
    assert(g_log[2].snap.temp_c == 11);

    /* Tick2: sensor +10 => 21, batt +100 => 200 */
    assert(g_log[5].snap.vbat_mv == 200u);
    assert(g_log[5].snap.temp_c == 21);

    /* Tick3: sensor +10 => 31, batt +100 => 300 */
    assert(g_log[8].snap.vbat_mv == 300u);
    assert(g_log[8].snap.temp_c == 31);
}

int main(void)
{
    test_telemetry_pipeline_basic();
     printf("All telemetry tests passed.\n");
    return 0;
}
