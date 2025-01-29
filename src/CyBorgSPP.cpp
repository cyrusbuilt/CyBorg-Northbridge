#include <Wire.h>
#include "hal.h"
#include "CyBorgSPP.h"

CyBorgSPPClass::CyBorgSPPClass() {
    this->_isPresent = false;
    this->_sppAutoFd = 0;
    this->_tempData = 0;
}

void CyBorgSPPClass::detect() {
    Wire.beginTransmission(SPP_ADDR);
    this->_isPresent = (Wire.endTransmission() == 0);
    #ifdef DEBUG
	if (this->_isPresent) {
		Serial.println(F("INIT: boot3 - IOS: Found Standard Parallel Port card"));
	}
	#endif
}

bool CyBorgSPPClass::isPresent() {
    return this->_isPresent;
}

void CyBorgSPPClass::init(byte data) {
    if (!this->_isPresent) {
        return;
    }

    this->_sppAutoFd = (!data) & 0x01;  // Store the value of the AUTOFD Control Line (active Low))

	// Set STROBE and INIT at 1, and AUTOFD = !D0
    Wire.beginTransmission(SPP_ADDR);
    Wire.write(GPIOA_REG);              // Select GPIOA
    Wire.write(0b00000101 | (byte) (this->_sppAutoFd << 1)); // Write value
    Wire.endTransmission();

	// Set the GPIO port to work as an SPP port (direction and pullup)
    Wire.beginTransmission(SPP_ADDR);
    Wire.write(IODIRA_REG);             // Select IODIRA
    Wire.write(0b11111000);             // Write value (1 = input, 0 = ouput)
    Wire.endTransmission();
    Wire.beginTransmission(SPP_ADDR);
    Wire.write(IODIRB_REG);             // Select IODIRB 
    Wire.write(0b00000000);             // Write value (1 = input, 0 = ouput)
    Wire.endTransmission();
    Wire.beginTransmission(SPP_ADDR);
    Wire.write(GPPUA_REG);              // Select GPPUA
    Wire.write(0b11111111);             // Write value (1 = pullup enabled, 0 = pullup disabled)
    Wire.endTransmission();

	// Initialize the printer using a pulse on INIT
    // NOTE: The I2C protocol introduces delays greater than needed by the SPP, so no further delay is used here to generate the pulse
    this->_tempData = 0b00000001 | (byte) (this->_sppAutoFd << 1);  // Change INIT bit to active (Low)
    Wire.beginTransmission(SPP_ADDR);
    Wire.write(GPIOA_REG);              // Select GPIOA
    Wire.write(this->_tempData);        // Set INIT bit to active (Low)
    Wire.endTransmission();

    this->_tempData = this->_tempData | 0b00000100;   // Change INIT bit to not active (High)
    Wire.beginTransmission(SPP_ADDR);
    Wire.write(GPIOA_REG);              // Select GPIOA
    Wire.write(this->_tempData);        // Set INIT bit to not active (High)
    Wire.endTransmission();
}

void CyBorgSPPClass::write(byte data) {
    if (!this->_isPresent) {
        return;
    }

    // NOTE: The I2C protocol introduces delays greater than needed by the SPP, so no further delay is used here to generate the pulse
    Wire.beginTransmission(SPP_ADDR);
    Wire.write(GPIOB_REG);            // Select GPIOB
    Wire.write(data);                 // Data on GPIOB
    Wire.endTransmission();

    this->_tempData = 0b11111100 | (byte) (this->_sppAutoFd << 1);  // Change STROBE bit to active (Low)
    Wire.beginTransmission(SPP_ADDR);
    Wire.write(GPIOA_REG);              // Select GPIOA
    Wire.write(this->_tempData);        // Set STROBE bit to active (Low)
    Wire.endTransmission();

    this->_tempData = this->_tempData | 0b00000001;   // Change STROBE bit to not active (High)
    Wire.beginTransmission(SPP_ADDR);
    Wire.write(GPIOA_REG);              // Select GPIOA
    Wire.write(this->_tempData);        // Set STROBE bit to not active (High)
    Wire.endTransmission();
}

byte CyBorgSPPClass::read() {
    if (!this->_isPresent) {
        return 0;
    }

    // Set MCP23017 pointer to GPIOA
    Wire.beginTransmission(SPP_ADDR);
    Wire.write(GPIOA_REG);
    Wire.endTransmission();
                
    // Read GPIOA (SPP Status Lines)
    Wire.beginTransmission(SPP_ADDR);
    Wire.requestFrom(SPP_ADDR, 1);
    byte ioData = Wire.read();
    ioData = (ioData & 0b11111000) | 0b00000001;      // Set D0 = 1, D1 = D2 = 0
    return ioData;
}

CyBorgSPPClass CyBorgSPP;