# Pulse

Pulse is a minimal, deterministic scheduler for periodic tasks on resource-constrained microcontrollers. It is designed with a strong emphasis on analyzability, predictability, and safety-oriented software practices, making it well suited for PocketQube and CubeSat flight software as well as other deeply embedded systems.

Pulse is implemented as a small header-only C library. It avoids dynamic memory allocation, context switching, and execution of application code in interrupt context.


## Key characteristics

- Deterministic execution with explicit, bounded scheduling behavior
- Static configuration with task registration at startup
- No dynamic memory allocation and no heap usage
- Cooperative execution model: tasks run to completion in main context
- Interrupt safety: ISRs perform bookkeeping only
- Portable across small and larger microcontrollers

## Design overview

Pulse follows a two-phase execution model.

### Timekeeping phase (interrupt context)

A periodic hardware timer interrupt calls `pulse_tick_isr()`. This function increments per-task counters and marks tasks as ready when their configured period expires. The interrupt service routine performs only bounded bookkeeping and never executes application code.

### Execution phase (main context)

Ready tasks are executed cooperatively when `pulse_poll()` is called. Tasks run in priority order (lower task index first) and are not preempted by the scheduler.
This strict separation simplifies worst-case execution time analysis and reduces interrupt-related risks.

## Task model

Each task is defined by:

- a fixed execution period in ticks,
- an initial state value,
- a task function.

The task function has the following signature:
```c
pulse_state_t task_fn(pulse_state_t state);
```
The returned state value is stored by the scheduler and passed back to the task on its next execution. This enables simple state-machine-style implementations without dynamic memory allocation.

## Scheduler lifecycle

A typical integration follows these steps.

### Initialization
```c
pulse_init(tick_ms);
```
Initializes the scheduler and configures the system tick period.

### Task registration

```c
pulse_add_task(initial_state, period_ticks, task_fn);
```
Tasks are registered statically during system startup. No tasks are created or destroyed at runtime.

### Start and execution

There are two supported integration patterns.

Automatic polling:
```c
    pulse_start();
```
Manual polling:
```c
    /* timer ISR calls pulse_tick_isr() */
    for (;;)
    {
        pulse_poll();
        /* idle or sleep */
    }
```
Both patterns preserve the same execution semantics.

## Telemetry example (PocketQube-style)

A common PocketQube workload is periodic telemetry downlink composed of data from multiple subsystems such as sensors, power management, and system status.
Pulse supports this pattern without explicit IPC primitives by using shared data structures combined with simple consistency mechanisms.
A typical design uses:

1. acquisition tasks that periodically update shared telemetry data,
2. a packaging task that reads a consistent snapshot and builds a telemetry frame,
3. a transmission task that sends the frame to Earth via the radio subsystem.

Consistency can be ensured using timestamps or sequence counters, avoiding long critical sections and working reliably even on 8-bit microcontrollers.
A complete working example of this pattern is provided in the hosted unit test: [test/test_telemetry.c](test/test_telemetry.c)

## Data sharing and IPC-like patterns

Pulse intentionally does not provide traditional IPC primitives such as queues, mailboxes, or semaphores. Tasks do not run concurrently, and the only source of concurrency is the timer interrupt.

Instead, Pulse encourages explicit data-sharing patterns that are easy to analyze and verify:

- shared snapshot structures with timestamps or sequence counters,
- seqlock-style consistency checks,
- small single-producer or single-consumer ring buffers.

These approaches are sufficient for common workloads such as telemetry generation and downlink, and they reduce hidden complexity compared to blocking synchronization primitives.

## Portability and scalability

All hardware-specific functionality is isolated behind a small set of port macros that define interrupt control, critical sections, and timer initialization.

This design allows Pulse to run unchanged on:

- small 8-bit and 16-bit microcontrollers such as AVR and MSP430,
- more capable platforms such as ARM Cortex-M and RISC-V.

On larger microcontrollers, Pulse can coexist with DMA, peripheral interrupts, and low-power modes while still providing a deterministic scheduling backbone for periodic control and housekeeping tasks.

## Safety-oriented design

Pulse is written to align with MISRA C guidance and conservative C style practices commonly used in safety- and mission-critical software.
Key properties include:

- no dynamic allocation,
- no recursion,
- explicit-width integer types,
- bounded control flow,
- strict separation between interrupt-level and application-level code.

These properties simplify static analysis, code review, and long-term maintenance.

## Design rationale and FAQ

For a detailed discussion of design decisions, trade-offs, and frequently asked questions, see: [docs/design_qa.md](docs/design_qa.md)

## Limitations

Pulse intentionally omits preemptive multitasking, dynamic task management, and built-in IPC primitives. These omissions are deliberate and reflect the scheduler’s focus on simplicity, predictability, and analyzability rather than general-purpose concurrency.

## License

MIT

## Citation

to cite this project:
```bibtex
@software{Pulse2026,
  author = {Oliveira, Paolo},
  doi = {0.0/y.z},
  month = {1},
  title = {{Pulse: A tiny cooperative real-time scheduler for microcontrollers}},
  url = {https://github.com/paoloo/pulse},
  version = {0.1.0},
  year = {2026}
}
```
