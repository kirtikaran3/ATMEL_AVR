/*
 * usart_rxc.c
 *
 * Created: 4/23/2016 2:40:48 AM
 * Author : kirti
 */

#include <avr/io.h>


int main(void)
{
	DDRB=0xff;
	UCSR0B=(1<<RXEN0);
	UCSR0C=(1<<UCSZ01)|(1<<UCSZ00)|(1<<UMSEL00);

    /* Replace with your application code */
    while (1)
    {
		while(!(UCSR0A &(1<<RXC0)));
		PORTB=UDR0;
    }
	return 0;
}
