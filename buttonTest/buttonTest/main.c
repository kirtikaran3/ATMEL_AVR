/*
 * buttonTest.c
 *
 * Created: 4/15/2016 12:31:04 PM
 * Author : kirti
 */ 

#include <avr/io.h>
#include <util/delay.h>

int main(void)
{ 
	DDRB &=~(1<<PIND0); 
	DDRB &=~(1<<PIND1);
	
	DDRD =0xFF; 
    /* Replace with your application code */
    while (1) 
    { 
		if(bit_is_clear(PINB,0)){
			PORTD=0XAA; 
			_delay_ms(1000); 
			PORTD=0x55; 
			_delay_ms(1000);
			
		} 
		if(bit_is_clear(PINB,1)){
			
			PORTD=0XFF;
			_delay_ms(1000);
			PORTD=0x00;
			_delay_ms(1000);
			
		}
		
    }
}

