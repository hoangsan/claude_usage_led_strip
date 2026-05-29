// UsageClient: HTTP GET against the local API server, parses the minimal
// {utilization, remaining_minutes} payload into a Reading.
//
// On the device (Arduino-ESP32) this depends on WiFi + HTTPClient. The native
// unit-test build does NOT pull in those headers; only the Reading struct
// declared here is shared.
#pragma once

#include <stdint.h>

struct Reading {
    float    utilization;        // percent, may exceed 100
    uint32_t remaining_minutes;  // whole minutes, >= 0
};

#ifndef UNIT_TEST

#include <Arduino.h>

namespace UsageClient {

// Performs an HTTP GET against API_ENDPOINT with a 12 s timeout, parses the
// JSON body, and validates per data-model.md §2. Returns true on success and
// populates `out`. On any failure (network, non-2xx, deserialize, missing
// field, type mismatch, out-of-range) returns false and writes a short
// human-readable reason into `errOut`.
bool fetch(Reading& out, String& errOut);

}  // namespace UsageClient

#endif  // !UNIT_TEST
