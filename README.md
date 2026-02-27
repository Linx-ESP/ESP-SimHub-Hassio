# This is a fork for a feature, you probably want to check upstream first

> [!WARNING]
>  The changes have been made with an AI/LLM. I wish I knew how to code it  
>  That said, the proper way would be with a SimHub plugin and not... (gestures broadly) ...this

> But at least this awful readme has been fully written by me
  
Allows to treat a light from Home Assistant as an LED on SimHub (via the arduino ui)  
Currently it is **only for one** light/light group (haven't tested light groups).  

--- 

Same as the rest of features, your configuration will be made at ``main.cpp``.  

### Uncomment: 
``` c
#define INCLUDE_RGB_LEDS_HOMEASSISTANT
```
### Configure:
``` c
// Home Assistant bridge configuration
#define HOME_ASSISTANT_WIFI_SSID "Wifi" // Your Wifi SSID
#define HOME_ASSISTANT_WIFI_PASSWORD "WifiPassword"  // Your Wifi password
#define HOME_ASSISTANT_BASE_URL "http://192.168.1.2:8123" // Home assistant's IP+port or domain
#define HOME_ASSISTANT_LIGHT_ENTITY "light.whatever_entity_of_your_light"  // How to get explained later
#define HOME_ASSISTANT_API_TOKEN "longlivetoken" // Long-lived access tokens can be created using the "Long-Lived Access Tokens" section at the bottom of a user's Home Assistant profile page.
```  

## How to get the light entity id

- Home Assistant > Settings > Devices and services > **ENTITIES**  
- Search for your light (group). Has to be the light entity, some bulbs might show entities for stuff like signal strength.  
- Clicking should open the light controls.  
- Top right corner > Cog > copy the entity ID  

## Test mode / red light
``` c
#define HOMEASSISTANT_TESTMODE 1
```
If enabled the light will change to `rgb(120, 0, 0)` [dim red] when connected to Home assistant to validate.  
