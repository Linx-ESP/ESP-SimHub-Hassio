/**
 * Original implementation and testing by moriusz: https://github.com/moriusz
*/

#include <typeinfo>
#include <string>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <HTTPClient.h>
#endif

/****************************
 * 
 * Configuration Starts here
 * 
 ************************** */

#define LED_COUNT 1
#define RIGHTTOLEFT 0
#define TEST_MODE 1

// Enable Home Assistant output.
// 1: RGB data controls a Home Assistant light entity (virtual LED mode)
// 0: no output (keeps parsing data only)
#define HOME_ASSISTANT_ENABLED 1

// WiFi credentials used to reach Home Assistant
#define HOME_ASSISTANT_WIFI_SSID "Wifi"
#define HOME_ASSISTANT_WIFI_PASSWORD "WifiPassword"

// Home Assistant API settings
#define HOME_ASSISTANT_BASE_URL "http://192.168.1.2:8123"
#define HOME_ASSISTANT_LIGHT_ENTITY "light.whatever_entity_of_your_light"
#define HOME_ASSISTANT_API_TOKEN "longlivetoken"

// Limit API call rate (in milliseconds)
#define HOME_ASSISTANT_UPDATE_INTERVAL_MS 120

// Connection behavior
#define HOME_ASSISTANT_WIFI_RETRY_MS 5000
#define HOME_ASSISTANT_CONNECT_TIMEOUT_MS 12000

// LED BRIGHTNESS NANNY
//  Think about why you want to go higher than this?
//  is your power supply ready? are your eyes ready?, is your heat dissipation ready?
//  https://learn.adafruit.com/sipping-power-with-neopixels/insights
//  remember, if you don't have an external power supply, your board or USB may not be able
//  to provide enough power.
// luminance goes from 0-255, UPDATE AT YOUR OWN RISK
#define LUMINANCE_LIMIT 150


// The color order that your LED strip uses
// https://github.com/Makuna/NeoPixelBus/wiki/Neo-Features
#define colorSpec NeoGrbFeature // A three-element color in the order of Green, Red, and then Blue. This is used for SK6812(grb), WS2811, and WS2812.
//#define colorSpec NeoRgbFeature //A three-element color in the order of Red, Green, and then Blue. Some older pixels used this. 
//#define colorSpec NeoBgrFeature //A three-element color in the order of Blue, Red, and then Green.


// Identify your LED model or protocol
// Ws2812x << default for this library, no changes required; WS2812a, WS2812b, WS2812c, etc The most compatible
// Sk6812
// Apa106
// 400kbps << old slower speed standard that started this all
// .. or any of these but inverted.. example: Ws2812xInverted
//
// Then replace Ws2812x in the methods below with your LED Model/protocol


// We use different methods for each type of board based on available features and their limitations
#ifdef ESP32
//****** ESP32 ******
// There are more variations of the methods available in this file
//  if you find that these don't work for you, feel free to read more about these here, and be aware of
//  board specific limitations
// https://github.com/Makuna/NeoPixelBus/wiki/ESP32-NeoMethods


//******
// RMT
// little CPU Usage and low memory but many interrupts run for it and requires hardware buffer
// Supports all pins below GPIO34
//******
#if ( !CONFIG_IDF_TARGET_ESP32S3 ) // https://github.com/Makuna/NeoPixelBus/issues/815 (temporary)
#define method NeoEsp32Rmt0Ws2812xMethod
#endif

//******
// I2S
// little CPU Usage, more memory; Not available for S3 or C3 boards
// Supports any output pin
//******
#if ( !CONFIG_IDF_TARGET_ESP32S2 && !CONFIG_IDF_TARGET_ESP32C3 && !CONFIG_IDF_TARGET_ESP32S3 ) // not supported by these boards 
//#define method NeoEsp32I2s0X8Ws2812xMethod // Uses the I2S 0 peripheral in 8 channel parallel mode
//#define method NeoEsp32I2s0X16Ws2812xMethod // Uses the I2S 0 peripheral in 16 channel parallel mode
//#define method NeoEsp32I2s0Ws2812xMethod // Uses the I2S 0 peripheral
#endif


//******
// BitBang
// Uses a lot of CPU, and interrupts such as the ones ran for WiFi make it unstable.
// Supports all pins below GPIO32
//******
//#define method NeoEsp32BitBangWs2812xMethod


// Pick your GPIO pin based on the limitations of the selected method above
#define DATA_PIN 8

#else

//****** ESP8266 ******
// There are other methods, but We're picking the most convenient ones here.
//  Feel free to investigate the others, understand their drawbacks and use them if you want
// https://github.com/Makuna/NeoPixelBus/wiki/ESP8266-NeoMethods

// DMA (I2S)
// FASTEST, BUT only over WIFI AND you cannot receive serial data, only send
// Only GPIO3 (usually named as RX, RDX0)
//  this method requires that we initialize serial before the strip
#if CONNECTION_TYPE != SERIAL
// #define method NeoEsp8266DmaWs2812xMethod
#endif


// UART
// FASTER; 
// Only GPIO2 ("D4" in nodemcu, d1Mini and others, but verify)
#define method NeoEsp8266Uart1Ws2812xMethod
// -- There are other UART methods, that may or may not break serial, this is the safest.


// BitBang
// SLOWEST and least stable over WiFi; 
// pins 0-15 (raw gpio number, not the D{1}, D{2} numbers)
//#define method NeoEsp8266BitBangWs2812xMethod


// IF using DMA, this will be ignored and only GPIO3 will be used
// IF using UART, this will be ignored and only GPIO2 will be used
#define DATA_PIN 2
#endif

// Initial color to fill the strip before SimHub connects to the device 
// R, G, B format from 0-255.. 
//  Be aware that (255, 255, 255) may consume a lot of current
//  more than your device can provide, which can damage it. Start with lower numbers 
//  ex: (50, 0, 0) is red and (100, 0, 0) is still red, just brighter
//  See this: https://learn.adafruit.com/adafruit-neopixel-uberguide/powering-neopixels#estimating-power-requirements-2894486
//
// note: that this color is not limited by the luminance limit
struct StartupColor {
    uint8_t R;
    uint8_t G;
    uint8_t B;
};
StartupColor initialColor = {120, 0, 0};


/*************************
 * 
 * Configuration ends here
 * 
 ********************** */

struct VirtualRgbColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

VirtualRgbColor virtualLeds[LED_COUNT];
bool virtualLedsDirty = false;
unsigned long lastHomeAssistantUpdateMs = 0;
bool lastSentColorValid = false;
uint8_t lastSentR = 0;
uint8_t lastSentG = 0;
uint8_t lastSentB = 0;
unsigned long lastWifiAttemptMs = 0;

void setVirtualPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= LED_COUNT)
    {
        return;
    }

    if (virtualLeds[index].r == r && virtualLeds[index].g == g && virtualLeds[index].b == b)
    {
        return;
    }

    virtualLeds[index].r = r;
    virtualLeds[index].g = g;
    virtualLeds[index].b = b;
    virtualLedsDirty = true;
}

void homeAssistantEnsureWifiConnected()
{
#if HOME_ASSISTANT_ENABLED
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    unsigned long now = millis();
    if (now - lastWifiAttemptMs < HOME_ASSISTANT_WIFI_RETRY_MS)
    {
        return;
    }

    WiFi.mode(WIFI_STA);
#ifdef ESP8266
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
#endif
    WiFi.begin(HOME_ASSISTANT_WIFI_SSID, HOME_ASSISTANT_WIFI_PASSWORD);
    lastWifiAttemptMs = now;
#endif
}

void homeAssistantConnectAtStartup()
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

    lastWifiAttemptMs = millis();
#endif
}

void homeAssistantSendColor(uint8_t r, uint8_t g, uint8_t b)
{
#if HOME_ASSISTANT_ENABLED
    homeAssistantEnsureWifiConnected();
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
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
        return;
    }
#else
    if (!http.begin(endpoint))
    {
        return;
    }
#endif

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + HOME_ASSISTANT_API_TOKEN);

    int httpCode = http.POST(payload);
    http.end();

    if (httpCode > 0)
    {
        lastSentR = r;
        lastSentG = g;
        lastSentB = b;
        lastSentColorValid = true;
        virtualLedsDirty = false;
        lastHomeAssistantUpdateMs = millis();
    }
#endif
}

void homeAssistantFlushIfNeeded()
{
#if HOME_ASSISTANT_ENABLED
    homeAssistantEnsureWifiConnected();

    if (!virtualLedsDirty || LED_COUNT <= 0)
    {
        return;
    }

    unsigned long now = millis();
    if (now - lastHomeAssistantUpdateMs < HOME_ASSISTANT_UPDATE_INTERVAL_MS)
    {
        return;
    }

    uint32_t sumR = 0;
    uint32_t sumG = 0;
    uint32_t sumB = 0;

    for (uint16_t i = 0; i < LED_COUNT; i++)
    {
        sumR += virtualLeds[i].r;
        sumG += virtualLeds[i].g;
        sumB += virtualLeds[i].b;
    }

    uint8_t r = (uint8_t)(sumR / LED_COUNT);
    uint8_t g = (uint8_t)(sumG / LED_COUNT);
    uint8_t b = (uint8_t)(sumB / LED_COUNT);

    if (lastSentColorValid && r == lastSentR && g == lastSentG && b == lastSentB)
    {
        virtualLedsDirty = false;
        return;
    }

    homeAssistantSendColor(r, g, b);
#endif
}


/**
 * Initialization function: prepares the strip and other related things
 */
void neoPixelBusBegin()
{
    homeAssistantConnectAtStartup();

    for (int i = 0; i < LED_COUNT; i++)
    {
        virtualLeds[i].r = 0;
        virtualLeds[i].g = 0;
        virtualLeds[i].b = 0;
    }

    if (TEST_MODE)
    {
        for (int i = 0; i < LED_COUNT; i++)
        {
            setVirtualPixelColor(i, initialColor.R, initialColor.G, initialColor.B);
        }
        homeAssistantFlushIfNeeded();
    }
}

void neoPixelBusRead()
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint16_t b1;
    uint16_t b2;
    uint8_t j;
    int mode = 1;
    mode = FlowSerialTimedRead();
    while (mode > 0)
    {
        // Read all
        if (mode == 1)
        {
            for (j = 0; j < LED_COUNT; j++)
            {
                r = FlowSerialTimedRead();
                g = FlowSerialTimedRead();
                b = FlowSerialTimedRead();

                if (RIGHTTOLEFT == 1)
                {
                    setVirtualPixelColor(LED_COUNT - j - 1, r, g, b);
                }
                else
                {
                    setVirtualPixelColor(j, r, g, b);
                }
            }
        }

        // partial led data
        else if (mode == 2)
        {
            int startled = FlowSerialTimedRead();
            int numleds = FlowSerialTimedRead();

            for (j = startled; j < startled + numleds; j++)
            {
                r = FlowSerialTimedRead();
                g = FlowSerialTimedRead();
                b = FlowSerialTimedRead();

                if (RIGHTTOLEFT == 1)
                {
                    setVirtualPixelColor(LED_COUNT - j - 1, r, g, b);
                }
                else
                {
                    setVirtualPixelColor(j, r, g, b);
                }
            }
        }

        // repeated led data
        else if (mode == 3)
        {
            int startled = FlowSerialTimedRead();
            int numleds = FlowSerialTimedRead();

            r = FlowSerialTimedRead();
            g = FlowSerialTimedRead();
            b = FlowSerialTimedRead();

            for (j = startled; j < startled + numleds; j++)
            {
                if (RIGHTTOLEFT == 1)
                {
                    setVirtualPixelColor(LED_COUNT - j - 1, r, g, b);
                }
                else
                {
                    setVirtualPixelColor(j, r, g, b);
                }
            }
        }

        mode = FlowSerialTimedRead();
    }
}

void neoPixelBusShow() {
    homeAssistantEnsureWifiConnected();
    homeAssistantFlushIfNeeded();
}

int neoPixelBusCount() {
    return LED_COUNT;
}
