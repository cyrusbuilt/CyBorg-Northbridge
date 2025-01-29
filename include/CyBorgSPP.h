#ifndef _CYBORG_SPP_H
#define _CYBORG_SPP_H

#include <Arduino.h>

class CyBorgSPPClass {
public:
    CyBorgSPPClass();
    void detect();
    bool isPresent();
    void init(byte data);
    void write(byte data);
    byte read();

private:
    bool _isPresent;
    byte _sppAutoFd;
    byte _tempData;
};

extern CyBorgSPPClass CyBorgSPP;

#endif