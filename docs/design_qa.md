# Design Q&A and Rationale

This document explains the key design decisions behind Pulse and addresses common questions that arise when comparing it to full-featured RTOSs or traditional superloop architectures.

The intent is to clarify *why* Pulse looks the way it does, not to justify missing features.

## Is Pulse a full RTOS?

No. Pulse is a minimal, deterministic scheduler for periodic tasks.

Pulse intentionally omits features such as preemption, dynamic task creation,
and built-in inter-process communication. These omissions are deliberate design
choices aimed at improving analyzability, predictability, and ease of
verification on resource-constrained systems.

## How is Pulse different from a simple superloop?

Pulse formalizes the superloop pattern by introducing:

- explicit periodic scheduling semantics,
- a well-defined task lifecycle,
- strict separation between interrupt-time bookkeeping and application-level
  execution.

Unlike ad hoc superloops, Pulse makes timing behavior explicit and bounded, which simplifies testing and worst-case execution time analysis as systems grow in complexity.

## Why does Pulse avoid running application code in interrupt context?

Executing application logic in interrupt service routines complicates timing analysis and increases the risk of overruns and priority inversion.

Pulse restricts interrupt handlers to bounded bookkeeping only. All task execution occurs in main context, making system behavior easier to reason about and align with conservative safety-oriented coding practices.

## Why does Pulse not provide IPC primitives such as queues or semaphores?

Pulse uses a cooperative execution model where tasks do not run concurrently. As a result, many traditional IPC primitives are unnecessary. Instead, Pulse encourages explicit data-sharing patterns such as:

- shared snapshot structures with timestamps or sequence counters,
- seqlock-style consistency checks,
- simple single-producer or single-consumer ring buffers.

These patterns are sufficient for common workloads, such as telemetry generation and downlink, and are easier to analyze and verify on small microcontrollers.

## How is telemetry data typically handled?

A common pattern in Pulse-based systems is:

1. acquisition tasks periodically update shared telemetry data,
2. a packaging task reads a consistent snapshot and assembles a telemetry
   frame,
3. a transmission task sends the frame to Earth via the radio subsystem.

Consistency is ensured using timestamps or sequence counters rather than blocking synchronization. This approach avoids long critical sections and works reliably even on 8-bit microcontrollers.

## Why not use FreeRTOS, Zephyr, or RTEMS?

Full-featured RTOSs are excellent tools, but they introduce additional complexity, resource usage, and assurance overhead that may be unnecessary for small satellite missions and deeply embedded systems. Pulse targets scenarios where:
- the workload is well understood,
- the number of tasks is small and static,
- predictability and auditability are higher priorities than throughput.

In such cases, a minimal scheduler can reduce integration risk while still providing sufficient structure.

## Can Pulse scale to more powerful microcontrollers?

Yes.

Pulse is designed to be hardware-agnostic. All platform-specific behavior is isolated behind small port interfaces. This allows Pulse to run unchanged on:

- small 8-bit and 16-bit microcontrollers (e.g., AVR, MSP430),
- more capable platforms such as ARM Cortex-M and RISC-V.

On larger systems, Pulse can coexist with DMA, peripheral interrupts, and low-power modes while providing a deterministic scheduling backbone for periodic tasks.

## Is Pulse suitable for production use?

Pulse is intentionally small and conservative, which makes it suitable for production use in systems with well-defined requirements.

Its static configuration, bounded execution, and avoidance of dynamic behavior support rigorous testing, code review, and long-term maintenance in mission-oriented environments.

## What are the main limitations of Pulse?

Pulse intentionally omits:

- preemptive multitasking,
- dynamic task management,
- built-in IPC primitives.

Systems requiring complex synchronization, dynamic workloads, or high throughput may be better served by a full RTOS. These limitations reflect Pulse’s design goals rather than incomplete implementation.
