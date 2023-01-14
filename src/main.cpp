#include <Arduino.h>
#include <Wire.h>
#include "LED.h"
#include "BiosSettings.h"
#include "Buzzer.h"
#include "hal.h"
#include "iLoad.h"
#include "PetitFS.h"
#include "rtc.h"
#include "ToggleSwitch.h"

#define DEBUG

#define FW_VERSION "1.0"

// Global vars
ToggleSwitch runTrigger(PIN_RUN, NULL, NULL);
HAF_LED iosLED(PIN_IOS_LED, NULL);
bool hasIOExpander = false;
bool hasRTC = false;
bool hasExtendedRxBuf = false;
bool isSlowClock = false;
FATFS filesysSD;

void initSerial() {
	Serial.begin(SERIAL_BAUD_RATE);
	#ifdef DEBUG
	Serial.println(F("INIT: Boot stage 0."));
	#endif
}

void initCpuSuspended() {
	#ifdef DEBUG
	Serial.print(F("INIT: boot0 - Initializing CPU suspended... "));
	#endif
	// Configure RESET and set it ACTIVE.
	pinMode(PIN_RESET, OUTPUT);
	digitalWrite(PIN_RESET, LOW);

	// Configure WAIT_RES and set ACTIVE to reset the WAIT FF (U1C/D).
	pinMode(PIN_WAIT_RES, OUTPUT);
	digitalWrite(PIN_WAIT_RES, LOW);
	#ifdef DEBUG
	Serial.println(F("DONE"));
	#endif
}

void initRunTrigger() {
	pinMode(PIN_RUN, INPUT);
	#ifdef DEBUG
	Serial.println(F("INIT: boot0 - Waiting for boot signal from Southbridge..."));
	#endif
	bool gotSignal = false;
	unsigned long timestamp = millis();
	while (true) {
		gotSignal = (digitalRead(PIN_RUN) == HIGH);
		if (gotSignal && (millis() - timestamp) > 200) {
			break;
		}

		timestamp = millis();
	}
}

void bootStage0() {
	initSerial();
	initCpuSuspended();
	initRunTrigger();
}

void flushSerialRXBuffer() {
	while (Serial.available() > 0) {
		Serial.read();
	}
}

bool checkUserButton() {
	#ifdef DEBUG
	Serial.println(F("INIT: boot1 - Checking USER button..."));
	#endif
	pinMode(PIN_USER, INPUT_PULLUP);
	return (digitalRead(PIN_USER) == LOW);
}

void pulseClock(byte numPulse) {
	for (byte i = 0; i < numPulse; i++) {
		digitalWrite(PIN_CLK, HIGH);
		digitalWrite(PIN_CLK, LOW);
	}
}

void singlePulseResetZ80() {
	digitalWrite(PIN_RESET, LOW);
	pulseClock(6);
	digitalWrite(PIN_RESET, HIGH);
	pulseClock(2);
}

void initSystemControl() {
	#ifdef DEBUG
	Serial.print(F("INIT: boot1 - Initializing system control... "));
	#endif
	
	// Turn USER LED off.
	pinMode(PIN_USER, OUTPUT);
	digitalWrite(PIN_USER, HIGH);

	// Configure INT and set INACTIVE.
	pinMode(PIN_INT, INPUT_PULLUP);
	pinMode(PIN_INT, OUTPUT);
	digitalWrite(PIN_INT, HIGH);

	// Configure RAM_CE2 and set ACTIVE.
	pinMode(PIN_RAM_CE2, OUTPUT);
	digitalWrite(PIN_RAM_CE2, HIGH);

	// Configure WAIT as input.
	pinMode(PIN_WAIT, INPUT);

	// Configure BUSREQ.
	pinMode(PIN_BUSREQ, INPUT_PULLUP);
	pinMode(PIN_BUSREQ, OUTPUT);
	digitalWrite(PIN_BUSREQ, HIGH);
	#ifdef DEBUG
	Serial.println(F("DONE"));
	#endif
}

void bootStage1(bool *showBootMenu) {
	#ifdef DEBUG
	Serial.println(F("INIT: Boot stage 1."));
	Serial.println(F("INIT: boot1 - CyBorg is alive!\r\nCyBorg IOS - I/O Subsystem\r\n"));
	Serial.print(F("INIT: boot1 - Northbridge FW v"));
	Serial.println(FW_VERSION);
	Serial.print(F("INIT: boot1 - Compiled "));
	Serial.print(compDateStr);
	Serial.print(F(" "));
	Serial.println(compTimeStr);
	#endif
	if (SERIAL_RX_BUFFER_SIZE >= 128) {
		#ifdef DEBUG
		Serial.println(F("INIT: boot1 - IOS: Found extended serial RX buffer."));
		#endif
		hasExtendedRxBuf = true;
	}

	bool isPressed = checkUserButton();
	showBootMenu = &isPressed;
	initSystemControl();
}

void initDataBus() {
	#ifdef DEBUG
	Serial.print(F("INIT: boot2 - Initializing data bus... "));
	#endif

	// Configure Z80 Data bus (D0 - D7 [PA0 - PA7]) as input w/pullup
	DDRA = 0x00;
	PORTA = 0xFF;

	// Configure remaining bus control pins as input w/pullup.
	pinMode(PIN_MREQ, INPUT_PULLUP);
	pinMode(PIN_RD, INPUT_PULLUP);
	pinMode(PIN_WR, INPUT_PULLUP);
	pinMode(PIN_AD0, INPUT_PULLUP);
	#ifdef DEBUG
	Serial.println(F("DONE"));
	#endif
}

void initRAM() {
	#ifdef DEBUG
	Serial.print(F("INIT: boot2 - Initializing RAM... "));
	#endif
	// Configure logical RAM bank (32KB) to map into lower half of Z80 address space.
	pinMode(PIN_BANK0, OUTPUT);  // Set RAM logical bank 1 (OS bank 0)
	digitalWrite(PIN_BANK0, HIGH);
	pinMode(PIN_BANK1, OUTPUT);
	digitalWrite(PIN_BANK1, LOW);
	#ifdef DEBUG
	Serial.println(F("DONE"));
	#endif
}

void initCpuClock() {
	#ifdef DEBUG
	Serial.print(F("INIT: boot2 - Initializing CPU clock... "));
	#endif
	pinMode(PIN_CLK, OUTPUT);
	singlePulseResetZ80();
	#ifdef DEBUG
	Serial.println(F("DONE"));
	#endif
}

void bootStage2() {
	#ifdef DEBUG
	Serial.println(F("INIT: Boot stage 2."));
	#endif
	initDataBus();
	initRAM();
	initCpuClock();
}

void loadBiosSettings() {
	#ifdef DEBUG
	Serial.print(F("INIT: boot3 - Loading BIOS settings... "));
	#endif
	biosSettings_t.load();
	#ifdef DEBUG
	Serial.println(F("DONE"));
	#endif
	
	#ifdef DEBUG
	Serial.print(F("INIT: boot3 - IOS: CPU clock set at "));
	#endif
	isSlowClock = biosSettings_t.clockMode == ClockMode::SLOW;
	#ifdef DEBUG
	Serial.print(isSlowClock ? CLOCK_LOW : CLOCK_HIGH);
	Serial.println(F("MHz"));
	
	Serial.print(F("INIT: boot3 - IOS: CP/M Autoexec is "));
	Serial.println(biosSettings_t.autoExecFlag ? F("ON") : F("OFF"));
	#endif
}

void initBusExpander() {
	#ifdef DEBUG
	Serial.print(F("INIT: boot3 - Initializing I2C bus... "));
	#endif
	Wire.begin();
	Serial.println(F("DONE"));
	Wire.beginTransmission(GPIOEXP_ADDR);
	hasIOExpander = (Wire.endTransmission() == 0);
	#ifdef DEBUG
	if (hasIOExpander) {
		Serial.println(F("INIT: boot3 - IOS: Found I/O Expander."));
	}
	#endif
}

void bootStage3() {
	#ifdef DEBUG
	Serial.print(F("INIT: Boot stage 3."));
	#endif
	loadBiosSettings();
	initBusExpander();
	hasRTC = RTC.autoSet();
}

void bootStage4() {

}

void bootStage5() {

	// TODO Show summary screen?
	// TODO BIOS version, RTC presence, IOEXP presence, CPU speed, RAM, etc.
}

void setup() {
	bool showBootMenu = false;
	bootStage0();
	bootStage1(&showBootMenu);
	bootStage2();
	bootStage3();
}

void loop() {

}