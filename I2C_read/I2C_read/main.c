/*
 * I2C_read.c
 *
 * Created: 4/27/2016 4:24:52 AM
 * Author : kirti
 */ 

#include <avr/io.h> 
#include <util/delay.h>
void i2c_init(void){
	
	TWSR=0x00;  //prescale to 0 
	TWBR=0x47;  //50khz 
	TWCR=0x04;  //twen 
} 

void i2c_start(void){
	
	TWCR=(1<<TWINT)|(1<<TWSTA)|(1<<TWEN); 
	while(((TWCR)&&(1<<TWINT))==0);
	
} 
void i2c_stop(void){
	
	TWCR=(1<<TWEN)|(1<<TWSTO)|(1<<TWINT);
	
} 
void i2c_write(char data){
	
	TWDR=data;
	TWCR=(1<<TWINT)|(1<<TWEN);
	while(((TWCR)&&(1<<TWINT))==0);
}

int main(void)
{
    /* Replace with your application code */ 
	i2c_init(); 
	i2c_start(); 
	i2c_write('H'); 
	_delay_ms(10); 
	i2c_write('I'); 
	i2c_stop();
    while (1) 
    {
    } 
	return 0;
}

