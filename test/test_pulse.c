/* test_pulse.c - Hosted unit tests for pulse.h (GCC) */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../src/pulse_port_host.h"
#include "../src/pulse_version.h"

#define PULSE_IMPLEMENTATION
#define PULSE_MAX_TASKS (8u)
#include "../src/pulse.h"

/* ---------------- Test logging ---------------- */

typedef struct
{
    uint32_t tick;
    uint8_t  task_id;
} exec_event_t;

static exec_event_t g_log[128];
static uint32_t g_log_len = 0u;
static uint32_t g_now_tick = 0u;

_Static_assert(PULSE_VERSION_MAJOR == 0u, "Unexpected major version");

static void log_exec(uint8_t task_id)
{
    if (g_log_len < (uint32_t)(sizeof(g_log) / sizeof(g_log[0])))
    {
        g_log[g_log_len].tick = g_now_tick;
        g_log[g_log_len].task_id = task_id;
        g_log_len++;
    }
}

static void reset_log(void)
{
    g_log_len = 0u;
    g_now_tick = 0u;
}

static void drive_ticks(uint32_t n_ticks)
{
    uint32_t i;
    for (i = 0u; i < n_ticks; i++)
    {
        g_now_tick = i + 1u;
        pulse_tick_isr();

        /* New behavior: ISR marks ready; main context executes via poll. */
        pulse_poll();
    }
}

static pulse_state_t task0(pulse_state_t s)
{
    (void)s;
    log_exec(0u);
    return 0;
}

static pulse_state_t task1(pulse_state_t s)
{
    (void)s;
    log_exec(1u);
    return 0;
}

static pulse_state_t task2(pulse_state_t s)
{
    (void)s;
    log_exec(2u);
    return 0;
}

static void expect_event(uint32_t idx, uint32_t tick, uint8_t task_id)
{
    assert(idx < g_log_len);
    assert(g_log[idx].tick == tick);
    assert(g_log[idx].task_id == task_id);
}

static void test_same_tick_priority_order(void)
{
    reset_log();

    pulse_init(1u);

    assert(pulse_add_task(0, 5u, task0) == 0);
    assert(pulse_add_task(0, 5u, task1) == 0);

    drive_ticks(1u);

    assert(g_log_len == 2u);
    expect_event(0u, 1u, 0u);
    expect_event(1u, 1u, 1u);
}

static void test_period_timing(void)
{
    reset_log();

    pulse_init(1u);

    assert(pulse_add_task(0, 2u, task0) == 0);
    assert(pulse_add_task(0, 3u, task1) == 0);

    drive_ticks(6u);

    assert(g_log_len == 5u);

    expect_event(0u, 1u, 0u);
    expect_event(1u, 1u, 1u);
    expect_event(2u, 3u, 0u);
    expect_event(3u, 4u, 1u);
    expect_event(4u, 5u, 0u);
}

static void test_three_tasks_staggered(void)
{
    reset_log();

    pulse_init(1u);

    assert(pulse_add_task(0, 4u, task0) == 0);
    assert(pulse_add_task(0, 2u, task1) == 0);
    assert(pulse_add_task(0, 6u, task2) == 0);

    drive_ticks(5u);

    assert(g_log_len == 6u);

    expect_event(0u, 1u, 0u);
    expect_event(1u, 1u, 1u);
    expect_event(2u, 1u, 2u);
    expect_event(3u, 3u, 1u);
    expect_event(4u, 5u, 0u);
    expect_event(5u, 5u, 1u);
}

int main(void)
{
    test_same_tick_priority_order();
    test_period_timing();
    test_three_tasks_staggered();

    printf("All Pulse tests passed.\n");
    return 0;
}
