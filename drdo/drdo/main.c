/*
 * drdo.c
 *
 * Created: 2/19/2016 1:03:02 PM
 * Author : kirti
 */ 

#include <avr/io.h>
#include <util/delay.h>

int main(void)
{ 
	int sinewave(); 
	int sqrwave(); 
	int triwave(); 
	int sawtooth();
	
	DDRC=0xFF; 
	DDRD=0x00;
	
	unsigned int sine[19]={0,25,45,75,90,125,145,190,225,255,225,190,145,125,90,75,45,25,0};
    /* Replace with your application code */
    while (1) 
    { //specify the count value first to switch between waves below is just a representation
		if((bit_is_set(PIND0))&&(bit_is_set(PIND1))){
		
			sinewave(); 
			_delay_us(100); 
		}
		
		if((bit_is_set(PIND0))&&(bit_is_set(PIND1))){
		
			triwave(); 
			_delay_us(100); 
		}
		
		if ((bit_is_set(PIND0))&&(bit_is_set(PIND1))){ 
			
			sqrwave(); 
			_delay_us(100);
		} 
		
		if ((bit_is_set(PIND0))&&(bit_is_set(PIND1))){ 
			
			sawtooth(); 
			_delay_us(100);
		}
		 
	
    }
}

int sine(){
	
	
	
} 

int sqrwave(){
	
	
	
}

int triwave(){
	
	
	
}

int sawtooth(){
	
	
}





