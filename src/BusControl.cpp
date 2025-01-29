#include <Wire.h>
#include "BusControl.h"
#include "hal.h"

// BUSCTRL pin mappings
#define PIN_CPRES_1 GPA0
#define PIN_CPRES_2 GPA1
#define PIN_CPRES_3 GPA2
#define PIN_CEN_1 GPA3
#define PIN_CEN_2 GPA4
#define PIN_CEN_3 GPA5

BusControlClass::BusControlClass() {
    this->_hasBUSCTRL = false;
    this->_hasIOXEP = false;
    this->_card1Present = false;
    this->_card2Present = false;
    this->_card3Present = false;
}

void BusControlClass::init() {
    Wire.beginTransmission(GPIOEXP_ADDR);
    this->_hasIOXEP = (Wire.endTransmission() == 0);
    #ifdef DEBUG
    if (this->_hasIOXEP) {
        Serial.println(F("INIT: boot3 - IOS: Found I/O Expander."));
    }
    #endif

    this->_hasBUSCTRL = this->_busctlr.begin_I2C(BUSCTLR_ADDR);
    if (this->_hasBUSCTRL) {
        #ifdef DEBUG
        Serial.println(F("INIT: boot3 - IOS: Found bus controller"));
        Serial.print(F("INIT: boot3 - IOS: Initializing bus controller ..."));
        #endif

        this->_busctlr.pinMode(PIN_CPRES_1, INPUT);
        this->_busctlr.pinMode(PIN_CPRES_2, INPUT);
        this->_busctlr.pinMode(PIN_CPRES_3, INPUT);
        this->_busctlr.pinMode(PIN_CEN_1, OUTPUT);
        this->_busctlr.digitalWrite(PIN_CEN_1, LOW);
        this->_busctlr.pinMode(PIN_CEN_2, OUTPUT);
        this->_busctlr.digitalWrite(PIN_CEN_2, LOW);
        this->_busctlr.pinMode(PIN_CEN_3, OUTPUT);
        this->_busctlr.digitalWrite(PIN_CEN_3, LOW);

        #ifdef DEBUG
        Serial.println(F("DONE"));
        #endif
    }
}

bool BusControlClass::hasIOEXP() {
    return this->_hasIOXEP;
}

bool BusControlClass::hasBUSCTLR() {
    return this->_hasBUSCTRL;
}

void BusControlClass::writeGPIOA(byte data) {
    if (!this->_hasIOXEP) {
        return;
    }

    Wire.beginTransmission(GPIOEXP_ADDR);
	Wire.write(GPIOA_REG);
	Wire.write(data);
	Wire.endTransmission();
}

void BusControlClass::writeGPIOB(byte data) {
    if (!this->_hasIOXEP) {
        return;
    }

    Wire.beginTransmission(GPIOEXP_ADDR);
	Wire.write(GPIOB_REG);
	Wire.write(data);
	Wire.endTransmission();
}

void BusControlClass::writeIODirA(byte data) {
    if (!this->_hasIOXEP) {
        return;
    }

    Wire.beginTransmission(GPIOEXP_ADDR);
	Wire.write(IODIRA_REG);
	Wire.write(data);
	Wire.endTransmission();
}

void BusControlClass::writeIODirB(byte data) {
    if (!this->_hasIOXEP) {
        return;
    }

    Wire.beginTransmission(GPIOEXP_ADDR);
	Wire.write(IODIRB_REG);
	Wire.write(data);
	Wire.endTransmission();
}

void BusControlClass::writeGPPUA(byte data) {
    if (!this->_hasIOXEP) {
        return;
    }

    Wire.beginTransmission(GPIOEXP_ADDR);
	Wire.write(GPPUA_REG);
    Wire.write(data);
	Wire.endTransmission();
}

void BusControlClass::writeGPPUB(byte data) {
    if (!this->_hasIOXEP) {
        return;
    }

    Wire.beginTransmission(GPIOEXP_ADDR);
	Wire.write(GPPUB_REG);
	Wire.write(data);
	Wire.endTransmission();
}

byte BusControlClass::readGPIOA() {
    if (!this->_hasIOXEP) {
        return 0;
    }

    Wire.beginTransmission(GPIOEXP_ADDR);
	Wire.write(GPIOA_REG);
	Wire.endTransmission();

	Wire.beginTransmission(GPIOEXP_ADDR);
	Wire.requestFrom(GPIOEXP_ADDR, 1);
    return Wire.read();
}

byte BusControlClass::readGPIOB() {
    if (!this->_hasIOXEP) {
        return 0;
    }

    Wire.beginTransmission(GPIOEXP_ADDR);
	Wire.write(GPIOB_REG);
	Wire.endTransmission();

	Wire.beginTransmission(GPIOEXP_ADDR);
	Wire.requestFrom(GPIOEXP_ADDR, 1);
    return Wire.read();
}

void BusControlClass::detectCards() {
    // TODO Probably worth adding methods to inidividually enable/disable each card if present.
    // I can see the utility in having opcodes for doing this in software or even the ability
    // to do this in firmware under the right conditions (ie. disable card if faulted or to reset).

    // TODO The general idea here is that each card slot have access to 16 I/O ports per slot,
    // then gate access to those ports based on whether the card is present and enabled.
    this->_card1Present = (this->_busctlr.digitalRead(PIN_CPRES_1) == HIGH);
    if (this->_card1Present) {
        this->_busctlr.digitalWrite(PIN_CEN_1, HIGH);
    }

    this->_card2Present = (this->_busctlr.digitalRead(PIN_CPRES_2) == HIGH);
    if (this->_card2Present) {
        this->_busctlr.digitalWrite(PIN_CEN_2, HIGH);
    }

    this->_card3Present = (this->_busctlr.digitalRead(PIN_CPRES_3) == HIGH);
    if (this->_card3Present) {
        this->_busctlr.digitalWrite(PIN_CEN_3, HIGH);
    }
}

bool BusControlClass::card1Present() {
    return this->_card1Present;
}

bool BusControlClass::card2Present() {
    return this->_card2Present;
}

bool BusControlClass::card3Present() {
    return this->_card3Present;
}

BusControlClass BusControl;