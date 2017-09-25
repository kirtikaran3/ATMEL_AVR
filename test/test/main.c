/*
 * test.c
 *
 * Created: 4/15/2016 7:06:52 AM
 * Author : kirti
 */ 

#include <avr/io.h> 
#include <util/delay.h>


int main(void)
{ 
	DDRD &=~(1<<PIND0); 
	DDRD &=~(1<<PIND2); 
	
	DDRB=0xFF; 
	int i;
    /* Replace with your application code */
    while (1) 
    { 
		if(((bit_is_clear(PIND,1)) && (bit_is_clear(PIND,2)))){
		
			PORTB=0xFF; 
			_delay_ms(1000); 
			PORTB=0x00; 
			_delay_ms(1000);
		
		}		 
		else if(((bit_is_set(PIND,1)) && (bit_is_clear(PIND,2)))){
			
			PORTB=0xAA; 
			_delay_ms(1000); 
			PORTB=0x55; 
			_delay_ms(1000); 
		}
		else if(((bit_is_clear(PIND,1)) && (bit_is_set(PIND,2)))){
		
			for(i=0;i<10;i++){}			
			
			PORTB=i; 
			
			}
		 
		else if(((bit_is_set(PIND,1)) && (bit_is_set(PIND,2)))){
		PORTB=0xFF; 
		_delay_ms(100); 
		PORTB=0x00; 
		_delay_ms(100);
		
		}
		}
}






