#include "initializeHardware.h"
#include "../../peripheral/timer/timerFunctions.h"
#include "../../peripheral/i2c/i2c2.h"


digitalPin LED2R = {&TRISCbits, &PORTCbits, 3};
digitalPin LED2G = {&TRISCbits, &PORTCbits, 2};
digitalPin LED2B = {&TRISEbits, &PORTEbits, 4};

void setUpPorts() {
    /* Set port configuration */
    ANSELA = ANSELB = ANSELC = ANSELD = ANSELE = ANSELG = 0x0000;; // all ports digitaL
    delay_t1(20);
    // configure LED2 ports as outputs
    digitalOutput(LED2R);
    digitalOutput(LED2G);
    digitalOutput(LED2B);
    // configure i2c2 as a slave device with address 0x0b and interrupt priority 1
    I2C2SlaveInit(0x0b, 1);
}

