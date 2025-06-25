// TI File $Revision: /main/3 $
// Checkin $Date: April 21, 2008   16:00:39 $
//###########################################################################
//
// FILE:    Example_2823xCpuTimer.c
//
// TITLE:   DSP2823x Device Getting Started Program.
//
// ASSUMPTIONS:
//
//    This program requires the DSP2823x header files.
//
//    Other then boot mode configuration, no other hardware configuration
//    is required.
//
//
//    As supplied, this project is configured for "boot to SARAM"
//    operation.  The 2823x Boot Mode table is shown below.
//    For information on configuring the boot mode of an eZdsp,
//    please refer to the documentation included with the eZdsp,
//
//       $Boot_Table:
//
//         GPIO87   GPIO86     GPIO85   GPIO84
//          XA15     XA14       XA13     XA12
//           PU       PU         PU       PU
//        ==========================================
//            1        1          1        1    Jump to Flash
//            1        1          1        0    SCI-A boot
//            1        1          0        1    SPI-A boot
//            1        1          0        0    I2C-A boot
//            1        0          1        1    eCAN-A boot
//            1        0          1        0    McBSP-A boot
//            1        0          0        1    Jump to XINTF x16
//            1        0          0        0    Jump to XINTF x32
//            0        1          1        1    Jump to OTP
//            0        1          1        0    Parallel GPIO I/O boot
//            0        1          0        1    Parallel XINTF boot
//            0        1          0        0    Jump to SARAM	    <- "boot to SARAM"
//            0        0          1        1    Branch to check boot mode
//            0        0          1        0    Boot to flash, bypass ADC cal
//            0        0          0        1    Boot to SARAM, bypass ADC cal
//            0        0          0        0    Boot to SCI-A, bypass ADC cal
//                                              Boot_Table_End$
//
// DESCRIPTION:
//
//    This example configures CPU Timer0, 1, and 2 and increments
//    a counter each time the timers assert an interrupt.
//
//       Watch Variables:
//          CpuTimer0.InterruptCount
//          CpuTimer1.InterruptCount
//          CpuTimer2.InterruptCount
//
//###########################################################################
// $TI Release: DSP2833x/DSP2823x C/C++ Header Files V1.31 $
// $Release Date: August 4, 2009 $
//###########################################################################


#include "DSP28x_Project.h"  // Device Headerfile and Examples Include File
#include <string.h>


#define KB_BUFFER_SIZE 64

#define Second 1000000
#define ENABLE_FALLING_EDGE {GpioDataRegs.GPBSET.bit.GPIO56 = 1; GpioDataRegs.GPBCLEAR.bit.GPIO56 = 1;}
#define LCD_CLEAR 0x01
#define SET_DISPLAY_MODE 0x3C  // 8 bit data with 2 line display and 5x11 dots character format
#define CURSOR_MOVE_R 0x14
#define DISPLAY_ON 0x0F        // display on with blinking cursor
#define DISPLAY_OFF 0x0B        // display off with blinking cursor
#define NEW_LINE 0xbf
#define CURSOR_BLINK 0x0F
#define CURSOR_OFF 0x0C
#define MAX_DELAY 640000L
#define LengthOfPassword 4
#define SizeOfDisplay 32
#define PULLUP 0

#define SW1 GpioDataRegs.GPADAT.bit.GPIO11
#define SW2 GpioDataRegs.GPADAT.bit.GPIO10
#define SW3 GpioDataRegs.GPADAT.bit.GPIO9
#define SW4 GpioDataRegs.GPADAT.bit.GPIO8
#define SW5 GpioDataRegs.GPADAT.bit.GPIO15
#define SW6 GpioDataRegs.GPADAT.bit.GPIO14
#define SW7 GpioDataRegs.GPADAT.bit.GPIO13
#define SW8 GpioDataRegs.GPADAT.bit.GPIO12

//ECCTL1 ( ECAP Control Reg 1)
//==========================
// TSCTRSTOP bit
#define EC_FREEZE 0x0
#define EC_RUN 0x1
// SYNCO_SEL bit
#define EC_SYNCIN 0x0
#define EC_CTR_PRD 0x1
#define EC_SYNCO_DIS 0x2
// CAP/APWM mode bit
#define EC_CAP_MODE 0x0
#define EC_APWM_MODE 0x1
// APWMPOL bit
#define EC_ACTV_HI 0x0
#define EC_ACTV_LO 0x1
// Generic
#define EC_DISABLE 0x0
#define EC_ENABLE 0x1
#define EC_FORCE 0x1

#define EPWM4_CMPA_INIT 0
#define EPWM4_CMPB_INIT 0

#define Password "7397"

#define Buzzer GpioDataRegs.GPADAT.bit.GPIO27
#define ResetTimer Timer = 10
#define LED1 GpioDataRegs.GPCDAT.bit.GPIO64
#define LED2 GpioDataRegs.GPCDAT.bit.GPIO65
#define LED3 GpioDataRegs.GPCDAT.bit.GPIO66

typedef enum {FALSE, TRUE, NONE} TRIPLE_BOOL;
typedef enum {WAIT, LOCK, OPEN, PWM} STATES;

static volatile TRIPLE_BOOL KP_Flag=FALSE;

// Prototype statements for functions found within this file.
interrupt void Xint3456_isr(void);
interrupt void cpu_timer0_isr(void);


STATES current_state = WAIT;
static int tries = 2;
static int Timer = 10;
char Timer_str[3] = "";

inline void Beep(int MiliSec);

void ConfigAndInstallKBInt(void);
void ConfigAndInstallTimerInt(void);
void PWM_Init(void);


void PWM_Mode(void);
void Set_PWM(long double Period, float Duty_Cycle);
char ReadKey(void);
void Gpio_select(void);
void PutS_LCD(const char *String);
void LCD_INIT(char* init_set);
void Clear_Display(void);
long GET_KEYCODE(void);
void SystemStateMachine(void);
void intToString(int num, char *str);
void reverseString(char *str, int length);
char KeyPadToChar(long KEYCODE);
TRIPLE_BOOL PasswordInsert(void);


void main(void)
{

char control_set[]={LCD_CLEAR,SET_DISPLAY_MODE,CURSOR_MOVE_R,DISPLAY_ON}; 	// Control Words To Boot Up LCD


// Step 1. Initialize System Control:
// PLL, WatchDog, enable Peripheral Clocks
// This example function is found in the DSP2833x_SysCtrl.c file.
   InitSysCtrl();

// Step 2. Initialize GPIO:
// This example function is found in the DSP2833x_Gpio.c file and
// illustrates how to set the GPIO to it's default state.
// InitGpio();  // Skipped for this example
   Gpio_select();

// Step 3. Clear all interrupts and initialize PIE vector table:
// Disable CPU interrupts
   DINT;

// Initialize the PIE control registers to their default state.
// The default state is all PIE interrupts disabled and flags
// are cleared.
// This function is found in the DSP2833x_PieCtrl.c file.
   InitPieCtrl();

// Disable CPU interrupts and clear all CPU interrupt flags:
   IER = 0x0000;
   IFR = 0x0000;

// Initialize the PIE vector table with pointers to the shell Interrupt
// Service Routines (ISR).
// This will populate the entire table, even if the interrupt
// is not used in this example.  This is useful for debug purposes.
// The shell ISR routines are found in DSP2833x_DefaultIsr.c.
// This function is found in DSP2833x_PieVect.c.
   InitPieVectTable();

   // Configuration and Installation of KeyPad interrupt

   ConfigAndInstallKBInt();

   // Configuration and Installation of Timer interrupt

   ConfigAndInstallTimerInt();
   
   // initialize PWM
   
   PWM_Init();

   // initialize LCD

   LCD_INIT(control_set);

// Final project code:
  while (TRUE)
    SystemStateMachine();

}


void SystemStateMachine(void) // State machine that counts seconds and displays the counter state to LCD - changes it's state using the KeyPad
{

    TRIPLE_BOOL Flag;

	   switch (current_state)
   {

	case WAIT:

	Flag = PasswordInsert();

	if (Flag == TRUE)
		current_state = OPEN;

	else if (Flag == FALSE)
	    tries--;

	if (!tries || !Timer) // If Timer or number of tries ran out
		current_state = LOCK;

    break;

	case LOCK:

	Clear_Display();
	PutS_LCD("LOCKED -- SW1 to release");
	Beep(0.001*Second);

	if(SW1) // Release from LOCK state
	{
		tries = 2;
		ResetTimer; // Reseting Timer back to 10
		current_state = WAIT;
	}


	break;

	case OPEN: 

	Clear_Display();
	PutS_LCD("Correct Password!");

	ResetTimer; // Reseting Timer back to 10
	current_state = PWM;
	Beep(0.001*Second);

	break;
	
	case PWM: 
	
	PWM_Mode();
	
	Clear_Display();
	PutS_LCD("To lock safe lift SW3");
	DELAY_US((long)(2*Second));
	
	if(SW3)
		current_state = LOCK;
	
	
	break;
   }
}


TRIPLE_BOOL PasswordInsert(void) // OPEN case handling
{
	int i;
	char AttemptedPassword[LengthOfPassword + 1] = "", Digit[2] = "D", TextDisplay[SizeOfDisplay] = "Insert Password:          ";

	intToString(Timer ,Timer_str);
	strcat(TextDisplay, Timer_str);

	Clear_Display();
	PutS_LCD(TextDisplay);

	if(KP_Flag == TRUE)
	{

	for(i = 0 ; i < LengthOfPassword ; i++)
	{
		
	Digit[0] = ReadKey(); // Extracting digit-i of attempted password from KeyPad
	strcat(AttemptedPassword, Digit);

	Clear_Display();
	PutS_LCD(AttemptedPassword);
	DELAY_US((long)(0.5*Second));
	
	}

	KP_Flag = FALSE; // End of presses

	if(!(strcmp(AttemptedPassword, Password)))
		return TRUE;

	Clear_Display();
	PutS_LCD("Wrong Password!  Try Again");
	ResetTimer; // Reseting Timer back to 10
	DELAY_US((long)(1.5*Second));
	return FALSE;
	}

	else
		return NONE; // If no press was made


}

void PWM_Mode(void) // PWM case handling
{ 
	char UserDutyCycle[4] = "", UserPeriod[SizeOfDisplay] = "", WaveParameters[SizeOfDisplay] = "Period(uS): ";
	int DutyCycle, Period;
	
	Clear_Display();
	PutS_LCD("PWM MODE - ACTIVE");
	DELAY_US((long)Second);
	
	
	Clear_Display();
	PutS_LCD("Insert Period(uS) '#' to end:");
	
	Period = ConvertInput(UserPeriod, SizeOfDisplay - 1, '#');
	
	Clear_Display();
	PutS_LCD("Insert DutyCycle(%) '#' to end:");
	
	DutyCycle = ConvertInput(UserDutyCycle, 3, '#');
	
	
	strcat(WaveParameters, UserPeriod);
	strcat(WaveParameters, " ,D.C(%): ");
	strcat(WaveParameters, UserDutyCycle);
	Clear_Display();
	PutS_LCD(WaveParameters); // Display square waveform parameters
	
	Set_PWM((long double)Period, (float)DutyCycle); // Sets parameters to PWM interface
	
	DELAY_US((long)(5*Second)); 
	
	Clear_Display();
	Puts_LCD("SW2 TO EXIT");
	
	while(TRUE)
		if(SW2) break;
	
}


interrupt void Xint3456_isr(void) // Definition of KeyPad inturrupt procedure 
{

    KP_Flag = TRUE; // Gets KeyCode from KeyPad

	// Acknowledge this interrupt to get more from group 12
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP12;
}

interrupt void cpu_timer0_isr(void) // Definition of Timer inturrupt procedure
{

   Timer--;

   // Acknowledge this interrupt to receive more interrupts from group 1
   PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}



void PutS_LCD(const char *String) // Prints String to LCD
{
	int i;

	for(i = 0; String[i] ;i++)		// All 32 Characters to send to LCD
    {
	  	if (i==16)
		{
	  	   GpioDataRegs.GPBDAT.all = (long) NEW_LINE<<16;  // Move cursor to next line start
	  	   GpioDataRegs.GPBCLEAR.bit.GPIO57 = 1;    // LCD RS bit = 0, Instruction Select
       	   ENABLE_FALLING_EDGE
           DELAY_US((long)(0.1*Second));		            // Delay Function
		}

	  	 GpioDataRegs.GPBDAT.all= (long) String[i]<<16;
	  	 GpioDataRegs.GPBSET.bit.GPIO57 = 1;       // LCD RS bit = 1, Data Select
	  	 ENABLE_FALLING_EDGE
		 DELAY_US((long)(0.05*Second));		               //Delay Function

    }

	GpioDataRegs.GPBDAT.all=(long) CURSOR_OFF<<16;
	GpioDataRegs.GPBCLEAR.bit.GPIO57 = 1;  	 // LCD RS bit = 0, Instruction Select
	ENABLE_FALLING_EDGE
    DELAY_US((long)(0.1*Second));		             // Delay Function
}

void Clear_Display(void) // Clears current LCD display
{
	GpioDataRegs.GPBDAT.all=(long)(LCD_CLEAR<<16);   // Instruction send to LCD
	 	GpioDataRegs.GPBCLEAR.bit.GPIO57 = 1;    // LCD RS bit = 0, Instruction Select
	 	ENABLE_FALLING_EDGE
		DELAY_US((long)(0.05*Second));		               // Delay Function
}



int ConvertInput(char* const str, int size, char BreakKey) // Converts input string from user to int type
{
	int i;
	char Digit[2] = "D";
	
	while(KP_Flag == FALSE); // Waiting for button press
	
	for(i = 0; i < size; i++) // Getting String input from User
	{
		Digit[0] = ReadKey();
		
		if(Digit[0] == BreakKey) break; // End Loop (optional)
			
		strcat(str, Digit);
		
		Clear_Display();
		PutS_LCD(str);
		DELAY_US((long)(0.5*Second));
		
	} KP_Flag = FALSE; // Resets Flag
	
	return StringToInt(str);
}

char ReadKey(void) // Extraction of letter pressed on KeyPad
{
   long key; 
	
   GpioDataRegs.GPBDAT.all =0;
   key = GET_KEYCODE();

   GpioDataRegs.GPADAT.all =key; // LEDS ON
   GpioDataRegs.GPCDAT.all =key>>4;
   
   Beep(50); // Beep Press
   
   return KeyPadToChar(key);
}

int BitIndex(int num) // Returns index of 1-bit in number, auxillery function for KeyPadToChar
{
	if(!num) return 0;

	int Index = 1;

	while(num != 1)
	{
		Index++;
		num >>= 1;
	}

	return Index;
}

char KeyPadToChar(long KEYCODE) // Identification of button on KeyPad and return symbol as char type
{
	char PadMat [4][4] = {{'1' , '2', '3', 'A'},
		                  {'4', '5', '6', 'B'},
						  {'7', '8', '9', 'C'},
	                      {'*', '0', '#', 'D'}};

	int row = ((~KEYCODE)>>4)&0x0F, col = (~KEYCODE)&0x0F; // Turn to single 1 bit

	// Make index for columns and rows

	row = BitIndex(row);
	col = BitIndex(col);

	return PadMat[row-1][col-1];

}

long GET_KEYCODE(void) // Returns indication to button on KeyPad according to push
{
	long active_row[4]= {0x0E,0x0D,0x0B,0x07};
    long keycode,row=3,column=0x0F;
	while(column==0x0F)
	{
		row=(row+1)%4;
        GpioDataRegs.GPBDAT.all= active_row[row]<<8;
		DELAY_US(25);
		column = (GpioDataRegs.GPBDAT.all >>12) & 0x0F;
	}
    return keycode=((active_row[row]<<4)|column);
}



void reverseString(char *str, int length)  // Auxillery function
{
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

void intToString(int num, char *str) // Converts int type to string
{
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    int isNegative = 0;
    if (num < 0) {
        isNegative = 1;
        num = -num;
    }

    int index = 0;
    while (num > 0) {
        str[index++] = (num % 10) + '0';
        num /= 10;
    }

    if (isNegative) {
        str[index++] = '-';
    }

    str[index] = '\0';

    reverseString(str, index);
}

int StringToInt(const char *s) // Converts A string to int type  
{
    int result = 0;
    int sign = 1; // Positive by default
    int i = 0;

    // Handle the sign
    if (s[0] == '-') {
        sign = -1;
        i++;
    }

    // Process each character in the string
    while (s[i] != '\0') {
        if (s[i] >= '0' && s[i] <= '9') {
            result = result * 10 + (s[i] - '0');
        } else {
            return 0; // You can return an appropriate value or handle the error differently
        }
        i++;
    }

    return result * sign;
}



inline void Beep(int MiliSec) // Beeps Buzzer
{
   GpioDataRegs.GPASET.bit.GPIO27 = 1; // Buzzer On
   DELAY_US(1000L*MiliSec);
   GpioDataRegs.GPACLEAR.bit.GPIO27 = 1; // Buzzer Off
}

void Set_PWM(long double Period, float Duty_Cycle) // Sets parameters for a square waveform with chosen dutycycle
{
	if(Period <= 0)
	{
		ECap6Regs.CAP1 = 0x0; //Default value for period
		ECap6Regs.CAP2 = 0x0; //Default value for Duty scale
		return;
	}

	Uint32 value_us = (Uint32)(Period * 150); //number of cycles is us * 150MHz
	Duty_Cycle /= 100;


	ECap6Regs.CAP1 = value_us;//Set period value
	ECap6Regs.CAP2 = value_us * Duty_Cycle; //Set Duty cycle to 50 percent (Divide by 2)
}



void LCD_INIT(char* init_set) // Initialize LCD
{
    const int size=(sizeof init_set)/(sizeof(char));
    int i;
    EALLOW;
	for (i=0;i<size;i++)
	{
	 	GpioDataRegs.GPBDAT.all=(long)(init_set[i]<<16);   // Instruction send to LCD
	 	GpioDataRegs.GPBCLEAR.bit.GPIO57 = 1;    // LCD RS bit = 0, Instruction Select
	 	ENABLE_FALLING_EDGE
		DELAY_US((long)(0.1*Second));		               // Delay Function
	}
	EDIS;
}

void PWM_Init(void) // Initialize PWM
{
	EALLOW;

	ECap6Regs.ECCTL2.bit.CAP_APWM = EC_APWM_MODE; //Set to APWM mode
	ECap6Regs.ECCTL2.bit.APWMPOL = EC_ACTV_HI; //Activate-High mode
	ECap6Regs.ECCTL2.bit.SYNCI_EN = EC_ENABLE; //Enable Sync-In
	ECap6Regs.ECCTL2.bit.SYNCO_SEL = EC_SYNCO_DIS; //Disable Sync-out

	ECap6Regs.CAP1 = 0x0; //Set period value
	ECap6Regs.CAP2 = 0x0; //Set the duty cycle
	ECap6Regs.CTRPHS = 0x0; //Set phase delay to zero

	ECap6Regs.ECCTL2.bit.TSCTRSTOP = EC_RUN; //Allow the time-stamp counter to run
	GpioCtrlRegs.GPAMUX1.bit.GPIO1 = 0x2; //Set Gpio 1 for peripheral 2

	EDIS;
}

void ConfigAndInstallTimerInt(void) // Configuration and Installation of Timer interrupt
{
	// Interrupts that are used in this example are re-mapped to
// ISR functions found within this file.
   EALLOW;  // This is needed to write to EALLOW protected registers
   PieVectTable.TINT0 = &cpu_timer0_isr;
   EDIS;    // This is needed to disable write to EALLOW protected registers

// Step 4. Initialize the Device Peripheral. This function can be
//         found in DSP2833x_CpuTimers.c
   InitCpuTimers();   // For this example, only initialize the Cpu Timers

// Configure CPU-Timer 0, 1, and 2 to interrupt every second:
// 150MHz CPU Freq, 1 second Period (in uSeconds)
   ConfigCpuTimer(&CpuTimer0, 150, 1.5*Second);


// Step 5. User specific code, enable interrupts:

   CpuTimer0Regs.TCR.all = 0x4001; // Use write-only instruction to set TSS bit = 0
// Enable CPU int1 which is connected to CPU-Timer 0, CPU int13
// which is connected to CPU-Timer 1, and CPU int 14, which is connected
// to CPU-Timer 2:
   IER |= M_INT1;
   IER |= M_INT13;
   IER |= M_INT14;

// Enable TINT0 in the PIE: Group 1 interrupt 7
   PieCtrlRegs.PIEIER1.bit.INTx7 = 1;

// Enable global Interrupts and higher priority real-time debug events:
   EINT;   // Enable Global interrupt INTM
   ERTM;   // Enable Global realtime interrupt DBGM
}

void ConfigAndInstallKBInt(void) // Configuration and Installation of KeyPad interrupt
{

EALLOW; // This is needed to write to EALLOW protected registers

      // Set input qualification period for GPIO44-GPIO47
      GpioCtrlRegs.GPACTRL.bit.QUALPRD3=1; // Qual period = SYSCLKOUT/2
      GpioCtrlRegs.GPBQSEL1.bit.GPIO44=2; // 6 samples
      GpioCtrlRegs.GPBQSEL1.bit.GPIO45=2; // 6 samples
      GpioCtrlRegs.GPBQSEL1.bit.GPIO46=2; // 6 samples
      GpioCtrlRegs.GPBQSEL1.bit.GPIO47=2; // 6 samples
      GpioIntRegs.GPIOXINT3SEL.all = 12; // Xint3 connected to GPIO44 32+12
      GpioIntRegs.GPIOXINT4SEL.all = 13; // Xint4 connected to GPIO45 32+13
      GpioIntRegs.GPIOXINT5SEL.all = 14; // Xint5 connected to GPIO46 32+14
      GpioIntRegs.GPIOXINT6SEL.all = 15; // Xint6 connected to GPIO47 32+15
      PieVectTable.XINT3 = &Xint3456_isr;
      PieVectTable.XINT4 = &Xint3456_isr;
      PieVectTable.XINT5 = &Xint3456_isr;
      PieVectTable.XINT6 = &Xint3456_isr;

EDIS; // This is needed to disable write to EALLOW protected registers



// Enable Xint 3,4,5,6 in the PIE: Group 12 interrupt 1-4
// Enable int1 which is connected to WAKEINT:
PieCtrlRegs.PIECTRL.bit.ENPIE = 1; // Enable the PIE block
PieCtrlRegs.PIEIER12.bit.INTx1 = 1; // Enable PIE Group 12 INT1
PieCtrlRegs.PIEIER12.bit.INTx2 = 1; // Enable PIE Group 12 INT2
PieCtrlRegs.PIEIER12.bit.INTx3 = 1; // Enable PIE Group 12 INT3
PieCtrlRegs.PIEIER12.bit.INTx4 = 1; // Enable PIE Group 12 INT4


// Enable CPU int12
IER |= M_INT12;

// Configure XINT3-6
XIntruptRegs.XINT3CR.bit.POLARITY = 0; // Falling edge interrupt
XIntruptRegs.XINT4CR.bit.POLARITY = 0; // Falling edge interrupt
XIntruptRegs.XINT5CR.bit.POLARITY = 0; // Falling edge interrupt
XIntruptRegs.XINT6CR.bit.POLARITY = 0; // Falling edge interrupt

// Enable XINT3-6
XIntruptRegs.XINT3CR.bit.ENABLE = 1; // Enable Xint3
XIntruptRegs.XINT4CR.bit.ENABLE = 1; // Enable XINT4
XIntruptRegs.XINT5CR.bit.ENABLE = 1; // Enable Xint5
XIntruptRegs.XINT6CR.bit.ENABLE = 1; // Enable XINT6
}

void Gpio_select(void) // Configuration of Ports
{
    EALLOW;
	GpioCtrlRegs.GPAMUX1.all = 0x00000000;  // All GPIO
	GpioCtrlRegs.GPAMUX2.all = 0x00000000;  // All GPIO
	GpioCtrlRegs.GPBMUX1.all = 0x00000000;  // All GPIO
	GpioCtrlRegs.GPBMUX2.all = 0x00000000;  // All GPIO
	GpioCtrlRegs.GPCMUX1.all = 0x00000000;  // All GPIO
	GpioCtrlRegs.GPCMUX2.all = 0x00000000;  // All GPIO
    GpioCtrlRegs.GPADIR.all = 0x0000000F;   // All outputs 4 LEDs
    GpioCtrlRegs.GPCDIR.all = 0x0000000F;   // All outputs 4 LEDs
    GpioCtrlRegs.GPADIR.bit.GPIO27 = 1;   // for buzzer
	GpioDataRegs.GPACLEAR.bit.GPIO27 = 1;   // buzzer off
    GpioCtrlRegs.GPBDIR.all = 0xFBFF0F00;   // All outputs 4 LEDs
    GpioCtrlRegs.GPBPUD.bit.GPIO44 = PULLUP;
    GpioCtrlRegs.GPBPUD.bit.GPIO45 = PULLUP;
    GpioCtrlRegs.GPBPUD.bit.GPIO46 = PULLUP;
    GpioCtrlRegs.GPBPUD.bit.GPIO47 = PULLUP;
    EDIS;
}

//===========================================================================
// No more.
//===========================================================================
