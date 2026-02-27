#ifndef __SHRGBLEDSHOMEASSISTANT_H__
#define __SHRGBLEDSHOMEASSISTANT_H__

#include <Arduino.h>
#include "SHRGBLedsBase.h"
#include "HomeAssistantLightBridge.h"

class SHRGBLedsHomeAssistant : public SHRGBLedsBase {
private:
	uint32_t _sumR = 0;
	uint32_t _sumG = 0;
	uint32_t _sumB = 0;
	uint16_t _writtenCount = 0;

public:
	void begin(int maxLeds, int righttoleft, bool testMode) {
		SHRGBLedsBase::begin(maxLeds, righttoleft);
		homeAssistantBegin();

		if (testMode > 0 && maxLeds > 0) {
			homeAssistantApplyColorIfNeeded(120, 0, 0);
		}
	}

	void show() {
		homeAssistantLoop();
		if (_maxLeds <= 0 || _writtenCount == 0) {
			_sumR = 0;
			_sumG = 0;
			_sumB = 0;
			_writtenCount = 0;
			return;
		}

		uint8_t r = (uint8_t)(_sumR / _writtenCount);
		uint8_t g = (uint8_t)(_sumG / _writtenCount);
		uint8_t b = (uint8_t)(_sumB / _writtenCount);

		homeAssistantApplyColorIfNeeded(r, g, b);

		_sumR = 0;
		_sumG = 0;
		_sumB = 0;
		_writtenCount = 0;
	}

protected:
	void setPixelColor(uint8_t lednumber, uint8_t r, uint8_t g, uint8_t b) {
		if (lednumber >= _maxLeds) {
			return;
		}

		if (_writtenCount == 0) {
			_sumR = 0;
			_sumG = 0;
			_sumB = 0;
		}

		_sumR += r;
		_sumG += g;
		_sumB += b;
		_writtenCount++;

		if (_writtenCount >= _maxLeds) {
			_writtenCount = _maxLeds;
		}
	}
};

#endif
