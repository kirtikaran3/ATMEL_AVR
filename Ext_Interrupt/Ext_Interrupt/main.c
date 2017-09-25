/*
 * Ext_Interrupt.c
 *
 * Created: 4/21/2016 10:18:37 AM
 * Author : kirti
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>


int main(void)
{
	DDRD &= ~(1 << DDD2);     // Clear the PD2 pin
	// PD2 (PCINT0 pin) is now an input

	PORTD |= (1 << PORTD2);    // turn On the Pull-up
	// PD2 is now an input with pull-up enabled



	EICRA |= (1 << ISC00);    // set INT0 to trigger on ANY logic change
	EIMSK |= (1 << INT0);     // Turns on INT0

	sei();                    // turn on interrupts

	while(1)
	{
		/*main program loop here */
	}
}



ISR (INT0_vect)
{
	/* interrupt code here */
}

