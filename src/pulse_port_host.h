/*
 * Copyright (c) 2026 Paolo Oliveira. All rights reserved.
 * Licensed under the MIT License. See LICENSE file in the project root for details.
 * pulse_port_host.h - Hosted (GCC) port for pulse.h unit tests
 */

#ifndef PULSE_PORT_HOST_H
#define PULSE_PORT_HOST_H

#include <stdint.h>

#define PULSE_PORT_DISABLE_GLOBAL_IRQ() do { } while (0)
#define PULSE_PORT_ENABLE_GLOBAL_IRQ()  do { } while (0)

#define PULSE_PORT_ENTER_CRITICAL()     do { } while (0)
#define PULSE_PORT_EXIT_CRITICAL()      do { } while (0)

#define PULSE_PORT_TIMER_INIT(tick_ms)  do { (void)(tick_ms); } while (0)

#ifndef PULSE_PORT_IDLE_HOOK
#define PULSE_PORT_IDLE_HOOK()          do { } while (0)
#endif

#endif /* PULSE_PORT_HOST_H */
