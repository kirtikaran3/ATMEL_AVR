/*
 * usart_string.c
 *
 * Created: 4/23/2016 2:37:20 AM
 * Author : kirti
 */ 

#include <avr/io.h>
void usart_init(void){
	UCSR0B=(1<<TXEN0);
	UCSR0C=(1<<UCSZ01)|(1<<UCSZ00);
	UBRR0=0x33; 
	
}
void usart_send(char ch){
	while(!(UCSR0A&(1<<UDRE0))&(1<<UMSEL00));
	UDR0=ch;

	
}


int main(void)
{ 
	char str[30]="this is dkop labs";
	char length=30; 
	char i=0; 
	usart_init();
    /* Replace with your application code */
    while (1) 
    { 
		usart_send(str[i++]); 
		if(i>length){
		i=0;
		} 
		return 0;
		
		
    }
}

