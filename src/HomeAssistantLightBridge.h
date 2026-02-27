#pragma once

#include <Arduino.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <HTTPClient.h>
#endif

// 1: enable Home Assistant light output
// 0: disable output logic
#define HOME_ASSISTANT_ENABLED 1

// Timing settings
#define HOME_ASSISTANT_UPDATE_INTERVAL_MS 120
#define HOME_ASSISTANT_WIFI_RETRY_MS 5000
#define HOME_ASSISTANT_CONNECT_TIMEOUT_MS 12000

static unsigned long haLastUpdateMs = 0;
static bool haLastSentColorValid = false;
static uint8_t haLastSentR = 0;
static uint8_t haLastSentG = 0;
static uint8_t haLastSentB = 0;
static unsigned long haLastWifiAttemptMs = 0;

static inline void homeAssistantEnsureWifiConnected()
{
#if HOME_ASSISTANT_ENABLED
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    unsigned long now = millis();
    if (now - haLastWifiAttemptMs < HOME_ASSISTANT_WIFI_RETRY_MS)
    {
        return;
    }

    WiFi.mode(WIFI_STA);
#ifdef ESP8266
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
#endif
    WiFi.begin(HOME_ASSISTANT_WIFI_SSID, HOME_ASSISTANT_WIFI_PASSWORD);
    haLastWifiAttemptMs = now;
#endif
}

static inline void homeAssistantConnectAtStartup()
{
#if HOME_ASSISTANT_ENABLED
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    WiFi.mode(WIFI_STA);
#ifdef ESP8266
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
#endif
    WiFi.begin(HOME_ASSISTANT_WIFI_SSID, HOME_ASSISTANT_WIFI_PASSWORD);

    unsigned long startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startedAt) < HOME_ASSISTANT_CONNECT_TIMEOUT_MS)
    {
        delay(150);
        yield();
    }

    haLastWifiAttemptMs = millis();
#endif
}

static inline bool homeAssistantPostColor(uint8_t r, uint8_t g, uint8_t b)
{
#if HOME_ASSISTANT_ENABLED
    homeAssistantEnsureWifiConnected();
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    bool isOff = (r == 0 && g == 0 && b == 0);
    String endpoint = String(HOME_ASSISTANT_BASE_URL) + "/api/services/light/" + (isOff ? "turn_off" : "turn_on");

    String payload;
    if (isOff)
    {
        payload = String("{\"entity_id\":\"") + HOME_ASSISTANT_LIGHT_ENTITY + "\"}";
    }
    else
    {
        uint8_t brightness = r;
        if (g > brightness)
        {
            brightness = g;
        }
        if (b > brightness)
        {
            brightness = b;
        }

        payload = String("{\"entity_id\":\"") + HOME_ASSISTANT_LIGHT_ENTITY +
                  "\",\"rgb_color\":[" + String(r) + "," + String(g) + "," + String(b) +
                  "],\"brightness\":" + String(brightness) + "}";
    }

    HTTPClient http;
#ifdef ESP8266
    WiFiClient client;
    if (!http.begin(client, endpoint))
    {
        return false;
    }
#else
    if (!http.begin(endpoint))
    {
        return false;
    }
#endif

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + HOME_ASSISTANT_API_TOKEN);

    int httpCode = http.POST(payload);
    http.end();

    if (httpCode > 0)
    {
        haLastSentR = r;
        haLastSentG = g;
        haLastSentB = b;
        haLastSentColorValid = true;
        haLastUpdateMs = millis();
        return true;
    }
#endif

    return false;
}

static inline bool homeAssistantApplyColorIfNeeded(uint8_t r, uint8_t g, uint8_t b)
{
#if HOME_ASSISTANT_ENABLED
    homeAssistantEnsureWifiConnected();

    if (haLastSentColorValid && r == haLastSentR && g == haLastSentG && b == haLastSentB)
    {
        return true;
    }

    unsigned long now = millis();
    if (now - haLastUpdateMs < HOME_ASSISTANT_UPDATE_INTERVAL_MS)
    {
        return false;
    }

    return homeAssistantPostColor(r, g, b);
#else
    (void)r;
    (void)g;
    (void)b;
    return true;
#endif
}

static inline void homeAssistantBegin()
{
    homeAssistantConnectAtStartup();
}

static inline void homeAssistantLoop()
{
    homeAssistantEnsureWifiConnected();
}
