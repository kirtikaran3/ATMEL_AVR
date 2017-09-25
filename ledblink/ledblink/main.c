/*
 * ledblink.c
 *
 * Created: 4/15/2016 5:55:20 AM
 * Author : kirti
 */ 

#include <avr/io.h>
#include <util/delay.h> 

#define F_CPU 1600000
int main(void) 
{ 
	DDRD=0xff;
    /* Replace with your application code */
    while (1) 
    { 
		PORTD=0xFF; 
		_delay_ms(1000); 
		PORTD=0x00; 
		_delay_ms(1000);
    }
}

