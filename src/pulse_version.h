/*
 * Copyright (c) 2026 Paolo Oliveira. All rights reserved.
 * Licensed under the MIT License. See LICENSE file in the project root for details. 
 * pulse_version.h - Pulse versioning
 */

#ifndef PULSE_VERSION_H
#define PULSE_VERSION_H

/* Semantic-ish versioning */
#define PULSE_VERSION_MAJOR (0u)
#define PULSE_VERSION_MINOR (1u)
#define PULSE_VERSION_PATCH (0u)

/* Optional: packed integer version for comparisons */
#define PULSE_VERSION_PACKED \
    ((uint32_t)(PULSE_VERSION_MAJOR * 10000u) + \
     (uint32_t)(PULSE_VERSION_MINOR * 100u) + \
     (uint32_t)(PULSE_VERSION_PATCH))

#endif /* PULSE_VERSION_H */
