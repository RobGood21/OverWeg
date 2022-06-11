/*
 Name:		OverWeg.ino
 Created:	5/15/2022 9:42:04 AM
 Author:	Rob Antonisse

 Sketch voor de techniek van een modelspoor overweg


*/

//byte sb; //shiftbyte GPIOR0 (gaat ff sneller)

#include <EEPROM.h>

# define Ahob 15 //nieuwe ahob snelheid 90x per minuut 
# define Ahobold 32 //oude ahob 45x per minuut
# define starttijdbomen 50 //hoelang minimaal tussen starten knipperlicht en sluiten bomen
//????starttijdbomen instelling van maken



# define apf 3 //aantal program fases
# define at 6 //aantal timers
# define uit sb &= ~(7 << 0); //zet leds uit
# define aan sb |=(7<<0); //leds aan

# define servomin 30  //minimaal bereik open
# define servomax 240 //maximaal bereik close
# define sb GPIOR0 //gebruik ingebakken register
byte status = 7; //bit0=M1/s1 bit1=M2/s2 bit2=SW/s0
struct Servo
{
	byte reg; //bit0=timer on  bit1=open(false)/close 
	byte open; //minimaal 20 uit eeprom
	byte close; //max 140 geeft ongeveer 160graden met TZT servo
	volatile byte rq; //gewenste eind positie
	volatile byte pos;
	unsigned int timer;
	unsigned int timercount;
};
Servo servo[2];
byte countservo = 5;
byte speedservo = 0;
byte tempspeedservo = 0;
unsigned int timer[at]; //ingestelde tijd x20ms
uint16_t timercount[at]; //timerteller per 20ms 
byte timerlot[at];//(lot=leds on timer) effect op welke leds? 0~2 leds on 3~5 leds off
byte timerseq[at];
byte tc = 10; //timing moment of timers
byte knipper; //tijd van knipperen
byte bedrijf = 0; //fase van in bedrijf 0=open 1=gesloten sensor 1 begin sensor 2 eind 2=gesloten sensor 1 eind sensor 2 begin


volatile byte sf = 0; //servo focus welke servo wordt aangestuurd, alleen in loop gebruiken
byte sv; //focus welke servo in program mode

byte pfase = 0; //pf=program fase
byte plevel = 0; //program level
//timers, counters
volatile byte pc = 0;
byte switchcount; //teller voor ingedrukt houden knop 3
unsigned long oldmillis;
byte slowcount;
int count;
byte countflsh = 0; //count aantal flashes in programmeer mode
byte flashpauze = 0; //tijd van led off in flash
byte countlp = 0; //count long press van knop 2

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

}
void MEM_read() {
	//reads EEPROM variables after power up
	speedservo = EEPROM.read(10);
	if (speedservo > 30)speedservo = 20; //20=default value

	//uit EEprom halen standen servo
	servo[0].open = EEPROM.read(11);
	if (servo[0].open < servomin || servo[0].open > servomax)servo[0].open = 180;
	servo[0].close = EEPROM.read(12);
	if (servo[0].close < servomin || servo[0].close > servomax)servo[0].close = 80;
	servo[1].open = EEPROM.read(13);
	if (servo[1].open < servomin || servo[1].open > servomax)servo[1].open = 180;
	servo[1].close = EEPROM.read(14);
	if (servo[1].close < servomin || servo[1].close > servomax)servo[1].close = 80;

	servo[0].rq = servo[0].open; //beginstanden servo1
	servo[1].rq = servo[1].open;
	servo[0].pos = servo[0].open;
	servo[1].pos = servo[0].open;

	//knipperfrequentie
	switch (EEPROM.read(20)) {
	case 0:
		knipper = Ahob;
		break;
	case 1:
		break;
	default:
		knipper = Ahob;
		break;
	}




	sb = 0;
}
ISR(TIMER1_COMPA_vect) {
	//compa A timer 1 =  50 x 0.0625 sec
	if (pc > servo[sf].pos) { //puls duur bereikt
		sb &= ~(1 << (6 + sf)); //reset servo pin sf = 0 of 1)	
		TCCR1 = 0; //stop timer
		Shift();
	}
	TCNT1 = 0;
	pc++;
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
		if (countlp > 50 && ~GPIOR1 & (1 << 2) && pfase > 0) {
			GPIOR1 |= (1 << 2);
			longpress(); //longpress 


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
	sequence(6); //flash

	plevel++;
	switch (pfase) {
	case 2: //instellen aantal levels in deze pfase
		if (plevel > 3)plevel = 0;
		break;
	}
	timer[2] = 0; //reset de te gebruiken timer
	switch (plevel) {
	case 1: //servo instellen snelheid 
		//dubbel knipper op contra led
		sequence(50);
		break;
	case 2: //servo instellen positie open
		//triple knipper		
		servo[sv].rq = servo[sv].open;
		break;
	case 3://servo instellen positie close
		servo[sv].rq = servo[sv].close;
		break;
	}



}
void sequence(byte seq) { //od=open dicht open false dicht true
	//temp
	byte b = 0;
	//cyclus, sequence

	switch (seq) {
	case 0: // free
		break;
		//*******afsluiting programmeer mode
	case 2: //program mode back to 0 
		settimer(0, 10, B111000, 3); //10ms wachten
		break;
	case 3:
		settimer(0, 20, B000111, 4); //400ms leds uit
		break;
	case 4:
		settimer(0, 50, B111000, 0); //1sec leds aan, daarna in bedrijf
		break;
		//***********flash tussen twee plevels in 
	case 6:
		settimer(0, 1, B111, 7);
		break;
	case 7:
		settimer(0, 50, B111000, 0);
		break;
		//*********************************************


	case 10:

		//sluit overweg... 
		GPIOR1 |= (1 << 3); //flag overweg gesloten (direct)

		//start knipperlicht 
		sb |= (3 << 0); //constante led aan, ie knipper led

		settimer(3, knipper, B010100, 12);
		//stop blokkeren voor een periode 
		GPIOR1 |= (1 << 4);
		settimer(2, 200, 0, 14);

		//sluit bomen
		for (byte i = 0; i < 2; i++) {
			//servo[i].reg &= ~(1 << 1);
			servo[i].reg |= (1 << 0);
			servo[i].timer = (random(starttijdbomen, starttijdbomen + 20));
			servo[i].timercount = 0;
		}
		break;

	case 12:
		settimer(3, knipper, B100010, 13);
		break;
	case 13:
		settimer(3, knipper, B010100, 12);

		break;
	case 14: //blokkeerd de stop timer tijdens de start van de bomen beweging
		GPIOR1 &= ~(1 << 4); //blokkade zie whenclose()
		break;
	case 15: //overweg open stop knipper effect
		timer[3] = 0;
		sb &= ~(7 << 0); //all leds off
		break;

	case 20: //open overweg 
		//GPIOR1 &= ~(1 << 3); //flag overweg open (direct) 
		//onderstaand straks na bomen zijn gesloten en een random wacht tijd, temp op loslaten knop
		//sb &= ~(1 << 0); //constante led uit

		//open bomen
		for (byte i = 0; i < 2; i++) {
			servo[i].reg &= ~(1 << 0);
			servo[i].reg |= (1 << 1);
			servo[i].timer = (random(3, 30));
			servo[i].timercount = 0;
		}
		//knipperen wordt uitgeschakeld in SV_control
		bedrijf = 0; //sensoren werken weer in openstand
		break;

		//******flash in program
	case 30: //Test servo 1 pfase=2 knipper led links
		//led off na flash
		switch (sv) { //keuze led combinatie die gaat flashen
		case 0:
			b = B100000;
			break;
		case 1:
			b = B010000;
			break;
		case 2:
			break;
		}
		settimer(1, 1, b, 31);
		countflsh++;
		if (countflsh > plevel) {
			flashpauze = 60;
			countflsh = 0;
		}
		else {
			flashpauze = 10;
		}
		break;

	case 31:
		switch (sv) { //keuze led combinatie die gaat flashen
		case 0:
			b = B100;
			break;
		case 1:
			b = B010;
			break;
		case 2:
			break;
		}
		settimer(1, flashpauze, b, 30);
		break;
		//********end flash

//*********voor sensoren open en dicht van de overweg
	case 40: //timer afgelopen, max tijd, van sluiten overweg
		settimer(4, 5, 0, 20); //open overweg na 5x20ms
		break;
	case 41: //sensor voor weer openen overweg geactiveerd
		//eerst check op sensor vrij is gegeven, detectie stilstaande trein

		if (bedrijf > 0 && bedrijf < 3) {
			if (status & (1 << 1) && status & (1<<0)) { //sensoren vrij
				settimer(4, 20, 0, 20); //open overweg na 20x20ms
			}
			else {
				settimer(5, 50, 0, 41); //set timer 2 seconde voor weer openen overweg (reset) beter 2sec?
			}
		}


		/*
		switch (bedrijf) {

		case 1:

			break;
		case 2:
			if (status & (1 << 0)) { //sensor vrij
				settimer(4, 5, 0, 20); //open overweg na 5x20ms
			}
			else {
				settimer(5, 50, 0, 41); //set timer 2 seconde voor weer openen overweg (reset) beter 2sec?
			}
			break;
		}
*/

		break;

		//***************************************************

						//*******servo heen en weer (speed instellen)
	case 50: //start positie program servo
		servo[sv].rq = servo[sv].open;// (servo[sv].open + servo[sv].close) / 2;
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

		if (servo[sv].pos == servo[sv].open) {
			servo[sv].rq = servo[sv].close;
		}
		else {
			servo[sv].rq = servo[sv].open;
		}
		settimer(2, 5, 0, 51); //wachtlus totdat servo op positie is
		break;
		//**********einde servo heen en weer
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
				servo[i].rq = servo[i].close;
				servo[i].reg &= ~(1 << 0); //stop timer open

			}
		}
		else if (servo[i].reg & (1 << 1)) { //timer aan?	{			
			servo[i].timercount++;
			if (servo[i].timercount > servo[i].timer) {
				servo[i].rq = servo[i].open;
				servo[i].reg &= ~(1 << 1); //open

			}
		}
	}
}
void SWon(byte sw) {
	//0=sensor1 1=sensor2 2=switch
	//sb |= (1 << sw); //temp led aan
	switch (pfase) {
	case 0: //in bedrijf
		switch (sw) {
		case 0: //sensor 1
			openclose(sw);
			break;
		case 1: //sensor 2
			openclose(sw);
			break;

		case 2:
			sequence(10);//sluit overweg
			bedrijf = 10; //sensoren uitgeschakeld
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

	case 2: //pfase=2 servo  test en instellen

		switch (sw) {
		case 2:
			switch (plevel) {
			case 0:
				if (sv == 0) {
					GPIOR1 ^= (1 << 6);
					if (GPIOR1 & (1 << 6)) {
						servo[0].rq = servo[0].close;
					}
					else {
						servo[0].rq = servo[0].open;
					}
				}
				else {
					GPIOR1 ^= (1 << 7);
					if (GPIOR1 & (1 << 7)) {
						servo[1].rq = servo[1].close;
					}
					else {
						servo[1].rq = servo[1].open;
					}
				}
				break; //instellen positie open

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
			switch (plevel) {
			case 0:
				if (sv == 0) {
					sv = 1;
				}
				else {
					pfase++;
					sv = 0;
				}
				break;
			case 2: //instellen servo open positie
				if (servo[sv].open > servomin) servo[sv].open--;
				servo[sv].rq = servo[sv].open;
				break;
			case 3: //instellen servo close positie
				if (servo[sv].close > servomin) servo[sv].close--;
				servo[sv].rq = servo[sv].close;
				break;
			} //end plevel
			break;

		default:
			plevel = 0;
			pfase++;
			if (pfase > apf)pfase = 0; //apf aantal program fases zie #define
			break;
		} //end pfase

		Programs();
	}
}
void SWoff(byte sw) {
	if (sw == 2) {
		countlp = 0; //reset longpress timer
		GPIOR1 &= ~(1 << 2);
	}


	//sb &= ~(1 << sw); //temp led off

	switch (pfase) {
	case 0:
		switch (sw) {
		case 0:
			break; //jo

		case 1:
			break;
		case 2:
			sequence(20);//overweg openen
			break;
		}
		break;
	case 1:
		break;
	case 2: //servo  test en program
		switch (sw) {
		case 2: //knop S
			switch (plevel) {
			case 1: //speed servo
				speedservo--;
				if (speedservo > 30)speedservo = 30;
				break;
			case 2:
				if (servo[sv].open < servomax) servo[sv].open++;
				servo[sv].rq = servo[sv].open;
				break;
			case 3: //instellen positie close
				if (servo[sv].close < servomax) servo[sv].close++;
				servo[sv].rq = servo[sv].close;
				break;
			}

			break;
		}
		break;
	case 3:
		break;
	}
}
void openclose(byte sw) {
	//controleerd open en sluiten overweg met sensor inputs 0=sensor1 1=sensor2 pressed!
	//Timers 4 en 5 exclusief voor deze functie
	switch (bedrijf) {
	case 0: //overweg open
		bedrijf = sw + 1;
		settimer(4, 3000, 0, 40); //sluiten na 2000*20ms =1 minuut
		sequence(10); //sluit overweg
		break;
	case 1: //overweg gesloten 1>2
		switch (sw) {
		case 0: //sensor 1
			settimer(4, 3000, 0, 40); //reset timer 
			timer[5] = 0; //reset eventuele timer 5 (trein uit andere richting)
			break;
		case 1: //sensor 2
			settimer(5, 50, 0, 41); //set timer 1 seconde voor weer openen overweg
			timer[4] = 0;
			break;
		}
		break;
	case 2: //overweg gesloten 2>1
		switch (sw) {
		case 0: //sensor 1
			settimer(5, 50, 0, 41); //set timer 1 seconde voor weer openen overweg (reset)
			timer[4] = 0;
			break;
		case 1: //sensor 2
			settimer(4, 3000, 0, 40); //reset timer 1 minuut
			timer[5] = 0;
			break;
		}

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
		// Aanpassingen opslaan in EEPROM
		EEPROM.update(10, speedservo);
		EEPROM.update(11, servo[0].open);
		EEPROM.update(12, servo[0].close);
		EEPROM.update(13, servo[1].open);
		EEPROM.update(14, servo[1].close);
		//leds uit_aan_uit
		sequence(2);
		break;
	case 1: //alle leds on   laatst?
		sb |= (7 << 0);
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

	if (servo[sf].pos != servo[sf].rq) { //s1rq) 		
		if (servo[sf].pos > servo[sf].rq) { //  s1rq) {
			servo[sf].pos--;
		}
		else {
			servo[sf].pos++;
		}
		pc = 0;
		sb |= (1 << 6 + sf); //set servo pin	
		Shift();

		TCNT1 = 0;
		TCCR1 = 129; // start timer no prescaler
	}

	else if (whenclose()) {//(GPIOR1 & (1 << 3)) {
		//alleen als beide servoos hun eindbestemming hebben bereikt		

		//if (servo[0].rq == servo[0].open && servo[1].rq == servo[1].open) {
			//boom bereikt stopplek en overweg is open, dus na timer knipperen stoppen
		GPIOR1 &= ~(1 << 3); //flag (open/dicht)
		//kan meerdere keren voor komen, timer wordt dan weer gereset
		settimer(2, 50, 0, 15);
		//}
	}
}
boolean whenclose() {
	bool yes = false;
	if (~GPIOR1 & (1 << 4)) { //blokkade stop proces tijdens starten van de servo
		if (GPIOR1 & (1 << 3)) { //open dicht flag
			if ((servo[0].rq == servo[0].open && servo[1].rq == servo[1].open)) {
				yes = true;
			}
		}
	}
	return yes;
}
void slow() { //1 ms teller
	slowcount++;
	countservo++;
	if (countservo > 3 + speedservo) { //servo frequentie regelbaar met speedservo
		countservo = 0;
		GPIOR1 ^= (1 << 0);
		sf = 0;
		if (GPIOR1 & (1 << 0)) sf = 1;
		SV_control();
	}

	if (slowcount == 10) {
		timers();
	}


	//switch & sensors scannen
	if (slowcount > 20) {
		slowcount = 0;
		read(); //lees switch 
		count++;
		if (count > 2)count = 0;
		sb &= ~(56 << 0); //clear bit 3-4-5
		sb |= (1 << count + 3); //opvolgend 3 4 5 om sensors en switch te scannen
		Shift();
	}
}
void loop() {
	//millis counter
	if (millis() != oldmillis) {
		oldmillis = millis();
		slow();
	}
}
