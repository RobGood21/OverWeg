/*
 Name:		OverWeg.ino
 Created:	5/15/2022 9:42:04 AM
 Author:	Rob Antonisse

 Sketch voor de techniek van een modelspoor overweg


*/

//byte sb; //shiftbyte GPIOR0 (gaat ff sneller)


# define apf 3 //aantal program fases
# define at 4 //aantal timers
# define uit sb &= ~(7 << 0); //zet leds uit

# define sb GPIOR0 //gebruik ingebakken register
//bit0 led1; bit1 led2; bit2 led3; bit3 sensL; bit4 sensR ;bit5 switch;bit6 servo1;bit7 servo2
# define flag GPIOR1 //gebruik intern voor snelle flags
//bit0 one shot 5ms(servo 1); 
//bit1 one shot 10ms (servo2); 
//bit2 flag in read, misschien is dit anders slimmer te doen?
//bit3 flag opstarten naar programmode 1 gaan aan switch is ingedrukt
//bit5 vertraging servo timer; bit6 switch 2 toggle; bit7 switch3 toggle

byte status = 7; //bit0=M1/s1 bit1=M2/s2 bit2=SW/s0

struct Servo
{
	byte reg; //bit0=timer on off
	byte left; //minimaal 20 uit eeprom
	byte right; //max 140 geeft ongeveer 160graden met TZT servo
	volatile byte rq; //gewenste eind positie
	volatile byte pos;
	volatile byte speed;
	unsigned int timer;
	unsigned int timercount;
};
Servo servo[2];

byte timer[at]; //timers 3x? 
byte timercount[at];
byte timerlot[at];//(lot=leds on timer) effect op welke leds? 0~2 leds on 3~5 leds off
byte timerseq[at];
byte tc = 10; //timing moment of timers




volatile byte sf = 0; //servo focus welke servo wordt aangestuurd, alleen in loop gebruiken
byte sv; //focus welke servo in program mode

byte pfase = 0; //pf=program fase
byte plevel = 0; //program level
//timers, counters
volatile byte pc[2];  //pulscount in ISR
byte switchcount; //teller voor ingedrukt houden knop 3
unsigned long oldmillis;
byte slowcount;
int count;
byte countlp; //count long press van knop 2
//temp


void setup() {
	//ports
	DDRB |= (1 << 0); //PB0 pin5 as output SERial
	DDRB |= (1 << 1); //PB1 pin 6 as output RCLK (latch)
	DDRB |= (1 << 2); //PB2 pin7 as output SRCLK (shiftclock)
	DDRB &= ~(1 << 3); PORTB |= (1 << 3); //pin2 as input with pullup
	DDRB &= ~(1 << 4); PORTB |= (1 << 4); //pin3 as input pullup, for program switch
	// Timer and interupts settings servo
	TCCR1 = 129; //Counter1 Output Compare RegisterA, no prescaler
	OCR1A = 100; //duur puls x 0.0625
	TIMSK |= (1 << 6); //enable compare A interupt
	//inits
	MEM_read();

	//start programma mode, merk op overweg mag niet gesloten zijn 
	//Shift();
}
void MEM_read() {
	//reads EEPROM variables after power up

	servo[0].speed = 1;
	servo[1].speed = 1;
	//servo[0].pos = 50;
	//servo[1].pos = 50;

	//uit EEprom halen standen servo
	servo[0].left = 90;
	servo[0].right = 120;
	servo[1].left = 90;
	servo[1].right = 120;

	servo[0].rq = servo[0].left; //beginstand servo1
	servo[1].rq = servo[1].left;
	servo[0].pos = servo[0].left;
	servo[1].pos = servo[0].left;

	sb = 0;
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
	//progam switch =status bit 4
	if (status & (1 << 4) && ~PINB & (1 << 4)) {
		SWon(4);
	}
	else if (~status & (1 << 4) && PINB & (1 << 4)) {
		SWoff(4);
	}
	if (PINB & (1 << 4)) {
		status |= (1 << 4);
	}
	else {
		status &= ~(1 << 4);
	}
	//leest switches en melders	bit 0~2  sensors and switch

	if (status & (1 << count) && ~PINB & (1 << 3)) {
		SWon(count);
	}
	else if (~status & (1 << count) && PINB & (1 << 3)) {
		SWoff(count);
	}
	else if (~status & (1 << 2) && ~PINB & (1 << 3)) {
		countlp++;
		if (countlp > 50) {
		countlp = 0; //reset teller
			//if (~GPIOR1 & (1 << 2)) { //one shot longpress
			//	GPIOR1 |= (1 << 2);
			longpress(); //longpress 
	
		//}
		}
	}
	//huidige schakel stand opslaan in status bit
	if (PINB & (1 << 3)) { //read pin high
		status |= (1 << count);
	}
	else { //read pin low
		status &= ~(1 << count);
	}
}
void longpress() {

	switch (plevel) {
	case 0: //servo instellen snelheid 
		//enkel knipper, enkele knipper op andere led
		break;
	case 1: //servo instellen posities
		//dubbel knipper
		break;
	}

	plevel++;
	switch (pfase) {
	case 2: //instellen aantal levels in deze pfase
		if (plevel > 2)plevel = 0;
		break;
	}
	sequence(50);


}

void sequence(byte seq) { //od=open dicht open false dicht true
	//temp
	byte b = 0;
	//cyclus, sequence

	switch (seq) {
	case 0:
		//geen actie
		break;
	case 1: // schakel alle leds aan
		sb |= (7 << 0);
		break;
	case 2: //program mode back to 0 
		settimer(0, 10, B111000, 3); //10ms wachten
		break;
	case 3:
		settimer(0, 20, B000111, 4); //400ms leds uit
		break;
	case 4:
		settimer(0, 50, B111000, 0); //1sec leds aan, daarna in bedrijf
		break;
	case 10:
		//start bel, start lights/sluit bomen 
		for (byte i = 0; i < 2; i++) {
			servo[i].reg &= ~(1 << 1);
			servo[i].reg |= (1 << 0);
			servo[i].timer = (random(3, 30));
			servo[i].timercount = 0;
		}
		break;

	case 20: //open bomen
		for (byte i = 0; i < 2; i++) {
			servo[i].reg &= ~(1 << 0);
			servo[i].reg |= (1 << 1);
			servo[i].timer = (random(3, 30));
			servo[i].timercount = 0;
		}
		break;
	case 30: //Test servo 1 pfase=2 knipper led links
		b = B100000;
		if (sv == 1)b = B010000;
		settimer(1, 2, b, 31);
		break;

	case 31:
		b = B100;
		if (sv == 1)b = B010;
		settimer(1, 80, b, 30);
		break;

	case 50: //start positie program servo
		servo[sv].rq = servo[sv].left;// (servo[sv].left + servo[sv].right) / 2;
		settimer(2, 5, 0, 51); //wachtlus totdat servo op positie is
		break;

	case 51:
		if (servo[sv].rq == servo[sv].pos) {
			settimer(2, 5, 0, 52); //servo op positie
		}
		else {
			settimer(2, 5, 0, 51); //wachtlus totdat servo op positie is		
		}
		break;

	case 52: //servo op positie, wissel richting

		if (servo[sv].pos == servo[sv].left) {
			servo[sv].rq = servo[sv].right;
		}
		else {
			servo[sv].rq = servo[sv].left;
		}
		settimer(2, 5, 0, 51); //wachtlus totdat servo op positie is
		break;

	}

}


void settimer(byte number, int time, byte lot, byte seq) {
	//number= welke timer
	//time=tijdx20ms
	//lot=payload 0~2 leds 1~3 aan   3~5  leds 1~3 uit
	//seq=wat welke seq uitvoeren na afloop timer
	timercount[number] = 0;
	timer[number] = time;
	timerlot[number] = lot;
	timerseq[number] = seq;
}


void timers() { //called from loop 20ms
	//led timers
	for (byte i = 0; i < at; i++) {
		if (timer[i] > 0) { //timer actief
			timercount[i]++;
			if (timer[i] < timercount[i]) { //timer afgelopen, uitvoeren
				for (byte b = 0; b < 6; b++) {
					if (timerlot[i] & (1 << b)) {
						if (b < 3) {
							sb |= (1 << b);
						}
						else {
							sb &= ~(1 << b - 3);
						}
					}
				}
				timer[i] = 0; //reset free timer
				sequence(timerseq[i]);//next proces in sequencer
			}
		}
	}
	//servo timers (voor de start random)
	for (byte i = 0; i < 2; i++) {
		if (servo[i].reg & (1 << 0)) { //timer aan?	
			servo[i].timercount++;
			if (servo[i].timercount > servo[i].timer) {
				servo[i].rq = servo[i].right;
				servo[i].reg &= ~(1 << 0); //stop timer
			}
		}
		else if (servo[i].reg & (1 << 1)) { //timer aan?	{			
			servo[i].timercount++;
			if (servo[i].timercount > servo[i].timer) {
				servo[i].rq = servo[i].left;
				servo[i].reg &= ~(1 << 1); //stop timer
			}
		}
	}
}

void SWon(byte sw) {
	//0=sensor1 1=sensor2 2=switch
	//sb |= (1 << sw); //temp led aan

	switch (pfase) {
	case 0:
		switch (sw) {
		case 0:
			//sv = 1;

			//seq = 52;
			break;
		case 1:
			break;
		case 2:
			sequence(10);//sluit overweg
			break;
		}
		break;

	case 1: //pfase=1 led test

		switch (sw) {
		case 0:
			break;
		case 1:
			break;
		case 2:
			break;
		}
		break;

	case 2: //pfase=2 servo  test

		switch (sw) {
		case 0:
			break;
		case 1:
			break;
		case 2:
			if (sv == 0) {
				GPIOR1 ^= (1 << 6);
				if (GPIOR1 & (1 << 6)) {
					servo[0].rq = servo[0].right;
				}
				else {
					servo[0].rq = servo[0].left;
				}
			}
			else {
				GPIOR1 ^= (1 << 7);
				if (GPIOR1 & (1 << 7)) {
					servo[1].rq = servo[1].right;
				}
				else {
					servo[1].rq = servo[1].left;
				}
			}
			break;
		}

		break;

	case 3:
		break;
	}


	if (sw == 4) {
		//program switch aparte knop op PB4, moet altijd onafhankelijk van program fase
		switch (pfase) {
		case 2: //uitzondering voor de twee servoos
			if (sv == 0) {
				sv = 1;
			}
			else {
				pfase++;
				sv = 0;
			}
			break;
		default:
			pfase++;
			if (pfase > apf)pfase = 0; //apf aantal program fases zie #define
			break;
		}
		Programs();
	}
}

void SWoff(byte sw) {
	if (sw == 2)countlp = 0; //reset longpress timer

	//sb &= ~(1 << sw); //temp led off

	switch (pfase) {
	case 0:
		switch (sw) {
		case 0:
			break;
		case 1:
			break;
		case 2:
			sequence(20);// bomen open dus de overweg
			break;
		}
		break;
	case 2: //servo 1 test 
		break;
	case 3: //servo 2 test
		break;
	}
}

void Programs() {
	//reset all timers
	for (byte i = 0; i < at; i++) {
		timer[i] = 0;
	}
	GPIOR1 &= ~(1 << 2);  //reset one shot long press

	switch (pfase) {
	case 0:
		//set program mode naar in bedrijf...
		//leds uit_aan_uit
		sequence(2);
		break;
	case 1: //alle leds on
		sequence(1);
		break;
	case 2: //servo 1 test
		uit;
		sequence(30);
		break;
	case 3: //servo 2 test
		break;
	}
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
void slow() {
	slowcount++;

	//servo1
	if (slowcount == 2) {
		sf = 0; //servo 1
		SV_control(); //ieder 20ms
	}
	//servo 2
	if (slowcount == 5) {
		sf = 1; //servo 1		
		SV_control(); //ieder 20ms eenmalig in de 'pauze'
	}

	if (slowcount == 10) {
		timers();
	}


	//switch & sensors scannen
	if (slowcount == 20) {
		read(); //lees switch 
		count++;
		if (count > 2)count = 0;
		sb &= ~(56 << 0); //clear bit 3-4-5
		sb |= (1 << count + 3); //opvolgend 3 4 5 om sensors en switch te scannen
		Shift();
	}

	if (slowcount > 19) slowcount = 0;
}

void loop() {
	//millis counter
	if (millis() > oldmillis) {
		oldmillis = millis();
		slow();
	}
}
