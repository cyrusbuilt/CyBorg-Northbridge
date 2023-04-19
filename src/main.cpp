#include <Arduino.h>
#include <Wire.h>
#include "LED.h"
#include "BiosSettings.h"
#include "Buzzer.h"
#include "hal.h"
#include "iLoad.h"
#include "opcodes.h"
#include "rtc.h"
#include "ToggleSwitch.h"

#define DEBUG

#define FW_VERSION "1.0"

// Global vars
DebugMode debug = DebugMode::ON;
ToggleSwitch runTrigger(PIN_RUN, NULL, NULL);
HAF_LED iosLED(PIN_IOS_LED, NULL);
Buzzer pcSpk(PIN_PC_SPK, NULL, NULL);
bool hasIOExpander = false;
bool hasRTC = false;
bool hasExtendedRxBuf = false;
bool isSlowClock = false;
bool z80IntEnFlag = false;
bool z80IntSysTick = false;
bool lastRxIsEmpty = false;
FATFS filesysSD;
unsigned long timestamp = 0;
char inChar;
char OsName[11] = DS_OSNAME;
byte bufferSD[32];
byte numReadBytes = 0;
byte iCount = 0;
word bootStrAddr = 0;
word bootImageSize = 0;
const char* fileNameSD;
byte* bootImage;
char diskName[11] = Z80DISK;
byte ioAddress = 0;
byte ioData = 0;
byte ioOpCode = 0;
byte ioByteCount = 0;
byte diskErr = 0;
word trackSel = 0;
byte sectSel = 0;
byte tempByte = 0;
byte numWriBytes = 0;
byte irqStatus = 0;
byte sysTickTime = 100;

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

void awaitUserInput() {
	while (Serial.available() < 1) {
		blinkIOSled(&timestamp);
	}
}

bool checkUserButton() {
	#ifdef DEBUG
	Serial.println(F("INIT: boot1 - Checking USER button..."));
	#endif
	pinMode(PIN_USER, INPUT_PULLUP);
	return (digitalRead(PIN_USER) == LOW);
}

void initSystemControl() {
	#ifdef DEBUG
	Serial.print(F("INIT: boot1 - Initializing system control... "));
	#endif
	
	// Turn USER LED off.
	pinMode(PIN_USER, OUTPUT);
	digitalWrite(PIN_USER, HIGH);

	// Configure /INT and set INACTIVE.
	pinMode(PIN_INT, INPUT_PULLUP);
	pinMode(PIN_INT, OUTPUT);
	digitalWrite(PIN_INT, HIGH);

	// Configure RAM_CE2 and set ACTIVE.
	pinMode(PIN_RAM_CE2, OUTPUT);
	digitalWrite(PIN_RAM_CE2, HIGH);

	// Configure /WAIT as input.
	pinMode(PIN_WAIT, INPUT);

	// Configure /BUSREQ.
	pinMode(PIN_BUSREQ, INPUT_PULLUP);
	pinMode(PIN_BUSREQ, OUTPUT);
	digitalWrite(PIN_BUSREQ, HIGH);
	#ifdef DEBUG
	Serial.println(F("DONE"));
	#endif
}

void initSpeaker() {
	#ifdef DEBUG
	Serial.print(F("INIT: boot1 - Initializing PC speaker driver... "));
	#endif
	pcSpk.init();
	#ifdef DEBUG
	Serial.println(F("DONE"));
	#endif
}

void playErrorSound() {
	pcSpk.buzz(50, 150);
}

void playStartupJingle() {
	pcSpk.playNote(BuzzerNotes::BUZZER_NOTE_C, 50);
	pcSpk.playNote(BuzzerNotes::BUZZER_NOTE_A, 50);
	pcSpk.playNote(BuzzerNotes::BUZZER_NOTE_C, 50);
	pcSpk.playNote(BuzzerNotes::BUZZER_NOTE_F, 500);
	pcSpk.playNote(BuzzerNotes::BUZZER_NOTE_C, 50);
	pcSpk.playNote(BuzzerNotes::BUZZER_NOTE_F, 900);
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

	initSpeaker();
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

void printOsName(byte currentDiskSet) {
	Serial.print(F("Disk Set "));
	Serial.print(currentDiskSet);
	OsName[2] = currentDiskSet + 48;
	openSD(OsName);
	readSD(bufferSD, &numReadBytes);
	if (numReadBytes > 0) {
		Serial.print(F(" ("));
		Serial.print((const char*)bufferSD);
		Serial.print(F(")"));
	}
}

void handleToggleClockMode() {
	if (biosSettings_t.clockMode == ClockMode::SLOW) {
		biosSettings_t.clockMode = ClockMode::FAST;
	}
	else {
		biosSettings_t.clockMode = ClockMode::SLOW;
	}

	biosSettings_t.save();
}

void handleToggleAutoExecFlag() {
	biosSettings_t.autoExecFlag = !biosSettings_t.autoExecFlag;
	biosSettings_t.save();
}

void handleChangeDiskSet() {
	Serial.println(F("\r\nPress CR to accept, ESC to exit or any other key to change"));
	iCount = biosSettings_t.diskSet;
	do {
		iCount = (iCount + 1) % MAX_DISK_SET;
		Serial.print(F("\r ->"));
		printOsName(iCount);
		Serial.print(F("                 \r"));
		flushSerialRXBuffer();
		awaitUserInput();
		inChar = Serial.read();
	} while ((inChar != KEY_CODE_CR) && (inChar != KEY_CODE_ESC));

	Serial.println();
	Serial.println();
	if (inChar == KEY_CODE_CR) {
		biosSettings_t.diskSet = iCount;
		biosSettings_t.save();
	}
}

void handleManualSetRTC() {
	byte tempByte = 0;
	RTC.update();
	Serial.println(F("\nIOS: RTC manual setting:"));
	Serial.println(F("\nPress T/U to increment +10/+1 or CR to accept"));
	do {
		do {
			Serial.print(F(" "));
			switch (tempByte) {
				case 0:
					Serial.print(F("Year -> "));
					RTC.print2Digit(RTC.dateTime->year);
					break;
				case 1:
					Serial.print(F("Month -> "));
					RTC.print2Digit(RTC.dateTime->month);
					break;
				case 2:
					Serial.print(F("             "));
					Serial.write(KEY_CODE_CR);
					Serial.print(F("Day -> "));
					RTC.print2Digit(RTC.dateTime->day);
					break;
				case 3:
					Serial.print(F("Hours -> "));
					RTC.print2Digit(RTC.dateTime->hours);
					break;
				case 4:
					Serial.print(F("Minutes -> "));
					RTC.print2Digit(RTC.dateTime->minutes);
					break;
				case 5:
					Serial.print(F("Seconds -> "));
					RTC.print2Digit(RTC.dateTime->seconds);
					break;
			}

			timestamp = millis();
			while (inChar != 'u' && inChar != 'U' && inChar != 't' && inChar != 'T' && inChar != KEY_CODE_CR) {
				blinkIOSled(&timestamp);
				inChar = Serial.read();
			}

			if (inChar == 'u' || inChar == 'U') {
				switch (tempByte) {
					case 0:
						RTC.dateTime->year++;
						if (RTC.dateTime->year > 99) {
							RTC.dateTime->year = 0;
						}
						break;
					case 1:
						RTC.dateTime->month++;
						if (RTC.dateTime->month > 12) {
							RTC.dateTime->month = 1;
						}
						break;
					case 2:
						{
							RTC.dateTime->day++;
							if (RTC.dateTime->month == 2) {
								const byte leapYear = RTC.isLeapYear(RTC.dateTime->year) ? 1 : 0;
								const byte dayOfMonth = DAYS_OF_MONTH[RTC.dateTime->month - 1];
								if (RTC.dateTime->day > dayOfMonth + leapYear) {
									RTC.dateTime->day = 1;
								}
								else if (RTC.dateTime->day > dayOfMonth) {
									RTC.dateTime->day = 1;
								}
							}
						}
						break;
					case 3:
						RTC.dateTime->hours++;
						if (RTC.dateTime->hours > 23) {
							RTC.dateTime->hours = 0;
						}
						break;
					case 4:
						RTC.dateTime->minutes++;
						if (RTC.dateTime->minutes > 59) {
							RTC.dateTime->minutes = 0;
						}
						break;
					case 5:
						RTC.dateTime->seconds++;
						if (RTC.dateTime->seconds > 59) {
							RTC.dateTime->seconds = 0;
						}
						break;
					default:
						break;
				}
			}

			if (inChar == 't' || inChar == 'T') {
				switch (tempByte) {
					case 0:
						RTC.dateTime->year = RTC.dateTime->year + 10;
						if (RTC.dateTime->year > 99) {
							RTC.dateTime->year = RTC.dateTime->year - (RTC.dateTime->year / 10) * 10;
						}
						break;
					case 1:
						if (RTC.dateTime->month > 10) {
							RTC.dateTime->month = RTC.dateTime->month - 10;
						}
						else if (RTC.dateTime->month < 3) {
							RTC.dateTime->month = RTC.dateTime->month + 10;
						}
						break;
					case 2:
						{
							RTC.dateTime->day = RTC.dateTime->day + 10;
							const byte leapYear = RTC.isLeapYear(RTC.dateTime->year) ? 1 : 0;
							const byte dayOfMonth = DAYS_OF_MONTH[RTC.dateTime->month - 1];
							if (RTC.dateTime->day > (dayOfMonth + leapYear)) {
								RTC.dateTime->day = RTC.dateTime->day - (RTC.dateTime->day / 10) * 10;
							}

							if (RTC.dateTime->day == 0) {
								RTC.dateTime->day = 1;
							}
						}
						break;
					case 3:
						RTC.dateTime->hours = RTC.dateTime->hours + 10;
						if (RTC.dateTime->hours > 23) {
							RTC.dateTime->hours = RTC.dateTime->hours - (RTC.dateTime->hours / 10) * 10;
						}
						break;
					case 4:
						RTC.dateTime->minutes = RTC.dateTime->minutes + 10;
						if (RTC.dateTime->minutes > 59) {
							RTC.dateTime->minutes = RTC.dateTime->minutes - (RTC.dateTime->minutes / 10) * 10;
						}
						break;
					case 5:
						RTC.dateTime->seconds = RTC.dateTime->seconds + 10;
						if (RTC.dateTime->seconds > 59) {
							RTC.dateTime->seconds = RTC.dateTime->seconds - (RTC.dateTime->seconds / 10) * 10;
						}
						break;
					default:
						break;
				}
			}

			Serial.write(KEY_CODE_CR);
		} while (inChar != KEY_CODE_CR);

		tempByte++;
	} while (tempByte < 6);

	RTC.save();
	Serial.println(F(" ...done      "));
	Serial.print(F("IOS: RTC date/time updated ("));
	RTC.printDateTime(true);
	Serial.println(F(")"));
}

void setBootModeFlags() {
	switch (biosSettings_t.bootMode) {
		case BootMode::BASIC:
			fileNameSD = BASICFN;
			bootStrAddr = BASSTRADDR;
			z80IntEnFlag = true;
			break;
		case BootMode::FORTH:
			fileNameSD = FORTHFN;
			bootStrAddr = FORSTRADDR;
			break;
		case BootMode::OS_ON_SD:
			// TODO maybe we can make these diskSet values into enum?
			switch (biosSettings_t.diskSet) {
				case 0:
					fileNameSD = CPMFN;
					bootStrAddr = CPMSTRADDR;
					break;
				case 1:
					fileNameSD = QPMFN;
					bootStrAddr = QPMSTRADDR;
					break;
				case 2:
					fileNameSD = CPM3FN;
					bootStrAddr = CPM3STRADDR;
					break;
				case 3:
					fileNameSD = UCSDFN;
					bootStrAddr = UCSDSTRADDR;
					break;
				case 4:
					fileNameSD = FUZIXFN;
					bootStrAddr = FUZSTRADDR;
					z80IntEnFlag = true;
					z80IntSysTick = true;
					break;
				case 5:
					fileNameSD = FUZIXFN;
					bootStrAddr = FUZSTRADDR;
					z80IntEnFlag = false;
					z80IntSysTick = false;
					break;
				default:
					break;
			}
		case BootMode::AUTO:
			fileNameSD = AUTOFN;
			bootStrAddr = AUTSTRADDR;
			break;
		case BootMode::ILOAD:
			bootImage = (byte*)pgm_read_word(&flashBootTable[0]);
			bootImageSize = sizeof(boot_A_);
			bootStrAddr = boot_A_StrAddr;
			break;
		default:
			break;
	}
}

void bootStage4(bool showBootMenu) {
	// TODO do we *need* to do this twice for some reason?
	// TODO actually, do we need them at all since we call it later on depending on
	// boot selection???
	mountSD(&filesysSD);
	mountSD(&filesysSD);
	const byte maxBootMode = (byte)BootMode::BASIC;
	byte selBootMode = 0;

	// TODO should we handle edge case scenario here where boot mode could potentially be invalid?
	if (showBootMenu) {
		flushSerialRXBuffer();
		playStartupJingle();
		Serial.println();
		Serial.println(F("INIT: boot4 - IOS: Select boot mode or system parameters:"));
		Serial.println();
		Serial.print(F(" 0: No change ("));
		Serial.print((uint8_t)biosSettings_t.bootMode + 1);
		Serial.println(F(")"));
		Serial.println(F(" 1: BASIC"));
		Serial.println(F(" 2: Forth"));
		Serial.print(F(" 3: Load OS from "));
		printOsName(biosSettings_t.diskSet);
		Serial.println(F("\r\n 4: Autoboot"));
		Serial.println(F(" 5: iLoad"));
		Serial.println(F(" 6: Change Z80 clock speed (->"));
		Serial.print(biosSettings_t.clockMode == ClockMode::FAST ? CLOCK_HIGH : CLOCK_LOW);
		Serial.println(F("MHz"));
		Serial.print(F(" 7: Toggle CP/M Autoexec (->"));
		Serial.print(biosSettings_t.autoExecFlag ? F("ON") : F("OFF"));
		Serial.println(F(")"));
		Serial.print(F(" 8: Change "));
		printOsName(biosSettings_t.diskSet);
		Serial.println();

		char minBootChar = '0';
		char maxSelChar = '8';
		if (hasRTC) {
			Serial.println(F(" 9: Change RTC time/date"));
			maxSelChar = '9';
		}

		// Ask the user to make a boot selection choice.
		Serial.println();
		timestamp = millis();
		Serial.print(F("Enter your choice >"));
		do {
			blinkIOSled(&timestamp);
			inChar = Serial.read();
		} while((inChar < minBootChar) || (inChar > maxSelChar));

		Serial.print(inChar);
		Serial.println(F(" OK"));

		switch (inChar) {
			case '6':
				handleToggleClockMode();
				break;
			case '7':
				handleToggleAutoExecFlag();
				break;
			case '8':
				handleChangeDiskSet();
				break;
			case '9':
				handleManualSetRTC();
				break;
			default:
				break;
		}

		
		selBootMode = (byte)(inChar - '1');
		if (selBootMode <= maxBootMode) {
			biosSettings_t.bootMode = (BootMode)selBootMode;
			biosSettings_t.save();
		}
		else {
			biosSettings_t.load();
		}
	}

	if (biosSettings_t.bootMode == BootMode::OS_ON_SD) {
		Serial.print(F("INIT: boot4 - IOS: Current "));
		printOsName(biosSettings_t.diskSet);
		Serial.println();
	}

	setBootModeFlags();
	digitalWrite(PIN_WAIT_RES, HIGH);
	if (bootStrAddr > ZERO_ADDR) {
		loadHL(ZERO_ADDR);
		loadByteToRAM(lowByte(bootStrAddr));
		loadByteToRAM(highByte(bootStrAddr));
		if (debug != DebugMode::OFF) {
			Serial.print(F("DEBUG: Injected JP 0x"));
			Serial.println(bootStrAddr, HEX);
		}
	}

	loadHL(bootStrAddr);
	if (debug != DebugMode::OFF) {
		Serial.print(F("DEBUG: Flash bootImageSize = "));
		Serial.println(bootImageSize);
		Serial.print(F("DEBUG: bootStrAddr = "));
		Serial.println(bootStrAddr, HEX);
	}

	byte errCodeSD = ERR_DSK_EMU_OK;
	if (selBootMode < maxBootMode) {
		if (mountSD(&filesysSD)) {
			errCodeSD = mountSD(&filesysSD);
			while (errCodeSD) {
				printErrSD(SD_OP_TYPE_MOUNT, errCodeSD, NULL);
				waitKeySD();
				mountSD(&filesysSD);
				errCodeSD = mountSD(&filesysSD);
			}
		}

		errCodeSD = openSD(fileNameSD);
		while (errCodeSD) {
			printErrSD(SD_OP_TYPE_OPEN, errCodeSD, fileNameSD);
			waitKeySD();
			errCodeSD = openSD(fileNameSD);
			if (errCodeSD != ERR_DSK_EMU_NO_FILE) {
				mountSD(&filesysSD);
				mountSD(&filesysSD);
				errCodeSD = openSD(fileNameSD);
			}
		}

		Serial.print(F("INIT: boot4 - IOS: Loading boot program ("));
		Serial.print(fileNameSD);
		Serial.print(F(")..."));
		while (errCodeSD) {
			while ((numReadBytes == 32) && (!errCodeSD)) {
				errCodeSD = readSD(bufferSD, &numReadBytes);
				for (iCount = 0; iCount < numReadBytes; iCount++) {
					loadByteToRAM(bufferSD[iCount]);
				}
			}

			if (errCodeSD) {
				printErrSD(SD_OP_TYPE_READ, errCodeSD, fileNameSD);
				waitKeySD();
				seekSD(0);
			}
		}
	}
	else {
		Serial.print(F("INIT: boot 4 - IOS: Loading boot program..."));
		for (word i = 0; i < bootImageSize; i++) {
			loadByteToRAM(pgm_read_byte(bootImage + i));
		}
	}

	Serial.println(F(" Done"));
}

void bootStage5(bool showBoot) {
	// TODO Show summary screen?
	// TODO BIOS version, RTC presence, IOEXP presence, CPU speed, RAM, etc.
	if (!showBoot) {
		playStartupJingle();
	}

	digitalWrite(PIN_RESET, LOW);
	ASSR &= ~(1 << AS2);
	TCCR2 |= (1 << CS20);
	TCCR2 &= ~((1 << CS21) | (1 << CS22));
	TCCR2 |= (1 << WGM21);
	TCCR2 &= ~(1 << WGM20);
	TCCR2 |= (1 << COM20);
	TCCR2 &= ~(1 << COM21);
	OCR2 = (byte)biosSettings_t.clockMode;
	
	pinMode(PIN_CLK, OUTPUT);
	Serial.println(F("INIT: boot5 - IOS: Z80 CPU running"));
	Serial.println();
	flushSerialRXBuffer();

	delay(1);
	digitalWrite(PIN_RESET, HIGH);
}

void setup() {
	bool showBootMenu = false;
	bootStage0();
	bootStage1(&showBootMenu);
	bootStage2();
	bootStage3();
	bootStage4(showBootMenu);
	bootStage5();
}

void loop() {
	if (!digitalRead(PIN_WAIT)) {
		// I/O Operation requested
		if (!digitalRead(PIN_WR)) {
			// I/O Write Operation requested.
			ioAddress = digitalRead(PIN_AD0);
			ioData = PINA;
			if (ioAddress) {
				ioOpCode = ioData;
				ioByteCount = 0;
			}
			else {
				switch (ioOpCode) {
					case OP_IO_WR_USR_LED:
						digitalWrite(PIN_USER, (ioData & B00000001) ? LOW : HIGH);
						break;
					case OP_IO_WR_SER_TX:
						Serial.write(ioData);
						break;
					case OP_IO_WR_GPIOA:
						if (hasIOExpander) {
							Wire.beginTransmission(GPIOEXP_ADDR);
							Wire.write(GPIOA_REG);
							Wire.write(ioData);
							Wire.endTransmission();
						}
						break;
					case OP_IO_WR_GPIOB:
						if (hasIOExpander) {
							Wire.beginTransmission(GPIOEXP_ADDR);
							Wire.write(GPIOB_REG);
							Wire.write(ioData);
							Wire.endTransmission();
						}
						break;
					case OP_IO_WR_IODIRA:
						if (hasIOExpander) {
							Wire.beginTransmission(GPIOEXP_ADDR);
							Wire.write(IODIRA_REG);
							Wire.write(ioData);
							Wire.endTransmission();
						}
						break;
					case OP_IO_WR_IODIRB:
						if (hasIOExpander) {
							Wire.beginTransmission(GPIOEXP_ADDR);
							Wire.write(IODIRB_REG);
							Wire.write(ioData);
							Wire.endTransmission();
						}
						break;
					case OP_IO_WR_GPPUA:
						if (hasIOExpander) {
							Wire.endTransmission(GPIOEXP_ADDR);
							Wire.write(GPPUA_REG);
							Wire.write(ioData);
							Wire.endTransmission();
						}
						break;
					case OP_IO_WR_GPPUB:
						if (hasIOExpander) {
							Wire.beginTransmission(GPIOEXP_ADDR);
							Wire.write(GPPUB_REG);
							Wire.write(ioData);
							Wire.endTransmission();
						}
						break;
					case OP_IO_WR_SELDSK:
						if (ioData <= MAX_DISK_NUM) {
							diskName[2] = biosSettings_t.diskSet + 48;
							diskName[4] = (ioData / 10) + 48;
							diskName[5] = ioData - ((ioData / 10) * 10) + 48;
							diskErr = openSD(diskName);
						}
						else {
							diskErr = ERR_DSK_EMU_ILLEGAL_DSK_NUM;
						}
						break;
					case OP_IO_WR_SELTRK:
						if (!ioByteCount) {
							trackSel = ioData;
						}
						else {
							trackSel = (((word)ioData) << 8) | lowByte(trackSel);
							if ((trackSel < MAX_TRACKS) && (sectSel < MAX_SECTORS)) {
								diskErr = ERR_DSK_EMU_OK;
							}
							else {
								if (sectSel < MAX_SECTORS) {
									diskErr = ERR_DSK_EMU_ILLEGAL_TRK_NUM;
								}
								else {
									diskErr = ERR_DSK_EMU_ILLEGAL_SCT_NUM;
								}
								ioOpCode = OP_IO_NOP;
							}
						}

						ioByteCount++;
						break;
					case OP_IO_WR_SELSCT:
						sectSel = ioData;
						if ((trackSel < MAX_TRACKS) && (sectSel < MAX_SECTORS)) {
							diskErr = ERR_DSK_EMU_OK;
						}
						else {
							if (sectSel < MAX_SECTORS) {
								diskErr = ERR_DSK_EMU_ILLEGAL_TRK_NUM;
							}
							else {
								diskErr = ERR_DSK_EMU_ILLEGAL_SCT_NUM;
							}
						}
						break;
					case OP_IO_WR_WRTSCT:
						if (!ioByteCount) {
							if ((trackSel < MAX_TRACKS) && (sectSel < MAX_SECTORS) && (!diskErr)) {
								diskErr = seekSD((trackSel << 5) | sectSel);
							}
						}

						if (!diskErr) {
							tempByte = ioByteCount % MAX_SECTORS;
							bufferSD[tempByte] = ioData;
							if (tempByte == (MAX_SECTORS - 1)) {
								diskErr = writeSD(bufferSD, &numWriBytes);
								if (numWriBytes < MAX_SECTORS) {
									diskErr = ERR_DSK_EMU_UNEXPECTED_EOF;
								}

								if (ioByteCount >= (MAX_TRACKS - 1)) {
									if (!diskErr) {
										diskErr = writeSD(NULL, &numWriBytes);
									}

									ioOpCode = OP_IO_NOP;
								}
							}
						}

						ioByteCount++;
						break;
					case OP_IO_WR_SETBNK:
						switch (ioData) {
							case OS_MEM_BANK_0:
								// Set physical bank 0 (logical bank 1)
								digitalWrite(PIN_BANK0, HIGH);
								digitalWrite(PIN_BANK1, LOW);
								break;
							case OS_MEM_BANK_1:
								digitalWrite(PIN_BANK0, HIGH);
								digitalWrite(PIN_BANK1, HIGH);
								break;
							case OS_MEM_BANK_2:
								digitalWrite(PIN_BANK0, LOW);
								digitalWrite(PIN_BANK1, HIGH);
								break;
							default:
								break;
						}
						break;
					case OP_IO_WR_SETIRQ:
						z80IntEnFlag = (bool)(ioData & 1);
						z80IntSysTick = (bool)(ioData & (1 << 1)) >> 1;
						break;
					case OP_IO_WR_SETTICK:
						if (ioData > 0) {
							sysTickTime = ioData;
						}
						break;
					case OP_IO_WR_BEEPSTART:
						// TODO Support frequencies higher than 255 by allowing for a 4 byte exchange to get
						// a full integer.
						pcSpk.buzz(ioData, 0UL);
						break;
					case OP_IO_WR_BEEPSTOP:
						pcSpk.off();
						break;
					default:
						break;
				}

				if ((ioOpCode != OP_IO_WR_SELTRK) && (ioOpCode != OP_IO_WR_WRTSCT)) {
					ioOpCode = OP_IO_NOP;
				}
			}

			exitWaitState();
		}
		else if (!digitalRead(PIN_RD)) {
			// I/O Read operaion requested.
			ioAddress = digitalRead(PIN_AD0);
			ioData = 0;
			if (ioAddress) {
				// AD0 = 1 (I/O Read Address = 0x01). We're reading Serial RX.
				ioData = OP_IO_NOP;
				if (Serial.available() > 0) {
					ioData = Serial.read();
					lastRxIsEmpty = false;
				}
				else {
					lastRxIsEmpty = true;
				}

				irqStatus &= B11111110;
				digitalWrite(PIN_INT, HIGH);
			}
			else {
				// AD0 = 0 (I/O Read address = 0x00). Execute read OpCode.
				switch (ioOpCode) {
					case OP_IO_RD_USRKEY:
						tempByte = digitalRead(PIN_USER);
						pinMode(PIN_USER, INPUT_PULLUP);
						ioData = !digitalRead(PIN_USER);
						pinMode(PIN_USER, OUTPUT);
						digitalWrite(PIN_USER, tempByte);
						break;
					case OP_IO_RD_GPIOA:
						if (hasIOExpander) {
							Wire.beginTransmission(GPIOEXP_ADDR);
							Wire.write(GPIOA_REG);
							Wire.endTransmission();

							Wire.beginTransmission(GPIOEXP_ADDR);
							Wire.requestFrom(GPIOEXP_ADDR, 1);
							ioData = Wire.read();
						}
						break;
					case OP_IO_RD_GPIOB:
						if (hasIOExpander) {
							Wire.beginTransmission(GPIOEXP_ADDR);
							Wire.write(GPIOB_REG);
							Wire.endTransmission();

							Wire.beginTransmission(GPIOEXP_ADDR);
							Wire.requestFrom(GPIOEXP_ADDR, 1);
							ioData = Wire.read();
						}
						break;
					case OP_IO_RD_SYSFLG:
						ioData = biosSettings_t.autoExecFlag
							| ((byte)hasRTC << 1)
							| ((Serial.available() > 0) << 2)
							| (lastRxIsEmpty << 3);
						break;
					case OP_IO_RD_DATTME:
						if (hasRTC) {
							if (ioByteCount == 0) {
								RTC.update();
							}

							if (ioByteCount < 7) {
								switch (ioByteCount) {
									case 0:
										ioData = RTC.dateTime->seconds;
										break;
									case 1:
										ioData = RTC.dateTime->minutes;
										break;
									case 2:
										ioData = RTC.dateTime->hours;
										break;
									case 3:
										ioData = RTC.dateTime->day;
										break;
									case 4:
										ioData = RTC.dateTime->month;
										break;
									case 5:
										ioData = RTC.dateTime->year;
										break;
									case 6:
										ioData = RTC.dateTime->tempC;
										break;
									default:
										break;
								}

								ioByteCount++;
							}
							else {
								ioOpCode = OP_IO_NOP;
							}
						}
						else {
							ioOpCode = OP_IO_NOP;
						}
						break;
					case OP_IO_RD_ERRDSK:
						ioData = diskErr;
						break;
					case OP_IO_RD_RDSECT:
						if (!ioByteCount && (trackSel < MAX_TRACKS) && (sectSel < MAX_SECTORS) && !diskErr) {
							diskErr = seekSD((trackSel << 5) | sectSel);
						}

						if (!diskErr) {
							tempByte = ioByteCount % MAX_SECTORS;
							if (!tempByte) {
								diskErr = readSD(bufferSD, &numReadBytes);
								if (numReadBytes < MAX_SECTORS) {
									diskErr = ERR_DSK_EMU_UNEXPECTED_EOF;
								}
							}

							if (!diskErr) {
								ioData = bufferSD[tempByte];
							}
						}

						if (ioByteCount >= (MAX_TRACKS - 1)) {
							ioOpCode = OP_IO_NOP;
						}

						ioByteCount++;
						break;
					case OP_IO_RD_SDMNT:
						ioData = mountSD(&filesysSD);
						break;
					case OP_IO_RD_ATXBUFF:
						ioData = Serial.availableForWrite();
						break;
					case OP_IO_RD_SYSIRQ:
						ioData = irqStatus;
						irqStatus = 0;
						digitalWrite(PIN_INT, HIGH);
						break;
					default:
						break;
				}

				if ((ioOpCode != OP_IO_RD_DATTME) && (ioOpCode != OP_IO_RD_RDSECT)) {
					ioOpCode = OP_IO_NOP;
				}
			}

			DDRA = 0xFF;       // Configure Z80 data bus D0 - D7 (PA0 - PA7) as output
			PORTA = ioData;    // Write to data bus.

			// Bus control to exit from wait state (M I/O read cycle)
			digitalWrite(PIN_BUSREQ, LOW);     // Request DMA
			digitalWrite(PIN_WAIT_RES, LOW);   // Now safe reset WAIT FF (exit wait state)
			delayMicroseconds(2);              // Wait 2us to be sure Z80 read the data and go Hi-Z
			DDRA = 0x00;                       // Configure Z80 data bus as input with pullup
			PORTA = 0xFF;
			digitalWrite(PIN_WAIT_RES, HIGH);  // Now Z80 is in DMA (Hi-Z), so safe to set WAIT_RES HIGH again
			digitalWrite(PIN_BUSREQ, HIGH);    // Resume Z80 from DMA.
		}
		else {
			// VIRTUAL INTERRUPT
			if (debug == DebugMode::TRACE) {
				Serial.println();
				Serial.println(F("DEBUG: INT op (nothing to do)"));
			}

			exitWaitState();
		}
	}

	if (z80IntSysTick) {
		if ((micros() - timestamp) > (((unsigned long)sysTickTime) * 1000)) {
			digitalWrite(PIN_INT, LOW);
			irqStatus |= B00000010;
			timestamp = micros();
		}
	}
}