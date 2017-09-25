/*
 * timer1.c
 *
 * Created: 4/19/2016 6:45:29 AM
 * Author : kirti
 */ 

#include <avr/io.h>

void timer_delay();
int main(void)
{ 
	DDRB=0x0f;
    /* Replace with your application code */
    while (1) 
    { 
		PORTB=0x0f; 
		timer_delay(); 
		PORTB=0x00; 
		timer_delay();
    }
} 
void timer_delay(){
	
	TCNT1H=0xff; 
	TCNT1L=0xff; 
	
	TCCR1A=0x00; 
	TCCR1B=0x01; 
	
	while((TIFR1 & 0x01)==0); 
	TCNT1H=0x00; 
	TCNT1L=0x00; 
	TIFR1=0x01;
	
}

