/*
 * Timer_interrupt.c
 *
 * Created: 4/21/2016 10:50:19 AM
 * Author : kirti
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>


int main(void)
{

	// Set the Timer Mode to CTC
	TCCR0A |= (1 << WGM01);

	// Set the value that you want to count to
	OCR0A = 0xF9;

	TIMSK0 |= (1 << OCIE0A);    //Set the ISR COMPA vect

	sei();         //enable interrupts


	TCCR0B |= (1 << CS02);
	// set prescaler to 256 and start the timer


	while (1)
	{
		//main loop
	}
}


ISR (TIMER0_COMPA_vect)  // timer0 overflow interrupt
{
	//event to be executed every 4ms here
}

