---
title: "Pulse: A Minimal Deterministic Scheduler for Resource-Constrained PocketQube Satellites"
tags:
  - embedded-systems
  - real-time-systems
  - cubesat
  - pocketqube
  - safety-critical
  - rtos
authors:
  - name: João Paolo Cavalcante Martins Oliveira
    orcid: 0000-0003-4117-953X
    affiliation: 1
affiliations:
  - name: University of Fortaleza, Brazil
    index: 1
date: 2026-01-07
bibliography: pulse_refs.bib
---

## Summary

PocketQube-class satellites operate under strict constraints in memory, CPU time, power, and software complexity. In these environments, a full-featured RTOS can be unnecessary overhead, while a bare-metal superloop can become difficult to scale and analyze as mission logic grows.

**Pulse** is a minimal, deterministic scheduler for periodic tasks on resource-constrained microcontrollers. It uses static configuration, avoids dynamic allocation and context switching, and enforces a strict separation between interrupt-time bookkeeping and application-level execution. Pulse is implemented as a small header-only C library and is intended for microcontrollers commonly used in PocketQube and CubeSat missions.

## Statement of need

Small satellite flight software benefits from strong analyzability, predictable timing behavior, and minimal integration risk. Mature RTOSs such as FreeRTOS and RTEMS provide extensive functionality, but at the cost of increased kernel complexity and a larger software assurance surface area [@freertos_mastering_pdf_2016; @rtems_site]. At the opposite end of the spectrum, superloop-based designs are widely used but often rely on implicit timing assumptions that become fragile as systems evolve [@barr_embedded_c_1999].

Educational minimal schedulers such as RIOS demonstrate that useful periodic scheduling can be achieved with a small codebase [@vahid_rios_site; @miller_rios_2012]. However, minimal patterns frequently seen in practice may execute application logic in interrupt context, complicating worst-case execution time (WCET) analysis and increasing the risk of ISR overruns.

**Pulse** addresses this gap by providing a scheduler that preserves the simplicity expected in deeply constrained systems, while explicitly enforcing that application code does **not** execute in interrupt context. This design choice improves timing robustness, simplifies testing, and supports mission-critical software assurance activities.

## State of the field

A wide range of software frameworks exists for task scheduling in embedded systems. Full-featured real-time operating systems such as FreeRTOS and RTEMS provide preemptive multitasking, dynamic task management, and rich inter-process communication primitives. These systems are well suited for complex applications, but their size and behavioral complexity can be a drawback in environments with tight resource and assurance constraints.

At the other end of the spectrum, many embedded systems rely on ad hoc superloop architectures. While simple, such designs often lack explicit scheduling semantics and can become difficult to analyze and maintain as system complexity grows.

Minimal schedulers such as RIOS demonstrate that useful periodic scheduling can be achieved with a small codebase. However, many minimal approaches execute application logic directly in interrupt context or rely on implicit timing assumptions, complicating worst-case execution time analysis.

Pulse occupies a middle ground between these approaches. It offers explicit periodic scheduling semantics and a well-defined task lifecycle, while remaining small enough to be fully audited. Its strict separation between interrupt-time bookkeeping and application-level execution distinguishes it from both ad hoc superloops and many minimal schedulers, providing a clear build-versus-contribute justification.

## Software Design

The design of **Pulse** is driven by the needs of resource-constrained and safety-oriented embedded systems, where predictability and analyzability are more important than maximizing concurrency or throughput. Several deliberate trade-offs shape the architecture of the scheduler.

**Pulse** uses cooperative rather than preemptive task execution. This choice avoids the complexity of context switching and priority inversion, and makes control flow easier to analyze. Application code is never executed in interrupt context; interrupt handlers are restricted to bounded bookkeeping operations. This separation simplifies worst-case execution time analysis and reduces the risk of hard-to-debug timing faults.

Tasks are registered statically at system startup and are not created or destroyed at runtime. This static configuration eliminates dynamic memory usage and enables the entire scheduling behavior to be reasoned about at compile time.

**Pulse** follows a deliberately simple lifecycle that separates configuration, timekeeping, and execution.

### Initialization and configuration

The scheduler is initialized once at system startup by calling `pulse_init()`, which configures the system tick period and clears all internal state. Tasks are registered statically during initialization using `pulse_add_task()`. No tasks are created or destroyed at runtime.

Each task is defined by: a fixed execution period (in ticks), an initial state value, and a task function.

The task function has the following signature: `pulse_state_t task_fn(pulse_state_t state);`.

The returned state value is stored by the scheduler and passed back to the task on its next invocation, enabling simple state-machine-style task implementations without dynamic memory.

### Scheduler lifecycle

After all tasks are registered, the scheduler is started by calling `pulse_start()` or, alternatively, by calling `pulse_tick_isr()` from a hardware timer interrupt and invoking `pulse_poll()` manually from the main loop.

Pulse execution is divided into two phases:

1. **Timekeeping phase (interrupt context)**
   A periodic timer interrupt invokes `pulse_tick_isr()`. This function updates per-task counters and marks tasks as ready when their configured period expires. The ISR performs only bounded bookkeeping and never executes application code.

2. **Execution phase (main context)**
   Ready tasks are executed cooperatively when `pulse_poll()` is called. Tasks run to completion in priority order (lower task index first). Because tasks are not preempted by the scheduler, application code runs in a well-defined and analyzable context.

This lifecycle makes the system behavior explicit and easy to reason about, even on very small microcontrollers.

## Determinism, WCET, and footprint

**Pulse** is structured so that all time-critical operations are bounded and visible.

The timer interrupt handler runs in `O(N)` time per tick, where `N` is the number of configured tasks. Task dispatch and execution occur exclusively in main context. The worst-case scheduler overhead per polling cycle is bounded by the number of ready tasks and the cost of selecting the next ready task.

Memory usage is entirely static and determined at compile time by the maximum number of tasks. Pulse uses a fixed-size task table and a small readiness bitmask. In a hosted reference build with 16 tasks and size optimization enabled, the kernel footprint is on the order of a few hundred bytes of text and static RAM. On typical 8-bit and 16-bit microcontrollers, the footprint is generally smaller due to reduced pointer widths and alignment requirements.

## Portability and scalability

**Pulse** is designed to be portable across a wide range of microcontrollers. All hardware-specific functionality is isolated behind a small set of port macros that define:
- interrupt enable/disable primitives,
- critical section handling, and
- timer initialization.

This structure allows **Pulse** to be used unchanged on small 8-bit and 16-bit microcontrollers (such as AVR and MSP430) as well as on more capable platforms, including ARM Cortex-M and RISC-V microcontrollers. On more powerful devices, Pulse can coexist with DMA, peripheral interrupts, or low-power modes, while still providing a deterministic scheduling backbone for periodic control and housekeeping tasks.

Although **Pulse** does not attempt to scale into a full preemptive RTOS, its static and explicit design makes it suitable as a foundational scheduling layer even on larger systems where predictability and auditability are prioritized over maximum throughput.

## Data sharing and IPC-like patterns for telemetry

**Pulse** intentionally does not provide traditional inter-process communication (IPC) primitives such as queues, mailboxes, or semaphores. This omission reflects the cooperative execution model: tasks do not run concurrently, and the only source of concurrency is the timer interrupt.

In practice, this simplifies data sharing for common satellite workloads such as telemetry generation and downlink. Instead of message passing, Pulse encourages explicit data-sharing patterns that are easier to analyze and verify.

A common telemetry architecture consists of:
- acquisition tasks that read sensors or power system data,
- a packaging task that assembles telemetry frames, and
- a transmission task that sends data to Earth via the radio subsystem.

Data transfer between these tasks can be implemented using shared structures protected by simple consistency mechanisms, such as:
- timestamped fields,
- sequence counters (seqlock-style snapshots), or
- small single-producer/single-consumer ring buffers.

For example, sensor and power tasks may periodically update a shared telemetry snapshot structure, incrementing a sequence counter before and after updates. The telemetry packaging task reads a consistent snapshot by verifying that the sequence counter did not change during the copy. This pattern avoids long critical sections, works on 8-bit architectures, and provides a clear notion of data freshness—an important property for downlinking telemetry to Earth.

These IPC-like patterns are sufficient for many PocketQube missions and align well with Pulse’s design goals: explicit data flow, bounded execution, and minimal hidden complexity.

## Software engineering and safety considerations

**Pulse** is written to align with MISRA-C guidance and conservative C style practices commonly used in safety- and mission-critical software. In particular, it avoids dynamic allocation and recursion, uses explicit-width integer types, and keeps control flow simple and bounded.

The strict separation between interrupt-level bookkeeping and application-level execution aligns with guidance from the NASA C Style Guide, reducing the risk of hard-to-analyze timing interactions and improving long-term maintainability [@nasa_c_style_1995].

## Limitations

**Pulse** intentionally omits preemption, dynamic task management, and built-in IPC mechanisms. These omissions are deliberate and reflect the scheduler’s focus on simplicity, predictability, and analyzability rather than general-purpose multitasking.

## Research impact Statement

Pulse is released as open-source software with comprehensive documentation, design rationale, and reproducible hosted tests. While the project is in its early stages of dissemination, it targets a clearly defined and active research and engineering community working on small satellites and safety-oriented embedded
systems.

The software includes unit tests that validate scheduling behavior and telemetry-oriented data-sharing patterns, supporting reproducible experimentation and evaluation. The explicit design documentation and conservative coding style make Pulse suitable as a reference implementation for research, education, and future mission prototypes.

Given the increasing interest in PocketQube and other low-cost satellite platforms, Pulse provides a practical foundation for near-term use in academic projects, technology demonstrators, and teaching environments focused on deterministic embedded systems.

## Conclusion

**Pulse** demonstrates that a carefully designed minimal scheduler can meet the needs of resource-constrained and safety-oriented embedded systems without the complexity of a full RTOS. By emphasizing determinism, static configuration, and explicit data flow, Pulse provides a practical foundation for PocketQube and similar small-satellite missions.

## References
