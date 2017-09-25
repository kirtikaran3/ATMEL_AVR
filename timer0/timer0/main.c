/*
 * timer0.c
 *
 * Created: 4/19/2016 6:36:24 AM
 * Author : kirti
 */ 

#include <avr/io.h>

void timer_delay();
int main(void)
{ 
	DDRB=0xff; 
	
    /* Replace with your application code */
    while (1) 
    { 
		PORTB=0xff; 
		timer_delay(); 
		PORTB=0x00; 
		timer_delay();
    }
} 
void timer_delay(){
	
	TCNT0=0xff; 
	
	TCCR0A = 0x02; 
	
	while((TIFR0)&(0x01)==0); 
	TCCR0A=0x00; 
	TIFR0=0x01;
}

