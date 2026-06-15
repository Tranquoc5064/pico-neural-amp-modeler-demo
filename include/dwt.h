// DWT cycle counter helpers for Cortex-M33 (RP2350).
//
// The DWT (Data Watchpoint & Trace) CYCCNT register is the cheapest way to get
// cycle-accurate timing on the device. It is part of the ARMv8-M debug
// architecture and is available on the RP2350's M33 cores. We reach it through
// the SDK's CMSIS stub (RP2350.h pulls in core_cm33.h), so DWT / CoreDebug are
// the standard CMSIS structs.
#pragma once

#include <stdint.h>
#include "RP2350.h"

// Enable the cycle counter. Must run once per core that will read CYCCNT.
static inline void dwt_init(void) {
    // Global trace enable (gates the whole DWT block).
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

#ifdef DWT_LAR_KEY
    // Some ARMv8-M implementations require unlocking software access to DWT.
    // Harmless where not needed.
    DWT->LAR = 0xC5ACCE55u;
#endif

    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline void dwt_reset(void) { DWT->CYCCNT = 0; }

static inline uint32_t dwt_cyccnt(void) { return DWT->CYCCNT; }
