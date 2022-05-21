/*
 Name:		OverWeg.ino
 Created:	5/15/2022 9:42:04 AM
 Author:	Rob Antonisse

 Sketch voor de techniek van een modelspoor overweg


*/

//byte sb; //shiftbyte GPIOR0 (gaat ff sneller)
# define sb GPIOR0 //gebruik ingebakken register
//bit0 led1; bit1 led2; bit2 led3; bit3 sensL; bit4 sensR ;bit5 switch;bit6 servo1;bit7 servo2
# define flag GPIOR1 //gebruik intern voor snelle flags
//bit0 one shot bit1 vertraging servo timer bit7 switch3 toggle


byte status;
//bit0=M1/s1 bit1=M2/s2 bit2=SW/s0
byte COM_reg;
//bit0=timer on servo1 bit1=timer on servo2


/*
struct Servo
{
	byte reg; //bit0 bit1 bit2 enz
	byte left; //minimaal 20
	byte right; //max 140 geeft ongeveer 160graden met TZT servo
	byte rq; //gewenste eind positie
	byte pos;
	byte speed;
	byte fase; //0=stop 1=run request 2=run
};

Servo servo[2];
*/

byte s1pos; //current position servo 1
byte s1rq;
byte s1speed;
byte s1sc; //speedcount

//byte svf; //focus servo

volatile int SV_count;
byte PRG_fase;

//temp
unsigned long slow;
int count;
void setup() {

	//ports
	DDRB |= (1 << 0); //PB0 pin5 as output SERial
	DDRB |= (1 << 1); //PB1 pin 6 as output RCLK (latch)
	DDRB |= (1 << 2); //PB2 pin7 as output SRCLK (shiftclock)
	DDRB &= ~(1 << 3); PORTB |= (1 << 3); //pin2 as input with pullup
	// Timer and interupts settings servo
	//TCCR1 – Timer / Counter1 Control Register
	TCCR1 = 129; //Counter1 Output Compare RegisterA, no prescaler
	//TCCR1 |= (1 << 0);
	//TCCR1 |= (1 << 1);
	//TCCR1 |= (1 << 2);
	//TCCR1 |= (1 << 7); //clear timer on OCRA1
	//OCR1A –Timer / 
	OCR1A = 100; //duur puls x 0.0625
	//TCNT1 – Timer / Counter1
	//TIMSK – Timer / Counter Interrupt Mask Register
	TIMSK |= (1 << 6); //enable compare A interupt
	//inits
	MEM_read();

	Shift();

}

void MEM_read() {
	//reads variables after power up
	sb = 0;
	s1rq = 60; //beginstand servo1
	//svf = 0;
	//PRG_fase = 0;
	//TIMSK = 0;
	s1speed = 2; //<10 vertragen >10 versnellen
}

ISR(TIMER1_COMPA_vect) {
	//compa A timer 1 =  50 x 0.0625 sec
	if (SV_count > s1pos) { //puls duur bereikt
		//TIMSK &= ~(1 << 6); //disable interupt
		sb &= ~(1 << 6); //reset servo pin	
		TCCR1=0; //stop timer
		//sb ^= (1 << 1);
		Shift();
	}
	TCNT1 = 0;
	SV_count++;
}

void Shift() {
	//shifts sb to external shiftregister
	for (byte i = 7; i < 8; i--) {
		if (sb & (1 << i)) {
			PORTB |= (1 << 0); //serial pin high
		}
		else {
			PORTB &= ~(1 << 0); //serial pin low
		}
		PINB |= (1 << 2); PINB |= (1 << 2); //make SRCLK shift puls
	}
	PINB |= (1 << 1); PINB |= (1 << 1); //make RCLK puls latch to ouputs
}
void read() {
	//leest switches en melders	
	if ((status & (1 << count + 3)) ^ (PINB & (1 << 3)) > 0) { //is status melder of switch veranderd?

		if (PINB & (1 << 3)) { //switch on
			SWoff(count);
		}
		else { //switch off
			SWon(count);
		}

		status &= ~(1 << count + 3); if (PINB & (1 << 3))status |= (1 << count + 3); //nieuwe status switch opslaan
		Shift();
	}

}
void SWon(byte sw) {
	//0=sensor1 1=sensor2 2=switch
	switch (sw) {
	case 0:
		break;
	case 1:
		break;
	case 2: //program en action switch on 
		flag ^= (1 << 7);
		if (flag & (1 << 7)) {
			s1rq = 30; //20
		}
		else {
			s1rq = 220; //140
		}
		break;
	}



	//bits in sb zijn gelijk aan sw 0~2 
	sb ^= (1 << sw);
	Shift();

}
void SWoff(byte sw) {

}
void SV_control() {
	/*
	controls de servo's
	Servo pin hoogzetten, timer 1 starten. aantal interupts tellen totaal gewenste pulslengte is bereikt
	Timer stoppen, servopin laagzetten.

	*/
	byte speed;
	if (s1pos == s1rq) { //servo staat gewenste positie

	}
	else { //servo moet verplaatsen

		if (s1speed == 0) {
			speed = 1;
			flag ^= (1 << 1);
		}
		else {
			speed = s1speed;
			flag |= (1 << 1);
		}

		if (flag & (1 << 1)) {
			if (s1pos > s1rq) {
				s1pos = s1pos - speed;
			}
			else {
				s1pos = s1pos + speed;
			}
		}


		SV_count = 0; //reset puls counter
		//s1pos = s1rq;
		sb |= (1 << 6); //set servo pin	
		//sb ^= (1 << 1);
		Shift();
		//TIMSK |= (1 << 6); //enable interupt
		TCNT1 = 0;
		TCCR1 =129; // start timer no prescaler

	}
}

void loop() {
	if ((millis() - slow) > 5 & (~flag & (1 << 0))) {
		SV_control(); //ieder 20ms eenmalig in de 'pauze'
		flag |= (1 << 0); //set flag one shot
	}

	if (millis() - slow > 20) {
		slow = millis();
		flag &= ~(1 << 0); //reset one-shot flag
		//GPIOR1 &=~(1<< 0); //reset flag
		//COM_reg &= ~(3 << 0); //reset flags servo1 and servo 2
		read(); //lees switch
		//switch & sensors scannen
		count++;
		if (count > 2)count = 0;
		sb &= ~(56 << 0); //clear bit 3-4-5
		sb |= (1 << count + 3); //opvolgend 3 4 5

		Shift();

	}

}
