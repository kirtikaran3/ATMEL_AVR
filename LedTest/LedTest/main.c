/*
 * LedTest.c
 *
 * Created: 4/15/2016 10:34:33 AM
 * Author : kirti
 */ 

#include <avr/io.h>
#include <util/delay.h> 


int main(void)
{ 
	DDRD = 0xFF;
    /* Replace with your application code */
    while (1) 
    { 
		PORTD=0xFF; 
		_delay_ms(1000); 
		PORTD=0x00; 
		_delay_ms(1000);
    }
}

