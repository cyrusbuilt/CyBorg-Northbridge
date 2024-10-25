#include "hal.h"
#include "opcodes.h"

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

void loadHL(word value) {
	pulseClock(1);
	digitalWrite(PIN_RAM_CE2, LOW);
	DDRA = 0xFF;
	PORTA = OPC_LD_HL_NN;
	pulseClock(2);
	DDRA = 0x00;
	PORTA = 0xFF;
	pulseClock(2);

	DDRA = 0xFF;
	PORTA = lowByte(value);
	pulseClock(3);

	PORTA = highByte(value);
	pulseClock(2);
	DDRA = 0x00;
	PORTA = 0xFF;
	digitalWrite(PIN_RAM_CE2, HIGH);
}

void loadByteToRAM(byte value) {
	pulseClock(1);
	digitalWrite(PIN_RAM_CE2, LOW);
	DDRA = 0xFF;
	PORTA = OPC_LD_HL;
	pulseClock(2);
	DDRA = 0x00;
	PORTA = 0xFF;
	pulseClock(2);
	
	DDRA = 0xFF;
	PORTA = value;
	pulseClock(2);
	DDRA = 0x00;
	PORTA = 0xFF;
	digitalWrite(PIN_RAM_CE2, HIGH);
	pulseClock(3);

	pulseClock(1);
	digitalWrite(PIN_RAM_CE2, LOW);
	DDRA = 0xFF;
	PORTA = OPC_INC_HL;
	pulseClock(2);
	DDRA = 0x00;
	PORTA = 0xFF;
	digitalWrite(PIN_RAM_CE2, HIGH);
	pulseClock(3);
}

void printBinaryByte(byte value) {
	for (byte mask = 0x80; mask; mask >>= 1) {
		Serial.print((mask & value) ? '1' : '0');
	}
}

// TODO This method is unused.
void serialEvent(bool intFlagUsed) {
	if ((Serial.available()) && intFlagUsed) {
		digitalWrite(PIN_INT, LOW);
	}
}

void flushSerialRXBuffer() {
	while (Serial.available() > 0) {
		Serial.read();
	}
}

void blinkIOSled(unsigned long *timestamp) {
	if ((millis() - *timestamp) > 200) {
		digitalWrite(PIN_IOS_LED, !digitalRead(PIN_IOS_LED));
		*timestamp = millis();
	}
}

byte mountSD(FATFS* fatfs) {
	#ifdef DEBUG
	Serial.println(F("DEBUG: Mounting SD filesystem ..."));
	#endif
	return pf_mount(fatfs);
}

byte openSD(const char* fileName) {
	return pf_open(fileName);
}

byte readSD(void* buffSD, byte* readBytes) {
	UINT numBytes;
	byte errCode = pf_read(buffSD, 32, &numBytes);
	*readBytes = (byte)numBytes;
	return errCode;
}

byte seekSD(word sectNum) {
	return pf_lseek((unsigned long)sectNum) << 9;
}

byte writeSD(void* buffSD, byte* numWrittenBytes) {
	UINT numBytes;
	byte errorCode;
	if (buffSD != NULL) {
		errorCode = pf_write(buffSD, MAX_SECTORS, &numBytes);
	}
	else {
		errorCode = pf_write(0, 0, &numBytes);
	}

	*numWrittenBytes = (byte)numBytes;
	return errorCode;
}

void printErrSD(byte opType, byte errCode, const char* fileName) {
	if (errCode == ERR_DSK_EMU_OK) {
		return;
	}

	Serial.print(F("\r\nIOS: SD error "));
	Serial.print(errCode);
	Serial.print(F(" ("));
	switch (errCode) {
		case ERR_DSK_EMU_DISK_ERR:
			Serial.print(F("DISK_ERR"));
			break;
		case ERR_DSK_EMU_NOT_READY:
			Serial.print(F("NOT_READY"));
			break;
		case ERR_DSK_EMU_NO_FILE:
			Serial.print(F("NO_FILE"));
			break;
		case ERR_DSK_EMU_NOT_OPENED:
			Serial.print(F("NOT_OPENED"));
			break;
		case ERR_DSK_EMU_NOT_ENABLED:
			Serial.print(F("NOT_ENABLED"));
			break;
		case ERR_DSK_EMU_NO_FILESYSTEM:
			Serial.print(F("NO_FILESYSTEM"));
			break;
		default:
			Serial.print(F("UNKNOWN"));
			break;
	}

	Serial.print(F(" on "));
	switch (opType) {
		case SD_OP_TYPE_MOUNT:
			Serial.print(F("MOUNT"));
			break;
		case SD_OP_TYPE_OPEN:
			Serial.print(F("OPEN"));
			break;
		case SD_OP_TYPE_READ:
			Serial.print(F("READ"));
			break;
		case SD_OP_TYPE_WRITE:
			Serial.print(F("WRITE"));
			break;
		case SD_OP_TYPE_SEEK:
			Serial.print(F("SEEK"));
			break;
		default:
			Serial.print(F("UNKNOWN"));
			break;
	}

	Serial.print(F(" operation"));
	if (fileName) {
		Serial.print(F(" - File: "));
		Serial.print(fileName);
	}
	
	Serial.println(F(")"));
}

void waitKeySD() {
	flushSerialRXBuffer();
	Serial.println(F("IOS: Check SD and press a key to repeat\r\n"));
	while (Serial.available() < 1);
}

void exitWaitState() {
	digitalWrite(PIN_BUSREQ, LOW);     // Request for DMA.
	digitalWrite(PIN_WAIT_RES, LOW);   // Reset WAIT FF exiting from WAIT state.
	digitalWrite(PIN_WAIT_RES, HIGH);  // Now Z80 is in DMA, so it's safe to set WAIT_RES HIGH again.
	digitalWrite(PIN_BUSREQ, HIGH);    // Resume Z80 from DMA.
}