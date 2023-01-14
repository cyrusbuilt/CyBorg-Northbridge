#include "hal.h"

void printBinaryByte(byte value) {
	for (byte mask = 0x80; mask; mask >>= 1) {
		Serial.print((mask & value) ? '1' : '0');
	}
}

void serialEvent(bool intFlagUsed) {
	if ((Serial.available()) && intFlagUsed) {
		digitalWrite(PIN_INT, LOW);
	}
}

void blinkIOSled(unsigned long *timestamp) {
	if ((millis() - *timestamp) > 200) {
		digitalWrite(PIN_IOS_LED, !digitalRead(PIN_IOS_LED));
		*timestamp = millis();
	}
}