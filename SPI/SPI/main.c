/*
 * SPI.c
 *
 * Created: 4/27/2016 3:34:32 AM
 * Author : kirti
 */ 

#include <avr/io.h>

#define MOSI 5 //check for MOSI pin
#define SCK 7  //check for SCK pin
#define ss 4
int main(void)
{ 
	DDRD=0xFF; 
	DDRB=(1<<MOSI)|(1<<SCK);
	SPCR=(1<<SPE)|(1<<SPR0)|(1<<MSTR); 
	
    /* Replace with your application code */
    while (1) 
    { 
		SPDR='G'; 
			while(!(SPSR)&&(1<<SPIF)){
			PORTD=SPDR;
			} 
			return 0;
			
    }
}

