#ifndef _BIOS_SETTINGS_H
#define _BIOS_SETTINGS_H

#include <Arduino.h>
#include <EEPROM.h>

#define BOOT_MODE_ADDR 10        // Internal EEPROM address for boot mode storage.
#define AUTOEXEC_FLAG_ADDR 12    // Internal EEPROM address for AUTOEXEC flag storage.
#define CLOCK_MODE_ADDR 13       // Internal EEPROM address for the Z80 clock high/low speed switch.
#define DISK_SET_ADDR 14         // Internal EEPROM address for the current disk set [0..99].
#define STARTUP_JINGLE_ADDR 15   // Internal EEPROM address of startup jingle flag storage.
#define MAX_DISK_NUM 99          // Maximum number of virtual disks.
#define MAX_DISK_SET 6           // Maximum number of configured disk sets.

enum class ClockMode : uint8_t {
	SLOW = 1,
	FAST = 0
};

enum class BootMode : uint8_t {
	BASIC = 0,
	FORTH = 1,
	OS_ON_SD = 2,
	AUTO = 3,
	ILOAD = 4
};

struct BiosSettings {
	ClockMode clockMode;
	byte diskSet;
	bool autoExecFlag;
	bool enableStartupJingle;
	BootMode bootMode;

	BiosSettings() {
		clockMode = ClockMode::SLOW;
		diskSet = 0;
		autoExecFlag = false;
		enableStartupJingle = true;
		bootMode = BootMode::ILOAD;
	}

	void load() {
		if (EEPROM.read(CLOCK_MODE_ADDR) > (byte)ClockMode::SLOW) {
			EEPROM.update(CLOCK_MODE_ADDR, (byte)ClockMode::SLOW);
		}

		clockMode = (ClockMode)EEPROM.read(CLOCK_MODE_ADDR);
		diskSet = EEPROM.read(DISK_SET_ADDR);
		if (diskSet >= MAX_DISK_SET) {
			EEPROM.update(DISK_SET_ADDR, 0);
			diskSet = 0;
		}

		if (EEPROM.read(AUTOEXEC_FLAG_ADDR) > 1) {
			EEPROM.update(AUTOEXEC_FLAG_ADDR, 0);
		}

		// TODO should we add special handling for an edge-case scenario where the value
		// being read is invalid (not one of the expected values we can cast to)?
		autoExecFlag = (bool)EEPROM.read(AUTOEXEC_FLAG_ADDR);
		bootMode = (BootMode)EEPROM.read(BOOT_MODE_ADDR);
		enableStartupJingle = (bool)EEPROM.read(STARTUP_JINGLE_ADDR);
	}

	void save() {
		EEPROM.update(CLOCK_MODE_ADDR, (byte)clockMode);
		EEPROM.update(DISK_SET_ADDR, diskSet);
		EEPROM.update(AUTOEXEC_FLAG_ADDR, (byte)autoExecFlag);
		EEPROM.update(BOOT_MODE_ADDR, (byte)bootMode);
		EEPROM.update(STARTUP_JINGLE_ADDR, (byte)enableStartupJingle);
	}
};

extern BiosSettings biosSettings_t;
#endif