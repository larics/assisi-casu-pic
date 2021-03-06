#include "p33FJ06GS101.h"
#include "../casu/initializeHardware.h"
#include "../actuators/pwm.h"
#include "../peripheral/timer/timerFunctions.h"
#include "../peripheral/timer/timer2.h"
#include <math.h>


#define PI 3.14159265359
#define VIBE_FREQ_REF_CONST 123665.5
/* Configuration Bit Settings */
_FOSCSEL(FNOSC_FRC)
_FOSC(FCKSM_CSECMD & OSCIOFNC_ON)
//_FWDT(FWDTEN_ON & WDTPRE_PR128 & WDTPOST_PS256)
_FWDT(FWDTEN_OFF)
_FPOR(FPWRT_PWR128 )
_FICD(ICS_PGD1 & JTAGEN_OFF)

void init_PWM(void);
void InitializeSPI(void);
void _ISRFAST _SPI1Interrupt(void);


volatile UINT16 vibeFreq_ref = 1;
volatile UINT16 vibeFreq_ref_old = 1;
volatile UINT16 vibeAmp_ref = 0;
UINT16 vibeFreq_ref_test = 1;
UINT16 vibeAmp_ref_test = 0;
float vibe_period = 5000.0; // in usec
volatile float dt_f = 0;
int N = 5;
float pwm_i[25];
int dt = 0;
int temp;
int SPI1counter = 0, tempVibeAmp = 0, tempVibeFreq = 0;
UINT8 wordsRec = 0;

int ampChange = 0;
int freqChange = 0;

int main()
{
/* Configure Oscillator to operate the device at 40Mhz
	   Fosc= Fin*M/(N1*N2), Fcy=Fosc/2
 	   Fosc= 7.37*(43)/(2*2)=80Mhz for Fosc, Fcy = 40Mhz */

	/* Configure PLL prescaler, PLL postscaler, PLL divisor */
	PLLFBD=41; 				/* M = PLLFBD + 2 */
	CLKDIVbits.PLLPOST=0;   /* N1 = 2 */
	CLKDIVbits.PLLPRE=0;    /* N2 = 2 */

    __builtin_write_OSCCONH(0x01);		/* New Oscillator FRC w/ PLL */
    __builtin_write_OSCCONL(0x01);  		/* Enable Switch */
      
	while(OSCCONbits.COSC != 0b001);		/* Wait for new Oscillator to become FRC w/ PLL */  
    while(OSCCONbits.LOCK != 1);			/* Wait for Pll to Lock */

	/* Now setup the ADC and PWM clock for 120MHz
	   ((FRC * 16) / APSTSCLR ) = (7.37 * 16) / 1 = ~ 120MHz*/

	ACLKCONbits.FRCSEL = 1;					/* FRC provides input for Auxiliary PLL (x16) */
	ACLKCONbits.SELACLK = 1;				/* Auxiliary Oscillator provides clock source for PWM & ADC */
	ACLKCONbits.APSTSCLR = 7;				/* Divide Auxiliary clock by 1 */
	ACLKCONbits.ENAPLL = 1;					/* Enable Auxiliary PLL */
	
	while(ACLKCONbits.APLLCK != 1);			/* Wait for Auxiliary PLL to Lock */
    
    int init_delay = 0;
    int dummy;
    
    ADPCFG = 0xFFFF;    /*A/D ports configuration bits*/
     
    TRISBbits.TRISB0 = 1; // SPI1 Slave Select  (input)
//    TRISBbits.TRISB5 = 1; // SPI1 CLK   (input)
//    TRISBbits.TRISB4 = 1; // SPI1 SDI/MOSI  (input)
//    TRISBbits.TRISB3 = 0; // SPI1 SDO/MISO  (output)
   
    
//    TMR2  = 0;          /* Reset Timer2 to 0x0000 */
//    PR2   = 0xFFFF;     /* assigning Period to Timer period register */
//    T2CONbits.TCKPS = 0b11;    /* configure control reg */
//    T2CONbits.TON = 1;
//    IFS0bits.T1IF = 0;
//    
//    for (init_delay = 0; init_delay < 4; init_delay++) {
//        while(IFS0bits.T1IF == 0);
//    } 
    
    OpenTimer2(T2_ON | T2_PS_1_256, 0xFFFF);
    while(TMR2<0xFFF0);
    
    init_PWM();
    
    /*
    INTCON1 = 0;
    INTCON2 = 0;
    */
    
    RPINR20bits.SDI1R = 0b00000100;
    RPINR20bits.SCK1R = 0b00000101;
    RPOR1bits.RP3R    = 0b00000111;
    RPINR21bits.SS1R  = 0b00000000;
    
    int i = 0;
    for (i = 0; i < N; i++) {
        
        pwm_i[i] = sin(0.5 * PI * i / N) ;
    }
    
    //vibe_period = 1000000.0 / vibeFreq_ref; // in usec
    dt_f = VIBE_FREQ_REF_CONST/(float)vibeFreq_ref; // 2 ms
    //CloseTimer2();
    if(dt_f > VIBE_FREQ_REF_CONST)
                dt_f = VIBE_FREQ_REF_CONST;
    ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
    OpenTimer2(T2_ON | T2_PS_1_1, ticks_from_us(dt_f, 1));
    
    InitializeSPI();
    
    float constFreq = 400;
    int firstLoop = 1;
    int freqFlag = 1;

    while(1) {
/*        
        if (firstLoop) {
            dt_f = VIBE_FREQ_REF_CONST/(float)constFreq; // 2 ms
            if(dt_f > VIBE_FREQ_REF_CONST)
                dt_f = VIBE_FREQ_REF_CONST;
            
            firstLoop = 0;
            OpenTimer2(T2_ON | T2_PS_1_1, ticks_from_us(dt_f, 1));  
        }

        if ((freqChange != 0) && (freqFlag == 1)) {
            freqFlag = 0;
            constFreq += freqChange;
            dt_f = VIBE_FREQ_REF_CONST/(float)constFreq; // 2 ms
            if(dt_f > VIBE_FREQ_REF_CONST)
                dt_f = VIBE_FREQ_REF_CONST;
            
            OpenTimer2(T2_ON | T2_PS_1_1, ticks_from_us(dt_f, 1)); 
        }
*/
        if (vibeFreq_ref != vibeFreq_ref_old) {
            CloseTimer2();
            dt_f = VIBE_FREQ_REF_CONST/(float)vibeFreq_ref; // 2 ms
            vibeFreq_ref_old = vibeFreq_ref;
            //CloseTimer2();
            if(dt_f > VIBE_FREQ_REF_CONST)
                dt_f = VIBE_FREQ_REF_CONST;


            ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
            OpenTimer2(T2_ON | T2_PS_1_1, ticks_from_us(dt_f, 1));  
        }

        ClrWdt();
    }

}

void init_PWM()
{

    PTCONbits.PTEN = 0;     //PWM module disable
    unsigned long period = ((unsigned long) FOSC/((unsigned long)FPWM*PWMPRE));
    //Primary Master Time Base
    PTCON2bits.PCLKDIV = 0b000;     //PWM prescaler = 1
    PTPER = (int) period;
    temp = PTPER / 200;

    //PWM1 Generator Initialization
    DTR1 = 50;
    ALTDTR1 = 50;
    PWMCON1 = 0;    //Edge-aligned mode, master time base, individual duty for both channels
    PHASE1 = PTPER;
    SPHASE1 = PTPER;
    PDC1 = PTPER*0.2;
    //SDC1 = PTPER/2;//PWM1H duty
    //PWM I/O control register
    IOCON1bits.PMOD = 0b00; //Complementary PWM mod
    PWMCON1bits.TRGSTAT = 0;
    PWMCON1bits.FLTSTAT = 0;
    PWMCON1bits.CLSTAT = 0;
    IOCON1bits.PENL = 1;
    IOCON1bits.PENH = 1;
    PTCONbits.SEIEN = 0; // disable special event primary master time base interrupt
    IFS5bits.PWM1IF = 0;
    IPC23bits.PWM1IP = 3; // priority 3
    TRIG1 = period;
    IEC5bits.PWM1IE = 0;
    PWMCON1bits.TRGIEN = 0; // a trigger event generates an interrupt request
    
    //Time base control register
    PTCONbits.PTSIDL = 1;   //PWM time base halts in CPU Idle mode
    PTCONbits.PTEN = 1;     //PWM module enable

}

void init_PWMuni()
{

    PTCONbits.PTEN = 0;     //PWM module disable
    unsigned long period = ((unsigned long) FOSC/((unsigned long)FPWM*PWMPRE));
    //Primary Master Time Base
    PTCON2bits.PCLKDIV = 0b000;     //PWM prescaler = 1
    PTPER = (int) period;

    //PWM1 Generator Initialization
    DTR1 = 0;
    ALTDTR1 = 0;
    PWMCON1 = 0;    //Edge-aligned mode, master time base, individual duty for both channels
    PHASE1 = PTPER;
    SPHASE1 = PTPER;
    PDC1 = 0;//PTPER*0.8;
    SDC1 = PTPER*0.8;//PWM1H duty

    //PWM I/O control register
    IOCON1bits.PMOD = 0b11; // PWM mod
    PWMCON1bits.TRGSTAT = 0;
    PWMCON1bits.FLTSTAT = 0;
    PWMCON1bits.CLSTAT = 0;
    IOCON1bits.PENL = 1;
    IOCON1bits.PENH = 1;
    PTCONbits.SEIEN = 0; // disable special event primary master time base interrupt
    IFS5bits.PWM1IF = 0;
    IPC23bits.PWM1IP = 3; // priority 3
    TRIG1 = period;
    IEC5bits.PWM1IE = 0;
    PWMCON1bits.TRGIEN = 0; // a trigger event generates an interrupt request

    //Time base control register
    PTCONbits.PTSIDL = 1;   //PWM time base halts in CPU Idle mode
    PTCONbits.PTEN = 1;     //PWM module enable
}

void __attribute__((__interrupt__, __auto_psv__)) _T2Interrupt(void)
{
    
    //T2CONbits.TON = 0;
    //TMR2 = 0;

    // scale pwm int according to given amplitude
    //pwm_int = pwm_int * vibeAmp_ref / 100.0;

    if (dt<N)
       PDC1 = temp * (pwm_i[dt]*vibeAmp_ref+100);
    else if (dt<2*N)
        PDC1 = temp * (pwm_i[2*N-1-dt]*vibeAmp_ref+100);
    else if (dt<N*3)
        PDC1 = temp * (-pwm_i[dt-2*N]*vibeAmp_ref+100);
    else if (dt<N*4)
        PDC1 = temp * (-pwm_i[4*N-dt]*vibeAmp_ref+100);
    else
        PDC1 = 0;

    /*
    int constAmp = 50;

    constAmp += ampChange;

    if (dt<N)
       PDC1 = temp * (pwm_i[dt]*constAmp+100);
    else if (dt<2*N)
        PDC1 = temp * (pwm_i[2*N-1-dt]*constAmp+100);
    else if (dt<N*3)
        PDC1 = temp * (-pwm_i[dt-2*N]*constAmp+100);
    else if (dt<N*4)
        PDC1 = temp * (-pwm_i[4*N-dt]*constAmp+100);
    else
        PDC1 = 0;
    */

    
    if (dt == (N-1) || dt == (2*N-1) || dt == (3*N-1))
        dt = dt + 2;
    else 
        dt = dt + 1;
    
    

    if (dt >= 4*N) {
        dt = 1;
    }
    IFS0bits.T2IF = 0;
    //T2CONbits.TON = 1;

}

 void InitializeSPI(void) {


    SPI1BUF = 0;                     //Datasheet specifies the following order for setting SPI2 in slave mode
    IFS0bits.SPI1IF = 0;             //Clear interrupt flag (no interrupt)
    IEC0bits.SPI1IE = 1;          //Enable SPI1 interrupts
    IPC2bits.SPI1IP = 6;             //Set SPI2 interrupt priority pretty high, higher than all other user interrupts
    SPI1CON1 = 0b0000011011011010;   //Setup for slave (0,0) mode with SS2 enabled.
    //SPI1CON1 = 0b0000000100011010;   //Setup for slave (0,0) mode with SS2 enabled.
    SPI1CON1bits.SMP = 0;            //Datasheet specifies this must be cleared.
    SPI1CON1bits.CKE = 0;            //Mode  
    SPI1CON1bits.CKP = 1;            //Idle state for clock is a low level;
    SPI1CON1bits.SSEN = 1;           //SS1 enabled
    SPI1CON2 = 0b0100000000000000;   //Do not used framed SPI
    SPI1STAT = 0b0000000000001100;   //Clear all status bits

    SPI1STATbits.SPIROV = 0;         //Make sure no errors
    
    IFS0bits.SPI1IF = 0;          //Clear interrupt flag (no interrupt)
    IEC0bits.SPI1IE = 1;             //Enable SPI2 interrupts
    SPI1STATbits.SPIEN = 1;          //Start SPI

 }
 
 
 void __attribute__((__interrupt__, __auto_psv__)) _SPI1Interrupt(void)
 {
    UINT16 buffer;
    if (!SPI1STATbits.SPIRBF)
        while(!SPI1STATbits.SPIRBF);
    buffer = (UINT16) SPI1BUF;
    
    if ((buffer & 0xF000) == 0x1000) {
        vibeAmp_ref =  buffer & 0x0FFF;
        
        if (vibeAmp_ref > 100)
            vibeAmp_ref = 100;
        else if (vibeAmp_ref < 0)
            vibeAmp_ref = 0;
        
        SPI1BUF = vibeAmp_ref;
    }
    else if ((buffer & 0xF000) == 0x2000) {
        vibeFreq_ref = buffer & 0x0FFF;
        
        if (vibeFreq_ref > 2000) 
            vibeFreq_ref = 2000;
        else if (vibeFreq_ref < 1)
            vibeFreq_ref = 1;
        
        SPI1BUF = vibeFreq_ref;
    }
    else {
        //SPI1BUF = 2000;
        ampChange = 50;
        freqChange = -100;
    }


    //SPI1BUF = buffer;

    if (SPI1STATbits.SPIROV == 1) {
//        SPI1STATbits.SPIEN = 0;
//        SPI1STATbits.SPIEN = 1;
        freqChange = 200;
    }

    IFS0bits.SPI1IF = 0;             //Clear the interrupt flag
    SPI1STATbits.SPIROV = 0;
 }
 