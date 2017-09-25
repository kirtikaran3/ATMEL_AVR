/*
 * usart_char.c
 *
 * Created: 4/23/2016 2:28:13 AM
 * Author : kirti
 */

#include <avr/io.h>
void usart_init(void){
	UCSR0B=(1<<TXEN0);
	UCSR0C=(1<<UCSZ01)|(1<<UCSZ00);
	UBRR0=0x33;
}
void usart_send(void){
	while(!(UCSR0A&(1<<UDRE0)));
	UDR0=ch;


}

int main(void)
{
	usart_init();
    /* Replace with your application code */
    while (1)
    {
		usart_send('K');
		return 0;
    }
}
