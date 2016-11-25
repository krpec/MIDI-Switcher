#define VERSION_MAJOR 1
#define VERSION_MINOR 0
//
////////////////////////////////////////////////////////////

#include <system.h>
#include <memory.h>
#include <eeprom.h>


// CONFIG OPTIONS 
// - RESET INPUT DISABLED
// - WATCHDOG TIMER OFF
// - INTERNAL OSC
#pragma DATA _CONFIG1, _FOSC_INTOSC & _WDTE_OFF & _MCLRE_OFF &_CLKOUTEN_OFF
#pragma DATA _CONFIG2, _WRT_OFF & _PLLEN_OFF & _STVREN_ON & _BORV_19 & _LVP_OFF
#pragma CLOCK_FREQ 16000000

#define CC_DISABLE 0x80
#define CC_PORT0 71
#define CC_PORT1 72
#define CC_PORT2 73
#define CC_PORT3 74
#define CC_PORT4 75
#define CC_PORT5 76
#define CC_PORT6 77
#define CC_PORT7 78




/*
Original MIDI Switcher
		
		VDD - VSS
LED		RA5	- RA0/PGD	P7
SW		RA4 - RA1/PGC	P6
		VPP - RA2		P5
RX		RC5 - RC0		P4
P0		RC4 - RC1		P1
P2		RC3 - RC2		P3
	
	
	
PWM OUTPUTS


CCP1	RC5
CCP2	RA5
CCP3	RA2
CCP4	RC1
		
*/
#define P_OUT0		latc.4 
#define P_OUT1		latc.1 
#define P_OUT2		latc.3
#define P_OUT3		latc.2 
#define P_OUT4		latc.0
#define P_OUT5		lata.2 
#define P_OUT6		lata.1
#define P_OUT7		lata.0

#define P_OUT0_CBIT		(1<<4)
#define P_OUT1_CBIT		(1<<1) 
#define P_OUT2_CBIT		(1<<3)
#define P_OUT3_CBIT		(1<<2) 
#define P_OUT4_CBIT		(1<<0)
#define P_OUT5_ABIT		(1<<2) 
#define P_OUT6_ABIT		(1<<1)
#define P_OUT7_ABIT		(1<<0)


#define P_LED		porta.5
#define P_MODE		porta.4

					//76543210
#define TRIS_A		0b11011000
#define TRIS_C		0b11100000


#define P_WPU		wpua.4



typedef unsigned char byte;


volatile byte midi_status = 0;
volatile byte midi_param = 0;
volatile byte which_cc = 0;
									
// the duty at each port - full duty is 127
volatile byte duty0 = 0;
volatile byte duty1 = 0;
volatile byte duty2 = 0;
volatile byte duty3 = 0;
volatile byte duty4 = 0;
volatile byte duty5 = 0;
volatile byte duty6 = 0;
volatile byte duty7 = 0;

rom char *gamma = {
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 1, 1, 
1, 1, 1, 1, 2, 2, 2, 2, 
3, 3, 3, 3, 4, 4, 5, 5, 
6, 6, 7, 7, 8, 8, 9, 10, 
10, 11, 12, 13, 13, 14, 15, 16, 
17, 18, 19, 20, 21, 22, 24, 25, 
26, 27, 29, 30, 32, 33, 35, 36, 
38, 39, 41, 43, 45, 47, 49, 50, 
52, 55, 57, 59, 61, 63, 66, 68, 
70, 73, 75, 78, 81, 83, 86, 89, 
92, 95, 98, 101, 104, 107, 110, 114, 
117, 120, 124, 127, 131, 135, 138, 142, 
146, 150, 154, 158, 162, 167, 171, 175, 
180, 184, 189, 193, 198, 203, 208, 213, 
218, 223, 228, 233, 239, 244, 249, 255 };

typedef struct {
	byte first;
	byte second;
	byte next;
} PWM_PAIR;

PWM_PAIR port01;
PWM_PAIR port23;
PWM_PAIR port45;
PWM_PAIR port67;

#define ON_CCP_INT(_pair, _portbit1, _portbit2) 	\
	t1con.0 = 0; 									\
	_portbit1 = (tmr1l < _pair.first);  			\
	_portbit2 = (tmr1l < _pair.second); 			\
	ccpr1l = _pair.next; 							\
	t1con.0 = 1;						

#define SET_CCP_INT(_pair, _ccpie, _ccpr, _pwm1, _pwm2) \
	_pair.first = _pwm1; 							\
	_pair.second = _pwm2; 							\
	_pair.next = (_pwm1 > _pwm2)? _pwm1 : _pwm2; 	\
	_ccpr  = (_pwm1 > _pwm2)? _pwm2 : _pwm1; 		\
	_ccpie = !!_ccpr; 								
		
#define INIT_STATE(_pair, _portbit1, _portbit2) 	\
	_portbit1 = !!_pair.first; 							\
	_portbit2 = !!_pair.second;

////////////////////////////////////////////////////////////
// INTERRUPT SERVICE ROUTINE
void interrupt( void )
{

	
	////////////////////////////////////////////////////////////
	// CCP compare interrupt
	if(pir1.CCP1IF) 
	{
		ON_CCP_INT(port01, P_OUT0, P_OUT1);
		pir1.CCP1IF = 0;
	}
	if(pir2.CCP2IF) 
	{
		ON_CCP_INT(port23, P_OUT2, P_OUT3);
		pir2.CCP2IF = 0;
	}
	if(pir3.CCP3IF) 
	{
		ON_CCP_INT(port45, P_OUT4, P_OUT5);
		pir3.CCP3IF = 0;
	}	
	if(pir3.CCP4IF) 
	{
		ON_CCP_INT(port67, P_OUT6, P_OUT7);
		pir3.CCP4IF = 0;
	}
	////////////////////////////////////////////////////////////
	
	/////////////////////////////////////////////////////
	// timer 1 overflow - at end of PWM cycle
	if(pir1.TMR1IF) 
	{
		// clear interrupt
		pir1.TMR1IF = 0;		
		
		SET_CCP_INT(port01, pie1.CCP1IE, ccpr1l, duty0, duty1);
		SET_CCP_INT(port23, pie2.CCP2IE, ccpr2l, duty2, duty3);
		SET_CCP_INT(port45, pie3.CCP3IE, ccpr3l, duty4, duty5);
		SET_CCP_INT(port67, pie3.CCP4IE, ccpr4l, duty6, duty7);
		
		INIT_STATE(port01, P_OUT0, P_OUT1);
		INIT_STATE(port23, P_OUT2, P_OUT3);
		INIT_STATE(port45, P_OUT4, P_OUT5);
		INIT_STATE(port67, P_OUT6, P_OUT7);
		
		// reset the timer
		tmr1h = 255;
		tmr1l = 0;
				
	}
	/////////////////////////////////////////////////////
	

	
	// SERIAL RECEIVE CHARACTERR
	if(pir1.5)
	{
		// get the byte
		byte b = rcreg;
		if(!!(b & 0x80)) {
			midi_status = b;
			midi_param = 1;
		}
		else if(midi_status == 0xB0) {
			P_LED = 1;
			if(midi_param == 1) {
				which_cc = b;
				midi_param = 2;
			}
			else 
			{
				switch(which_cc) {
					case CC_PORT0: duty0 = b; break;
					case CC_PORT1: duty1 = b; break;
					case CC_PORT2: duty2 = b; break;
					case CC_PORT3: duty3 = b; break;
					case CC_PORT4: duty4 = b; break;
					case CC_PORT5: duty5 = b; break;
					case CC_PORT6: duty6 = b; break;
					case CC_PORT7: duty7 = b; break;
				}
				midi_param = 1;
			}
		}
		pir1.5 = 0;
	}
}


////////////////////////////////////////////////////////////
// INITIALISE SERIAL PORT FOR MIDI
void init_usart()
{
	pir1.1 = 1;		//TXIF 		
	pir1.5 = 0;		//RCIF
	
	pie1.1 = 0;		//TXIE 		no interrupts
	pie1.5 = 1;		//RCIE 		enable
	
	baudcon.4 = 0;	// SCKP		synchronous bit polarity 
	baudcon.3 = 1;	// BRG16	enable 16 bit brg
	baudcon.1 = 0;	// WUE		wake up enable off
	baudcon.0 = 0;	// ABDEN	auto baud detect
		
	txsta.6 = 0;	// TX9		8 bit transmission
	txsta.5 = 0;	// TXEN		transmit enable
	txsta.4 = 0;	// SYNC		async mode
	txsta.3 = 0;	// SEDNB	break character
	txsta.2 = 0;	// BRGH		high baudrate 
	txsta.0 = 0;	// TX9D		bit 9

	rcsta.7 = 1;	// SPEN 	serial port enable
	rcsta.6 = 0;	// RX9 		8 bit operation
	rcsta.5 = 1;	// SREN 	enable receiver
	rcsta.4 = 1;	// CREN 	continuous receive enable
		
	spbrgh = 0;		// brg high byte
	spbrg = 30;		// brg low byte (31250)		
	
}


////////////////////////////////////////////////////////////
// MAIN
void main()
{ 
	int i;
	
	// osc control / 16MHz / internal
	osccon = 0b01111010;
		
	trisa = TRIS_A;
	trisc = TRIS_C;

	ansela = 0b00000000;
	anselc = 0b00000000;

	porta = 0;
	portc = 0;
	
	P_WPU = 1; // weak pull up on switch input
	option_reg.7 = 0; // weak pull up enable


	// initialise MIDI comms
	init_usart();
	
	// Configure timer 0 (controls pwm clock)
	// 	timer 0 runs at 4MHz
	//option_reg.5 = 0; // timer 0 driven from instruction cycle clock
	//option_reg.3 = 1; // } prescalar
	//option_reg.2 = 0; // }
	//option_reg.1 = 0; // } 
	//option_reg.0 = 0; // }
	//intcon.5 = 1; 	  // enabled timer 0 interrrupt
	//intcon.2 = 0;     // clear interrupt fired flag


//	pie1.2 = 0;		// CCP1IE
//	pie2.0 = 0;		// CCP2IE
//	pie3.4 = 0;		// CCP3IE
//	pie3.5 = 0;		// CCP4IE


	// set each compare module to generate interrupt only
	ccp1con = 0b00001010; 
	ccp2con = 0b00001010; 
	ccp3con = 0b00001010; 
	ccp4con = 0b00001010; 

	// the high byte of each compare register is 255
	ccpr1h = 255;
	ccpr2h = 255;
	ccpr3h = 255;
	ccpr4h = 255;
	
	// enable interrupts	
	intcon.7 = 1; //GIE
	intcon.6 = 1; //PEIE	

	// initialise timer1
	tmr1h = 0;
	tmr1l = 0;
	pir1.TMR1IF = 0;		
	
	
	pie1.TMR1IE = 1; 	// enable timer 1 interrupt
	t1con.7 = 0;	// }	drive from instruction clock
	t1con.6 = 0;	// }
	t1con.5 = 1;	// } 	prescale
	t1con.4 = 1;	// }
	t1con.3 = 0;	
	t1con.2 = 1;	// do not sync with external input
	//t1con.1 = 0;	// unimplemented
	t1con.0 = 1;	// enable timer 1

	
	duty0 = 1;
	duty1 = 2;
	duty2 = 4;
	duty3 = 8;
	duty4 = 16;
	duty5 = 32;
	duty6 = 64;
	duty7 = 128;
	while(1);

}