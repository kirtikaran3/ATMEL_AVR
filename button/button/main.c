/*
 * button.c
 *
 * Created: 3/15/2016 11:29:45 AM
 * Author : kirti
 */ 

#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
	DDRB |= 1 << PINB0;
	DDRB &= ~(1 << PINB1);
	//PORTB |= 1 << PINB1;
    DDRD=0xff;
	while (1)
	{
		//PORTB ^= 1 << PINB0;
		if (bit_is_clear(PINB, 1))
		{ 
			PORTD=0xff;
			_delay_ms(1000); //Fast 
			PORTD=0x00; 
			_delay_ms(1000);
		}
		else
		{ 
			PORTD=0xAA;
			_delay_ms(1000); //Slow, from previous 
			PORTD=0x55; 
			_delay_ms(1000);
		}
	}
}

