/*
 * LCD8BIT.c
 *
 * Created: 3/21/2016 11:28:32 AM
 * Author : kirti
 */  
#define F_CPU 1000000
#include <avr/io.h>
void delay(unsigned char); 

//Main Code 
int main()
{
DDRB=0xff;    //set PORTB as out put
  
  DDRD=0b01110000;   //Set PD.4,5 and 6 as Output
 
/*
RS=PD5
           R/W=PD6
         
           E=PD4          */

  //Give Inital Delay for LCD to startup as LCD is a slower Device
  delay(2);

  init_lcd();

while(1)
    {

        
            
   lcd_cmd(0x80);       //Goto Line-1,first position 
  lcd_send_string("WELCOME TO! ");

   lcd_cmd(0xC0);      //Goto Line-2, first position     

   lcd_send_string("Embedded class at DKOP :) ");
   lcd_cmd(0x01);     //Clear the lcd
   delay(1);
      
}
} 

//LCD function
/*------------------------------------------------------------------------------------------------------------*/

//Function for sending commands to LCD
void lcd_cmd(unsigned char command)

{
 

//Put command on the Data Bus
  PORTB = command;

  //Enable LCD for command writing
  PORTD = 0b00010000;

  //Allow delay for LCD to read the databus
  delay(1);

  //Disable LCD again
  PORTD = 0b00000000;

  //Allow some more delay
  delay(1);
}

//Function for sending Data to LCD
void lcd_data(unsigned char data)
{
  //Put data on Data Bus
  PORTB= data;


  //Set R/S (Regiter Select) to High, and Enable to High
  PORTD = 0b00110000;

  //Allow for delay
  delay(1);

  //Disable LCD again
  PORTD = 0b00100000;

  //Allow for some more delay
  delay(1);
}

//Function to send String to LCD
void lcd_send_string(char* string)
{
  while(*string)
{
  //Send value of pointer as data to LCD
  lcd_data(*string);
  //Increment string pointer
  string++;
  }
}




//Function to Initilise LCD
void init_lcd()
{
  //Setup both lines of LCD
  lcd_cmd(0x38);
  //Set Cursor off - Enable LCD
  lcd_cmd(0x0E);
  //Clear Screen
  lcd_cmd(0x01);
  //Goto first position
  lcd_cmd(0x80);
}
 /*----------------------------------------------------------------------------------------------------------*/

//Delay function

void delay(unsigned char dtime)
{
int i,j;
for(i=0;i<=dtime;i++)
{
for(j=0;j<5000;j++);
}}