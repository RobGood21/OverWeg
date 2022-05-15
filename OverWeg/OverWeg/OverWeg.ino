/*
 Name:		OverWeg.ino
 Created:	5/15/2022 9:42:04 AM
 Author:	Rob Antonisse

 Sketch voor de techniek van een modelspoor overweg


*/



//byte sb; //shiftbyte GPIOR0 (gaat ff sneller)
 # define sb GPIOR0 //gebruik ingebakken register
//bit0 led0; bit1 led1; bit2 led2; bit3 sensL; bit4 sensR ;bit5 switch;bit6 servo1;bit7 servo2

//temp
unsigned long tijd;


void setup() {

	//ports
	DDRB |= (1 << 0); //PB0 pin5 as output SERial
	DDRB |= (1 << 1); //PB1 pin 6 as output RCLK (latch)
	DDRB |= (1 << 2); //PB2 pin7 as output SRCLK (shiftclock)


	//temp
	DDRB |= (1 << 3); //pin2 as output
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

void loop() {
  
	if (millis() - tijd > 500) {
		tijd = millis();
		//temp
		sb ^= (1 << 1); //knipper op pin 1 van shiftregister
		Shift();
	}

}
