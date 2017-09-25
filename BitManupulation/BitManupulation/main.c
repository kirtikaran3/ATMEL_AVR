/*
 * BitManupulation.c
 *
 * Created: 4/15/2016 6:24:48 AM
 * Author : kirti
 */ 

#include <avr/io.h> 
#include <util/delay.h> 



int main(void)
{ 
	DDRD &= ~(1<<PIND2); 
	DDRD &= ~(1<<PIND1);
	DDRB = 0xFF;
    /* Replace with your application code */
    while (1) 
    { 
		if((bit_is_clear(PIND,1))&&((bit_is_clear(PIND,2)))){
	
			PORTB=0xFF; 
			_delay_ms(1000); 
			PORTB=0x00; 
			_delay_ms(1000);
	
		} 
		if((bit_is_set(PIND,1))&&((bit_is_clear(PIND,2)))){
			
			PORTB=0xaa;
			_delay_ms(1000);
			PORTB=0x00;
			_delay_ms(1000); 
			PORTB=0x55;
			_delay_ms(1000);
			PORTB=0x00;
			_delay_ms(1000);
			
		}  
		if((bit_is_clear(PIND,1))&&((bit_is_set(PIND,2)))){
			
			PORTB=0xFF;
			_delay_ms(100);
			PORTB=0x00;
			_delay_ms(100);
			
		}
		
    }
}

