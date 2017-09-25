/*
 * waves.c
 *
 * Created: 2/16/2016 4:06:03 PM
 * Author : kirti
 */ 

#include <avr/io.h>
#include <util/delay.h>

unsigned int sinev[20]={0,17,34,65,95,125,128,192,224,238,256,238,224,192,125,95,65,34,17,0};


void sqrwave(){
	while(PINA==0x0f){
		
		PORTC=0xFF; 
		_delay_us(500); 
		PORTC=0x00; 
		_delay_us(500);
	}
} 

void sine(){
	while(PINA==0xF0){
	int i; 
	for(i=0;i<20;i++){
		
		PORTC=sinev[i]; 
		_delay_us(500);
	}
 }
} 
void tri(){
	
	while(1){
		 
		for (int i=0;i<255;i++)
		{ 
			PORTC=i; 
			_delay_us(500);
		} 
		for (int i=255;i>0;i++)
		{ 
			PORTC=i; 
			_delay_us(500);
		}
		
	}	
	
}



/* Replace with your library code */
int myfunc(void)
{
	
	DDRC=0xFF;
	PORTC=0x00; 
	DDRA=0x00;
		
	while(PINA==0xF0){
		
	loop:switch(PINA){
		
		case 0xF1: 
			_delay_ms(200); 
			sine(); 
			break; 
		case 0xF4: 
			_delay_ms(200); 
			sqrwave(); 
			break; 
		case 0xF8: 
			_delay_ms(200); 
			tri(); 
			break; 
			
		}	
	goto loop;
	}
	
	return 0;
}






