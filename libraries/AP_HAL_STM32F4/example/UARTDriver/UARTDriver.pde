// -*- Mode: C++; c-basic-offset: 8; indent-tabs-mode: nil -*-

//
// Example code for the AP_HAL AVRUARTDriver, based on FastSerial
//
// This code is placed into the public domain.
//

#include <AP_Common.h>
#include <AP_Math.h>
#include <AP_Param.h>
#include <AP_Progmem.h>

#include <AP_HAL.h>
#include <AP_HAL_STM32F4.h>

const AP_HAL::HAL& hal = AP_HAL_BOARD_DRIVER;

void setup(void)
{
    hal.console->begin(115200);
    
    //
    // Test printing things
    //
        
    hal.console->print("test");
    hal.console->println(" begin");
    hal.console->println(1000);
    hal.console->println(1000, 8);
    hal.console->println(1000, 10);
    hal.console->println(1000, 16);
    hal.console->println_P(PSTR("progmem"));
    int x = 3;
    int *ptr = &x;
    hal.console->printf("printf %d %u %#x %p %f %s\n", -1000, 1000, 1000, ptr, 1.2345, PSTR("progmem"));
    hal.console->printf_P(PSTR("printf_P %d %u %#x %p %f %s\n"), -1000, 1000, 1000, ptr, 1.2345, PSTR("progmem"));
    hal.console->println("done");
}

void loop(void)
{
    int c;
    //
    // Perform a simple loopback operation.
    //
    c = hal.console->read();
    if (-1 != c)
        hal.console->write(c);
}

AP_HAL_MAIN();
