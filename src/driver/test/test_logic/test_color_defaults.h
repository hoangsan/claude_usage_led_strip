// Force-included via -include in [env:native] build_flags. Provides the three
// quota-color macros that Animations.cpp expects, plus the CRGB type that
// those macros instantiate. Pulled into every C++ TU in the native test build
// so the production code and the assertions share identical constants.
//
// The header guards itself against being pulled into pure-C translation
// units (Unity's unity_config.c is C, and our CRGB shim is C++ only).
#pragma once

#ifdef __cplusplus

#include "mocks/CRGB.h"

#ifndef NORMAL_QUOTA_COLOR
#define NORMAL_QUOTA_COLOR     CRGB(0x00, 0xCC, 0x00)
#endif
#ifndef WARN_QUOTA_COLOR
#define WARN_QUOTA_COLOR       CRGB(0xFF, 0xA5, 0x00)
#endif
#ifndef EXHAUSTED_QUOTA_COLOR
#define EXHAUSTED_QUOTA_COLOR  CRGB(0xCC, 0x00, 0x00)
#endif
#ifndef ERROR_COLOR
#define ERROR_COLOR            CRGB(0xCC, 0x00, 0x00)
#endif
#ifndef STARTUP_COLOR
#define STARTUP_COLOR          CRGB(0xFF, 0xFF, 0xFF)
#endif
#ifndef WARN_THRESHOLD_PERCENT
#define WARN_THRESHOLD_PERCENT       70
#endif
#ifndef EXHAUSTED_THRESHOLD_PERCENT
#define EXHAUSTED_THRESHOLD_PERCENT  90
#endif

#endif  // __cplusplus
