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
//bit0 one shot 5ms(servo 1); bit1 one shot 10ms (servo2); bit5 vertraging servo timer; bit6 switch 2 toggle; bit7 switch3 toggle


byte status; //bit0=M1/s1 bit1=M2/s2 bit2=SW/s0

struct Servo
{
	byte reg; //bit0=timer on off
	byte left; //minimaal 20 uit eeprom
	byte right; //max 140 geeft ongeveer 160graden met TZT servo
	volatile byte rq; //gewenste eind positie
	volatile byte pos;
	volatile byte speed;
	int timer;
	int timercount;
};
Servo servo[2];


volatile byte sf = 0; //servo focus welke servo wordt aangestuurd
byte seq; //fase van sequentie
//timers, counters
volatile byte pc[2];  //pulscount in ISR
byte switchcount; //teller voor ingedrukt houden knop 3
unsigned long slow;
int count;

//temp


void setup() {
	//ports
	DDRB |= (1 << 0); //PB0 pin5 as output SERial
	DDRB |= (1 << 1); //PB1 pin 6 as output RCLK (latch)
	DDRB |= (1 << 2); //PB2 pin7 as output SRCLK (shiftclock)
	DDRB &= ~(1 << 3); PORTB |= (1 << 3); //pin2 as input with pullup
	// Timer and interupts settings servo
	TCCR1 = 129; //Counter1 Output Compare RegisterA, no prescaler
	OCR1A = 100; //duur puls x 0.0625
	TIMSK |= (1 << 6); //enable compare A interupt
	//inits
	MEM_read();
	Shift();
}

void MEM_read() {
	//reads EEPROM variables after power up
	servo[0].rq = 120; //beginstand servo1
	servo[1].rq = 120;
	servo[0].speed = 1;
	servo[1].speed = 1;
	servo[0].pos = 50;
	servo[1].pos = 50;
	sb = 0;

	seq = 0;

	//s1speed = 2; //<10 vertragen >10 versnellen
}

ISR(TIMER1_COMPA_vect) {
	//compa A timer 1 =  50 x 0.0625 sec
	if (pc[sf] > servo[sf].pos) { //puls duur bereikt
		sb &= ~(1 << (6 + sf)); //reset servo pin sf = 0 of 1)	
		//sb ^= (1 << 0);
		TCCR1 = 0; //stop timer
		Shift();
	}
	TCNT1 = 0;
	pc[sf] ++;
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
	//leest switches en melders	bit 0~2  sensors and switch
	GPIOR1 &= ~(1 << 2);
	if (status & (1 << count) && PINB & (1 << 3)) GPIOR1 |= (1 << 2);
	if (~status & (1 << count) && ~PINB & (1 << 3)) GPIOR1 |= (1 << 2);
	if (~GPIOR1 & (1 << 2)) { //is status melder of switch veranderd? lelijke oplossing.
		if (PINB & (1 << 3)) { //switch off
			SWoff(count);
		}
		else { //switch on
			SWon(count);
		}
	}
	else { //niks veranderd test of knop ingedrukt wordt gehouden??

	}

	Shift();
	//huidige schakel stand opslaan in status bit
	if (PINB & (1 << 3)) { //read pin high
		status |= (1 << count);
	}
	else { //read pin low
		status &= ~(1 << count);
	}
}
void sluit() {
	seq = 10;
}
void open() {
	seq = 20;
}
void sequence() { //od=open dicht open false dicht true
	//cyclus, sequence

	switch (seq) {
	case 0:
		//geen actie
		break;

	case 10:
		//start bel, start lights


			//sluit bomen 
		for (byte i = 0; i < 2; i++) {
			servo[i].reg &= ~(1 << 1);
			servo[i].reg |= (1 << 0);
			servo[i].timer = (random(3, 100));
			servo[i].timercount = 0;
		}
		break;

	case 20: //open bomen
		for (byte i = 0; i < 2; i++) {
			servo[i].reg &= ~(1 << 0);
			servo[i].reg |= (1 << 1);
			servo[i].timer = (random(3, 100));
			servo[i].timercount = 0;
		}
		break;
	}
	seq = 0; //altijd?
}
void timers() { //called from loop 20ms

	//servo timers (voor de start random)
	for (byte i = 0; i < 2; i++) {
		if (servo[i].reg & (1 << 0)) { //timer aan?	
			servo[i].timercount++;
			if (servo[i].timercount > servo[i].timer) {
				sluitboom(i);
				servo[i].reg &= ~(1 << 0); //stop timer
			}
		}
		else if (servo[i].reg & (1 << 1)) { //timer aan?	{			
			servo[i].timercount++;
			if (servo[i].timercount > servo[i].timer) {
				openboom(i);
				servo[i].reg &= ~(1 << 1); //stop timer
			}
		}
	}
}

void sluitboom(bool b) {
	if (b) {
		servo[0].rq = 80;
	}
	else {
		servo[1].rq = 80;
	}
}
void openboom(bool b) {
	if (b) {
		servo[0].rq = 180;
	}
	else {
		servo[1].rq = 180;
	}
}

void SWonnew(byte sw) {
	sb |= (1 << sw);
	Shift();
}
void SWon(byte sw) {
	//0=sensor1 1=sensor2 2=switch
	switch (sw) {
	case 0:
		flag ^= (1 << 6);
		if (flag & (1 << 6)) {
			servo[0].rq = 80;
		}
		else {
			servo[0].rq = 180;
		}

		break;
	case 1:
		flag ^= (1 << 6);
		if (flag & (1 << 6)) {
			servo[1].rq = 80;
		}
		else {
			servo[1].rq = 180;
		}
		break;
	case 2: //program en action switch on 
		sluit(); //bomen sluiten
		break;
	}
}
void SWoff(byte sw) {
	//sb ^= (1 << 1);
	switch (sw) {
	case 2:
		open();
		break;
	}
	//Shift();
}
void SV_control() {
	/*
	controls de servo's
	Servo pin hoogzetten, timer 1 starten. aantal interupts tellen totaal gewenste pulslengte is bereikt
	Timer stoppen, servopin laagzetten.
	In loop wordt bepaald sf= servo focus, welke servo wordt aangestuurd.

	*/

	//sb ^= (1 << 0);

	byte speed;
	if (servo[sf].pos == servo[sf].rq) { //s1rq) 
		 //servo staat gewenste positie
		  //blijkbaar gebeurt er hier niks nadda...?

	}
	else { //servo moet verplaatsen

		if (servo[sf].speed == 0) {
			speed = 1;
			flag ^= (1 << 5);
		}
		else {
			speed = servo[sf].speed;
			flag |= (1 << 5);
		}

		if (flag & (1 << 5)) {
			if (servo[sf].pos > servo[sf].rq) { //  s1rq) {
				servo[sf].pos = servo[sf].pos - speed;
			}
			else {
				servo[sf].pos = servo[sf].pos + speed;
			}
		}


		pc[sf] = 0; //reset puls counter, in ISR
		sb |= (1 << 6 + sf); //set servo pin	
		//sb ^= (1 << 0);
		Shift();
		//TIMSK |= (1 << 6); //enable interupt, is altijd aan.... timer zelf wordt geschakeld.
		TCNT1 = 0;
		TCCR1 = 129; // start timer no prescaler

	}
}

void loop() {

	sequence(); //gebeurtenissen
	if ((millis() - slow > 1) && (~flag & (1 << 0))) {
		sf = 0; //servo 1
		flag |= (1 << 0); //set flag one shot
		SV_control(); //ieder 20ms eenmalig in de 'pauze'
	}
	if ((millis() - slow > 12) && (~flag & (1 << 1))) {
		sf = 1; //servo 1
		flag |= (1 << 1); //set flag one shot
		SV_control(); //ieder 20ms eenmalig in de 'pauze'
	}



	if (millis() - slow > 20) {
		slow = millis();
		flag &= ~(1 << 0); //reset one-shot flags 0  and 1
		flag &= ~(1 << 1);

		read(); //lees switch
		//switch & sensors scannen
		count++;
		if (count > 2)count = 0;
		sb &= ~(56 << 0); //clear bit 3-4-5
		sb |= (1 << count + 3); //opvolgend 3 4 5 om sensors en switch te scannen
		Shift();
		timers();
	}

}
