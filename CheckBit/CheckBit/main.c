/*
 * CheckBit.c
 *
 * Created: 4/15/2016 6:53:16 AM
 * Author : kirti
 */ 

#include <avr/io.h>


int main(void)
{ 
	PORTB=0b01010101;
    /* Replace with your application code */
    while (1) 
    { 
		PORTB |=(0x01<<2); 
		PORTB &=~(0x01<<1);
    }
}

