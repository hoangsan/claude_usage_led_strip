#ifndef UNIT_TEST

#include "UsageClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "../include/config.h"

namespace UsageClient {

namespace {
constexpr uint16_t kClientTimeoutMs = 5000;
constexpr size_t   kJsonCapacity    = 256;
}  // namespace

bool fetch(Reading& out, String& errOut) {
    if (WiFi.status() != WL_CONNECTED) {
        errOut = "wifi not connected";
        return false;
    }

    HTTPClient http;
    http.setTimeout(kClientTimeoutMs);
    http.setConnectTimeout(kClientTimeoutMs);

    if (!http.begin(API_ENDPOINT)) {
        errOut = "http begin failed";
        return false;
    }

    const int status = http.GET();
    if (status < 200 || status >= 300) {
        errOut = String("http status ") + status;
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    doc.shrinkToFit();
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        errOut = String("json: ") + err.c_str();
        return false;
    }

    if (!doc["utilization"].is<float>() && !doc["utilization"].is<double>() &&
        !doc["utilization"].is<int>()) {
        errOut = "utilization missing/wrong type";
        return false;
    }
    if (!doc["remaining_minutes"].is<int>() &&
        !doc["remaining_minutes"].is<unsigned int>() &&
        !doc["remaining_minutes"].is<long>() &&
        !doc["remaining_minutes"].is<unsigned long>()) {
        errOut = "remaining_minutes missing/wrong type";
        return false;
    }

    const float util  = doc["utilization"].as<float>();
    const long  remLg = doc["remaining_minutes"].as<long>();
    if (!(util >= 0.0f)) {
        errOut = "utilization < 0";
        return false;
    }
    if (remLg < 0) {
        errOut = "remaining_minutes < 0";
        return false;
    }

    out.utilization       = util;
    out.remaining_minutes = static_cast<uint32_t>(remLg);
    return true;
}

}  // namespace UsageClient

#endif  // !UNIT_TEST
