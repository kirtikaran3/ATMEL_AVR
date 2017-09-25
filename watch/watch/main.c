/*
 * watch.c
 *
 * Created: 4/15/2016 6:41:36 PM
 * Author : kirti
 */ 


#include "sam.h"
#include <arm_math.h> 
#include <alloca.h>
#include <arm_common_tables.h> 
#include <arm_neon.h> 
#include <assert.h>

int main(void)
{
    /* Initialize the SAM system */
    SystemInit();

    /* Replace with your application code */
    while (1) 
    { 



//#pragma config FSOSCEN = ON, OSCIOFNC = ON



#define F_CPU         40000000
#define PB_CLK          SYS_CLK

#define SS              LATAbits.LATA4      // physical pin 12
#define dirSS           TRISAbits.TRISA4

// useful VT100 escape sequences for PuTTY/GTKterm
#define clearPutty()    printf( "\x1b[2J")
#define homePutty()     printf( "\x1b[H")

#define AC_ZERO         2048    // since Dac resolution is 12-bits

#define BAUDRATE        115200  // for serial comm 9600, 19200, 38400, 57600, 115200

#define stackSize       128     // cyclic buffer/stack

#define allGood             0
#define incorrectHeaderSize 1
#define riff                2
#define wav                 3
#define fmt                 4
#define notPCM              5

#define textHeight (textsize * 8)
#define textWidth (textsize * 6)

UINT32 spicon_fs, spicon_tft;
UINT32 spicon2_fs, spicon2_tft;
static char debugbmp = 0;
static struct pt pt_touch, pt_timer, pt_uart, pt_input, pt_output, pt_DMA_output, pt_buttonPressed, pt_settings, pt_setTime, pt_setBalls;
static struct pt_sem sem;

#define GREEN 0x05E0//0x13E7
#define ILI9340_TEAL 0x03EF
rtccTime tm, tm1; // time structure
rtccDate dt, dt1; // date structure

UINT8 receiveBuffer[100];
char txtBuffer[100];
char buffer[60];


volatile UINT16 LSTACK[stackSize];
volatile UINT16 RSTACK[stackSize];
volatile UINT32 BOS;
volatile UINT32 TOS;

volatile UINT32 msCounter = 0;
volatile UINT32 randomvar = 0;
volatile UINT32 CSlength = 0;
volatile UINT32 j = 0;
volatile UINT32 TIC = 0, TOC = 0;

volatile UINT32 bufferCounter = 0;
const int UINT_32 = 0;
volatile UINT32 intCounter = UINT_32;

void setupUART(void);
void infiniteBlink(void);
void setupSystemClock(void);
void getFilename(char * buffer);
void configureHardware(UINT32 sampleRate);

// command array
static char cmd[30];
static int value;

static int savePWM; //PWM for screen brightness
static unsigned char onTime; //time screen has been on
static unsigned char stateTransition; //Keeps track of time since last state transition

static int xTouch; //x position of touch on resistive screen
static int yTouch; //y position of touch on resistive screen
static int lastxTouch; //debounce
static int lastyTouch; //debounce
static int color = ILI9340_WHITE;
static char theme = 1;
static float bufferTime;//saves time when RTCC is turned off
static float bufferDate;//saves date when RTCC is turned off

//which images are displayed is dependent on what theme is chosen
#define background theme == 0? "space.bmp":theme == 1?"vault-tec.bmp":"bruce_background.bmp"
#define settingsImg theme == 0? "gears.bmp":theme == 1? "vault-boy-settings.bmp": "Bruce_1.bmp"
#define gameImg theme == 0? "Controller.bmp": theme == 1? "vault-boy-game.bmp": "Bruce_2.bmp"
#define paintImg theme == 0? "paintbrush.bmp": theme == 1? "vault-boy-paint.bmp": "Bruce_3.bmp"


static char state; //determines which screen is shown
static char on; //screen is on?
// UART parameters
//#define BAUDRATE 9600 // must match PC end
#define PB_DIVISOR (1 << OSCCONbits.PBDIV) // read the peripheral bus divider, FPBDIV
//#define PB_FREQ SYS_FREQ/PB_DIVISOR // periperhal bus frequency

// useful ASCII/VT100 macros for PuTTY
#define clrscr() printf( "\x1b[2J")
#define home()   printf( "\x1b[H")
#define pcr()    printf( '\r')
#define crlf     putchar(0x0a); putchar(0x0d);
//#define max_chars 50 // for input buffer

static struct pt pt_anim, pt_fRate, pt_end;
static struct pt_sem sem;

// === the fixed point macros ========================================
typedef signed int fix16;
#define multfix16(a,b) ((fix16)(((( signed long long)(a))*(( signed long long)(b)))>>16)) //multiply two fixed 16:16
#define float2fix16(a) ((fix16)((a)*65536.0)) // 2^16
#define fix2float16(a) ((float)(a)/65536.0)
#define fix2int16(a)    ((int)((a)>>16))
#define int2fix16(a)    ((fix16)((a)<<16))
#define divfix16(a,b) ((fix16)((((signed long long)(a)<<16)/(b))))
#define sqrtfix16(a) (float2fix16(sqrt(fix2float16(a))))
#define absfix16(a) abs(a)
#define EnablePullDownB(bits) CNPUBCLR=bits; CNPDBSET=bits;

static PT_THREAD(protothread_rate(struct pt *pt)) {
    PT_BEGIN(pt);
    while (1) {
        PT_SEM_SIGNAL(&pt_anim, &sem);
        PT_YIELD_TIME_msec(10);

    }
    PT_END(pt);
}

static int ended = 0;//ended game flag, supresses interrupt animations
static int time;//time remaining (in seconds)
static int gameTime = 30;//Length of game in seconds
static int msTimer = 0;//keeps track of miliseconds, updates every timer interrupt
static int numBalls;//max number of balls in play
static fix16 x[50];//x coordinates of balls
static fix16 y[50];//y coordinates of balls
static fix16 vx[50];//x velocities of balls
static fix16 vy[50];//y velocities of balls
static int inPlay[50];//flag for whether a ball is in play
static int startTime[50];//keeps track of time at which a ball has spawned
static char hitCount[50];//
static int counter = 0;//keeps track of how many frames have elapsed since last ball was inserted
static int score = 0;
static int numFrames = 0;//number of frame updates since fps was last calculated
static int bumperWidth = 50;//width of user-controlled bumper
static int highScore;//set at 0 by default, updated after every game

static PT_THREAD(protothread_uart(struct pt *pt)) {
    // this thread interacts with the PC keyboard to take user input and set up PID parameters
    PT_BEGIN(pt);
    while (1) {
        // send the prompt via DMA to serial
        sprintf(PT_send_buffer, "%s%02x%s%02x%s%02x%s%02x%s%02x%s%02x%s", "Time: ", dt1.mon, "/", dt1.mday, "/", dt1.year, "  ", tm1.hour, ":", tm1.min, ":", tm1.sec, "\n\r");
//        sprintf(PT_send_buffer, , "cmd>");
        // by spawning a print thread
        PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));//send date and time

        //spawn a thread to handle terminal input
        // the input thread waits for input
        // -- BUT does NOT block other threads
        // string is returned in "PT_term_buffer"
        PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input));//wait for bluetooth input
        // returns when the thread dies
        // in this case, when <enter> is pushed
        // now parse the string
//        sscanf(PT_term_buffer, "%s %d", cmd, &value);
//
//        // echo
//        sprintf(PT_send_buffer, PT_term_buffer);
//        PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
//        sprintf(PT_send_buffer, "\n");
//        PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
//        sprintf(PT_send_buffer, "\r");
//        PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
        PT_YIELD_TIME_msec(30);
        /*
                     if (cmd[0]=='t' && cmd[1]=='1') { wait_t1 = value ;} // set the blink time MILLIsec
                     if (cmd[0]=='t' && cmd[1]=='4') { wait_t2 = value ;} // set the blink time MICROsec
                     if (cmd[0] == 'g' && cmd[1]=='1') {cntl_blink = 1 ;} // make it blink using YIELD
                     if (cmd[0] == 's' && cmd[1]=='1') {cntl_blink = 0 ;} // make it stop
                     if (cmd[0] == 'g' && cmd[1]=='4') {run_t4 = 1 ;} // make it blink using scheduler
                     if (cmd[0] == 's' && cmd[1]=='4') {run_t4 = 0 ;} // make it stop using scheduler
                     if (cmd[0] == 'k') {PT_EXIT(pt);} // kill this input thread (see also MAIN)
                     // scheduler control
                     if (cmd[0] == 'p' && cmd[1]=='1') { t1_rate = value ;}
                     if (cmd[0] == 'p' && cmd[1]=='3') { t3_rate = value ;}
                     if (cmd[0] == 'p' && cmd[1]=='4') { t4_rate = value ;}
                     if (cmd[0] == 'z') printf("%d\t%d\n\r", PT_GET_TIME(), sys_time_seconds);
         */
    } // while(1)
    PT_END(pt);
} // uart input thread

static char PushState;
static char buttonPressed;
//debounce protothread, poles button
static PT_THREAD(protothread_button(struct pt *pt)) {
    PT_BEGIN(pt);
    while (1) {
        PT_YIELD_TIME_msec(20);


        switch (PushState) {//debounce state machine
            case 0:
                // initial / no push
                if (mPORTBReadBits(BIT_9)) {
                    PushState = 1;
                }
                break;

            case 1:
                // maybe pushed
                if (mPORTBReadBits(BIT_9)) {
                    PushState = 2;
                    buttonPressed = 1;
                } else PushState = 0;
                break;

            case 2:
                // pushed
                // place key press into the buffer
                if (mPORTBReadBits(BIT_9)) {
                    PushState = 2;

                    //pressBuffer[numBuffer] = i;
                    //numBuffer ++;
                } else PushState = 3;

                break;

            case 3:
                // potentially released
                if (mPORTBReadBits(BIT_9)) {
                    PushState = 2;
                } else {
                    PushState = 0;
                    buttonPressed = 0;
                }
                break;
        } // end switch statement
    }
    PT_END(pt);
}


void SD_Config() {//resets config and status registers so that SD card transaction can occur
    SPI1CON = spicon_fs;
    SPI1CON2 = spicon2_fs;
    SPISTAT = 0;
}

inline UINT16 im8to16(UINT8 c) {
    // 8-bit format: R R R G G G B B
    // 16-bit format: 5R, 6G, 5B
    return ( (c & 0xE0) << 8
            | (c & 0x1C) << 6
            | (c & 0x03) << 3);
}


UINT32 HRES, VRES; // resolution, horizontal and vertical
UINT32 imgdataSize;
UINT32 BPP; // bits per pixel
UINT8 compression; // 0 = no compression, 1 = RLE-8, 2 = RLE-4
UINT32 start_offset;

void readBMP24(FSFILE * pointer, unsigned short x, unsigned short y) {
    UINT8 dataStream[720];
    UINT32 nr, nc;
    UINT32 rowSize = 4 - ((HRES * 3) % 4);
    if (rowSize == 4) rowSize = HRES * 3;
    else rowSize = (HRES * 3) + rowSize;

    // for now assume compression = 0

    tft_setAddrWindow(x, y, x + HRES - 1, y + VRES - 1);

    for (nr = 0; nr < VRES; nr++) {
        SD_Config();
        Mode8();
        //        if (FSfread(dataStream, 1, rowSize, pointer) != rowSize)
        if (FSfread(dataStream, rowSize, 1, pointer) != 1)
            break;
        for (nc = 0; nc < HRES * 3; nc += 3) {
            tft_pushColor(tft_Color565(dataStream[nc + 2], // R
                    dataStream[nc + 1], // G
                    dataStream[nc])); // B
        }
    }
}

void readBMP_not24(FSFILE * pointer) {

}

INT8 readBMP(char image[], unsigned short x, unsigned short y) {
    UINT8 bmpStream[200];
    char textBuffer[50];
    UINT32 temp32;
    FSFILE * pointer = NULL;

    if (debugbmp) {
        tft_setCursor(0, 0);
        tft_setTextSize(1);
        tft_setTextColor(ILI9340_GREEN);
        tft_writeString("In readBMP_header\n");
        tft_writeString("Opening \"img.bmp\"\n");


    }
    SD_Config();
    Mode8();
    pointer = FSfopen(image, "r");
    if (pointer == NULL) {
        if (debugbmp) {
            tft_writeString("Failed to open \"img.bmp\"\n");
        }
        return -1;
    }
    if (debugbmp) {
        tft_writeString("Opened \"img.bmp\"\n");
        SD_Config();
        Mode8();
    }
    if (FSfread(bmpStream, 1, 14, pointer) != 14) {
        FSfclose(pointer);
        return -1;
    }
    if (debugbmp) {
        tft_writeString("Read 14 bytes\n");
        SD_Config();
    }

    if ((bmpStream[0] != 'B') & (bmpStream[1] != 'M')) {
        Mode8();
        FSfclose(pointer);
        tft_writeString("Not BMP file\n");
        return -1;
    }
    if (debugbmp) {
        tft_writeString("BMP file confirmed\n");
        SD_Config();
    }

    temp32 = bmpStream[5] << 24 | bmpStream[4] << 16 |
            bmpStream[3] << 8 | bmpStream[2];

    if (debugbmp) {
        sprintf(textBuffer, "Size = %u bytes\n", temp32);
        tft_writeString(textBuffer);
    }

    temp32 = bmpStream[13] << 24 | bmpStream[12] << 16 |
            bmpStream[11] << 8 | bmpStream[10];
    if (debugbmp) {
        sprintf(textBuffer, "Start offset = %u\n", temp32);
        tft_writeString(textBuffer);
    }
    start_offset = temp32;


    SD_Config();
    Mode8();
    FSfseek(pointer, 0, SEEK_SET);
    if (FSfread(bmpStream, 1, start_offset, pointer) != start_offset)
        return -1;

    temp32 = bmpStream[17] << 24 | bmpStream[16] << 16 |
            bmpStream[15] << 8 | bmpStream[14];
    if (debugbmp) {
        sprintf(textBuffer, "Bitmap info header size = %u\n", temp32);
        tft_writeString(textBuffer);
    }
    temp32 = bmpStream[21] << 24 | bmpStream[20] << 16 |
            bmpStream[19] << 8 | bmpStream[18];
    if (debugbmp) {
        sprintf(textBuffer, "Image width = %u pixels\n", temp32);
        tft_writeString(textBuffer);
    }
    HRES = temp32;

    temp32 = bmpStream[25] << 24 | bmpStream[24] << 16 |
            bmpStream[23] << 8 | bmpStream[22];
    if (debugbmp) {
        sprintf(textBuffer, "Image height = %u pixels\n", temp32);
        tft_writeString(textBuffer);
    }
    VRES = temp32;

    temp32 = bmpStream[27] << 8 | bmpStream[26];
    if (debugbmp) {
        sprintf(textBuffer, "Number of planes in image = %u\n", temp32);
        tft_writeString(textBuffer);
    }

    temp32 = bmpStream[29] << 8 | bmpStream[28];
    if (debugbmp) {
        sprintf(textBuffer, "Bits per pixel = %u\n", temp32);
        tft_writeString(textBuffer);
    }
    BPP = temp32;

    temp32 = bmpStream[33] << 24 | bmpStream[32] << 16 |
            bmpStream[31] << 8 | bmpStream[30];
    if (debugbmp) {
        sprintf(textBuffer, "Compression type = %u\n", temp32);
        // 0 = no compression, 1 = RLE-8, 2 = RLE-4
        tft_writeString(textBuffer);
    }
    compression = temp32;

    temp32 = bmpStream[37] << 24 | bmpStream[36] << 16 |
            bmpStream[35] << 8 | bmpStream[34];
    if (debugbmp) {
        sprintf(textBuffer, "Data Size (with padding) = %u\n", temp32);
        tft_writeString(textBuffer);
    }
    imgdataSize = temp32;

    temp32 = bmpStream[49] << 24 | bmpStream[48] << 16 |
            bmpStream[47] << 8 | bmpStream[46];
    if (debugbmp) {
        sprintf(textBuffer, "Number of colors in image = %u\n", temp32);
        tft_writeString(textBuffer);
    }

    temp32 = bmpStream[53] << 24 | bmpStream[52] << 16 |
            bmpStream[51] << 8 | bmpStream[50];
    if (debugbmp) {
        sprintf(textBuffer, "Number of important colors = %u\n", temp32);
        tft_writeString(textBuffer);
    }
    //    delay_ms(1000);


    SD_Config();
    Mode8();
    FSfseek(pointer, start_offset, SEEK_SET); // go to beginning of data

    if (BPP == 24) {
        //        tft_fillScreen(ILI9340_BLACK);
        readBMP24(pointer, x, y);
    } else {
        //        tft_fillScreen(ILI9340_BLACK);
        readBMP_not24(pointer);
    }

    SPISTAT = 0;
    Mode8();
    FSfclose(pointer);
    return 0;
}


static char lastmin = -1;//initial values of -1, so time thread must update time and display it to the screen
static char lastsec = -1;
static char lasthour = -1;
static char lastmday = -1;
static char lastwday = -1;
static char lastmonth = -1;
static char lastyear = -1;
static int hour = -1;

static PT_THREAD(protothread_timer(struct pt *pt)) {
    PT_BEGIN(pt);
    //     tft_setCursor(0, 0);
    //     tft_setTextColor(ILI9340_WHITE);  tft_setTextSize(1);
    while (1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(30);
        // draw sys_time
        tft_setTextColor(ILI9340_WHITE);
        tft_setTextSize(2);
        //        sprintf(buffer,"%d", sys_time_seconds);
        //        tft_writeString(buffer);
        tm1.l = RtccGetTime();
        dt1.l = RtccGetDate();
        tft_setRotation(1);
        switch (state) {
            case 0://home screen

                if ((tm1.min != lastmin)) {//only update hours and minutes when minutes changes


                    if(theme == 0){//different update for different theme
                        tft_fillRect(0, 90, 60, 20, ILI9340_BLACK); // x,y,w,h,radius,color
                        tft_setCursor(0, 90);
                        sprintf(buffer, "%02x%s%02x", tm1.hour, ":", tm1.min);
                        tft_setTextColor(ILI9340_WHITE);
                        tft_writeString(buffer);
                        lastmin = tm1.min;//save minute vaue
                    }
                    else if(theme == 1){
                        tft_setRotation(1);
                        tft_writecommand(0x36); //Set Scanning Direction
                        tft_writedata(0x28);//change screen orientation so that image writes to correct location
                        SPISTAT = 0;
                        sprintf(buffer, "%s", "vault-tec_corner.bmp");//"space.bmp");
                        readBMP(buffer, 0, 103);
                        tft_setRotation(1);
                        sprintf(buffer, "%02x%s%02x", tm1.hour, ":", tm1.min);
                        tft_setCursor(0, 0);
                        tft_setTextColor(GREEN);
                        tft_writeString(buffer);
                        lastmin = tm1.min;
                    }
                    else{
                        tft_writecommand(0x36); //Set Scanning Direction
                        tft_writedata(0x28);//change screen orientation so that image writes to correct location
                        SPISTAT = 0;
                        sprintf(buffer, "%s", "bruce_corner.bmp");//"space.bmp");
                        readBMP(buffer, 0, 109);
                        tft_setRotation(1);
//                        tft_fillRect(0, 0, 75, 19, ILI9340_BLACK); // x,y,w,h,radius,color
                        sprintf(buffer, "%02x%s%02x", tm1.hour, ":", tm1.min);
                        tft_setCursor(1, 1);
                        tft_setTextColor(ILI9340_BLACK);
                        tft_writeString(buffer);
                        lastmin = tm1.min;
                    }
                }
                if (!(tm1.sec == lastsec)) {//only update second counter every second
                    tft_setTextSize(1);

                    if(theme == 0){//different update for different theme
                        tft_fillRect(60, 90, 20, 20, ILI9340_BLACK); // x,y,w,h,radius,color
                        sprintf(buffer, "%02x", tm1.sec);
                        tft_setCursor(60, 96);
                        tft_setTextColor(ILI9340_WHITE);
                        tft_writeString(buffer);
                    }
                    else if (theme == 1){
                        tft_setRotation(1);
                        tft_writecommand(0x36); //Set Scanning Direction
                        tft_writedata(0x28);
                        SPISTAT = 0;
                        sprintf(buffer, "%s", "vault-tec_corner_s.bmp");//
                        readBMP(buffer, 60, 103);
                        tft_setRotation(1);
                        tft_setCursor(60, 7);
                        sprintf(buffer, "%02x", tm1.sec);
                        tft_setTextColor(GREEN);
                        tft_writeString(buffer);
                    }
                    else{
                        tft_writecommand(0x36); //Set Scanning Direction
                        tft_writedata(0x28);
                        SPISTAT = 0;
                        sprintf(buffer, "%s", "Bruce_corner_s.bmp");//
                        readBMP(buffer, 60, 109);
//                        tft_fillRect(60, 0, 15, 19, ILI9340_BLACK); // x,y,w,h,radius,color
                        tft_setRotation(1);
                        tft_setCursor(61, 1);
                        sprintf(buffer, "%02x", tm1.sec);
                        tft_setTextColor(ILI9340_BLACK);
                        tft_writeString(buffer);
                    }

                    lastsec = tm1.sec;

                }
                if (!(dt1.wday == lastwday)) {//only update date when day changes
                    if(theme == 0){
                        tft_fillRect(0, 108, 80, 20, ILI9340_BLACK); // x,y,w,h,radius,color
                        tft_fillRect(110, 117, 40, 20, ILI9340_BLACK); // x,y,w,h,radius,color

                    }
                    else if(theme==1){
                        tft_writecommand(0x36); //Set Scanning Direction
                        tft_writedata(0x28);

                        SPISTAT = 0;
                        sprintf(buffer, "%s", "vault-tec_bottom.bmp");//
                        readBMP(buffer, 0, 0);
                        tft_setRotation(1);
                    }
                    else{
                         tft_setTextColor(ILI9340_WHITE);

//                        tft_fillRect(0, 108, 160, 20, ILI9340_BLACK); // x,y,w,h,radius,color
                    }
                    switch (dt1.wday) {//write out day of week
                        case 0:
                            sprintf(buffer, "%s", "Sunday");
                            break;
                        case 1:
                            sprintf(buffer, "%s", "Monday");
                            break;
                        case 2:
                            sprintf(buffer, "%s", "Tuesday");
                            break;
                        case 3:
                            sprintf(buffer, "%s", "Wednesday");
                            break;
                        case 4:
                            sprintf(buffer, "%s", "Thursday");
                            break;
                        case 5:
                            sprintf(buffer, "%s", "Friday");
                            break;
                        default:
                            sprintf(buffer, "%s", "Saturday");
                    }



                    tft_setTextSize(2);
                    tft_setCursor(0, 111);
                    tft_writeString(buffer);
                    lastwday = dt1.wday;

                    sprintf(buffer, "%02x%s%02x",  dt1.mon, "/", dt1.mday);

                    tft_setTextSize(1);
                    tft_setCursor(110, 120);
                    tft_writeString(buffer);
                    lastwday = dt1.wday;

                }
                break;
            case 2://settings menu


                if (!(tm1.sec == lastsec)) {//update every second
                    tft_setCursor(0, 45);
                    tft_setTextSize(1);
                    tft_fillRect(0, 45, 160, 10, ILI9340_BLACK);
                    sprintf(buffer, "%s%02x%s%02x%s%02x%s%02x%s%02x%s%02x", "Time: ", dt1.mon, "/", dt1.mday, "/", dt1.year, "  ", tm1.hour, ":", tm1.min, ":", tm1.sec);
                    tft_writeString(buffer);
                    lastsec = tm1.sec;
                }
                break;
        }

        // NEVER exit while
    } // END WHILE(1)
    PT_END(pt);
} // timer thread



void drawstate0() {//draws home screen
    time = gameTime;
    tft_setRotation(1);
    tft_writecommand(0x36); //Set Scanning Direction
    tft_writedata(0x28);
    SPISTAT = 0;
    sprintf(buffer, "%s", background);//"space.bmp");
    readBMP(buffer, 0, 0);
    lastmin = -1;//sets last values to -1 to force timer thread to redraw all dates and times (redraws when current different than last)
    lastsec = -1;
    lasthour = -1;
    lastmday = -1;
    lastwday = -1;
    lastmonth = -1;
    lastyear = -1;
    hour = -1;
    //    tft_drawRoundRect(150, 50, 20, 28, 1, 0x03EF);
}

void drawstate1() {//draw app menu
    tft_setTextColor(ILI9340_WHITE);
    tft_fillScreen(ILI9340_BLACK);
    tft_setRotation(1);
    tft_writecommand(0x36); //Set Scanning Direction
    tft_writedata(0x28);//change screen orientation for image
    SPISTAT = 0;
    sprintf(buffer, "%s", settingsImg);//"gears.bmp");
    readBMP(buffer, 0, 75);
    sprintf(buffer, "%s", gameImg);//"Controller.bmp");
    readBMP(buffer, 55, 75);
    sprintf(buffer, "%s", paintImg);//"paintbrush.bmp");
    readBMP(buffer, 110, 75);
}

void drawstate2() {//draw settings app
    tft_setRotation(1);
    tft_fillScreen(ILI9340_BLACK);
    tft_setCursor(50, 3);
    tft_setTextSize(1);
    tft_writeString("brightness:");
    tft_fillRoundRect(5, 20, 150, 4, 2, ILI9340_WHITE);//draw slider
    tft_fillCircle(savePWM + 5, 21, 6, ILI9340_TEAL);//draw slider
    tft_drawLine(0, 34, 160, 34, 0x03FF);//partitions between sections
    tft_drawLine(0, 60, 160, 60, 0x03FF);
    tft_fillTriangle(20, 106, 20, 125, 10, 116, ILI9340_WHITE);//back button

//    tft_drawLine(29, 60, 29, 159, 0x03FF);
//    tft_drawLine(48, 60, 48, 159, 0x03FF);
//    tft_drawLine(67, 60, 67, 159, 0x03FF);
//    tft_drawLine(86, 60, 86, 159, 0x03FF);
//    tft_drawLine(105, 60, 105, 159, 0x03FF);
//    tft_drawLine(124, 60, 124, 159, 0x03FF);

    switch (dt.wday) {//draw highlighting rectangle behind current day
        case 0:
            tft_fillRoundRect(11, 67, 20, 13, 2, ILI9340_TEAL);
            break;
        case 1:
            tft_fillRoundRect(30, 67, 20, 13, 2, ILI9340_TEAL);
            break;
        case 2:
            tft_fillRoundRect(48, 67, 20, 13, 2, ILI9340_TEAL);
            break;
        case 3:
            tft_fillRoundRect(66, 67, 20, 13, 2, ILI9340_TEAL);
            break;
        case 4:
            tft_fillRoundRect(86, 67, 20, 13, 2, ILI9340_TEAL);
            break;
        case 5:
            tft_fillRoundRect(106, 67, 20, 13, 2, ILI9340_TEAL);
            break;
        case 6:
            tft_fillRoundRect(128, 67, 20, 13, 2, ILI9340_TEAL);
            break;

    }
    tft_setCursor(50, 96);
    tft_setTextSize(1);
    tft_writeString("Theme:");

    switch (theme) {//draw highlighting rectangle behind current theme
        case 0:
            tft_fillRoundRect(86, 96, 50, 13, 2, ILI9340_TEAL);
            break;
        case 1:
            tft_fillRoundRect(86, 111, 50, 13, 2, ILI9340_TEAL);
            break;
        case 2:
            tft_fillRoundRect(86, 81, 50, 13, 2, ILI9340_TEAL);
            break;
//        case 3:
//            tft_fillRoundRect(66, 67, 20, 13, 2, 0x03EF);
//            break;
//        case 4:
//            tft_fillRoundRect(86, 67, 20, 13, 2, 0x03EF);
//            break;
//        case 5:
//            tft_fillRoundRect(106, 67, 20, 13, 2, 0x03EF);
//            break;
//        case 6:
//            tft_fillRoundRect(128, 67, 20, 13, 2, 0x03EF);
//            break;
//
    }

    tft_setCursor(0, 70);
    tft_setTextSize(1);
    tft_setTextColor(ILI9340_WHITE);
    tft_writeString("   S  M  T  W  Th  F  Sa");
    tft_setCursor(90, 98);
    tft_writeString("Space");
    tft_setCursor(90, 113);
    tft_writeString("Fallout");
    tft_setCursor(90, 83);
    tft_writeString("Bruce");

}

void drawstate3() {//draw time set state
    tft_fillRect(0, 59, 160, 128, ILI9340_BLACK);
    tft_drawLine(0, 77, 160, 77, 0x03FF);//grid for keypad
    tft_drawLine(0, 94, 160, 94, 0x03FF);
    tft_drawLine(0, 111, 160, 111, 0x03FF);
    tft_drawLine(53, 60, 53, 159, 0x03FF);
    tft_drawLine(106, 60, 106, 159, 0x03FF);


    //numbers of keypad
    tft_setTextColor(ILI9340_WHITE);
    tft_setTextSize(2);
    tft_setCursor(20, 62);
    tft_writeString("1");
    tft_setCursor(73, 62);
    tft_writeString("2");
    tft_setCursor(126, 62);
    tft_writeString("3");
    tft_setCursor(20, 79);
    tft_writeString("4");
    tft_setCursor(73, 79);
    tft_writeString("5");
    tft_setCursor(126, 79);
    tft_writeString("6");
    tft_setCursor(20, 96);
    tft_writeString("7");
    tft_setCursor(73, 96);
    tft_writeString("8");
    tft_setCursor(126, 96);
    tft_writeString("9");

    //backspace key
    tft_fillTriangle(30, 113, 30, 125, 20, 119, ILI9340_WHITE);
    tft_setCursor(73, 113);
    tft_writeString("0");

    //check mark
    tft_drawLine(126, 120, 131, 125, ILI9340_WHITE);
    tft_drawLine(131, 125, 136, 113, ILI9340_WHITE);

}

void drawstate4(){//draw game start state
    tft_fillScreen(ILI9340_BLACK);
    tft_setRotation(1);

    tft_setCursor(0, 30);
    tft_setTextColor(ILI9340_WHITE);
    tft_setTextSize(1);
    sprintf(buffer, "%s", "  Number of Balls:");//user selects number of balls
    tft_writeString(buffer);

    tft_fillRect(0,0, 160, 15, ILI9340_TEAL);//continue button
    tft_setCursor(0, 0);
    tft_setTextColor(ILI9340_WHITE);
    tft_setTextSize(1);
    sprintf(buffer, "%s", "         continue");
    tft_writeString(buffer);

    tft_fillRect(0, 50, 160, 15, ILI9340_BLACK);
    tft_fillRoundRect(5, 55, 150, 4, 2, ILI9340_WHITE);
    tft_fillCircle(80, 56, 6, ILI9340_TEAL);//slider for num balls select
    numBalls = 17;

    tft_fillRect(0, 108, 20, 20, ILI9340_RED);
    tft_fillTriangle( 18, 110, 18, 126, 2, 118,ILI9340_WHITE);//button to go back

    tft_setTextSize(3);
    tft_setCursor(60,70);
    tft_fillRect(40,70,160, 30,ILI9340_BLACK );
    sprintf(buffer, "%d", numBalls);
    tft_writeString(buffer);
}
void drawstate5(){// draw game state
    tft_fillRect(0, 0, 20, 20, ILI9340_RED);
    tft_fillTriangle( 18, 2, 18, 18, 2, 10,ILI9340_WHITE);
    tft_fillScreen(ILI9340_BLACK);

    OpenTimer4(T4_ON | T4_SOURCE_INT | T4_PS_1_1, 40000); //timer 4 is required for the game

    // set up the timer interrupt with a priority of 2
    ConfigIntTimer4(T4_INT_ON | T4_INT_PRIOR_6);
    mT4ClearIntFlag(); // and clear the interrupt flag
    ended = 0; // if game end flag was set, reset it
}

drawstate6(){//paint app
    tft_setRotation(1);
    tft_fillRect(0, 0, 20, 20, ILI9340_RED);
    tft_fillTriangle( 18, 2, 18, 18, 2, 10,ILI9340_WHITE);//back button

    tft_fillRect(0,108, 20, 20, ILI9340_MAGENTA);//draw color selections
    tft_fillRect(20,108, 20, 20, ILI9340_RED);
    tft_fillRect(40,108, 20, 20, 0xFD20);//orange
    tft_fillRect(60,108, 20, 20, ILI9340_YELLOW);
    tft_fillRect(80,108, 20, 20, ILI9340_GREEN);
    tft_fillRect(100,108, 20, 20, ILI9340_BLUE);//
    tft_fillRect(120,108, 20, 20, 0x780F);//purple
    tft_fillRect(140,108, 20, 20, ILI9340_WHITE);
    tft_fillRect(140,0, 20, 20, color);





}

// === Animation Thread =================================================
// Animates game
static PT_THREAD(protothread_anim(struct pt *pt)) {

    PT_BEGIN(pt);
    static int adc;//ADC value
    static int radius = 1;//ball radius
    static fix16 drag = float2fix16(0.001); //.001
    static int bumperPos;//Bumper position, determined by ADC val
    static int lastBumperPos;//used to determine bumper velocity
    static int bumperV;//bumper velocity
    while (1) {
        tft_fillRect(0, 0, 20, 20, ILI9340_RED);
        tft_fillTriangle( 18, 2, 18, 18, 2, 10,ILI9340_WHITE);
        tft_fillRect(20, 0, 80, 20, ILI9340_TEAL); // x,y,w,h,color
        tft_setCursor(20, 0);
        tft_setTextColor(ILI9340_WHITE);
        tft_setTextSize(1);
        sprintf(buffer, "%s%d", "Score:", score);//print current score
        tft_writeString(buffer);
        tft_fillRect(0, 40, 1, 48, ILI9340_RED); // x,y,w,h,color
        tft_fillRect(158, 40, 1, 48, ILI9340_RED); // x,y,w,h,color

        tft_setCursor(20, 8);
        tft_setTextColor(ILI9340_WHITE);
        tft_setTextSize(1);
        sprintf(buffer, "%s%d", "High Score:", highScore);//print High score
        tft_writeString(buffer);

        int i;
        for (i=0; i < numBalls; i++) {//add new balls to game, up to ball max
            if ((inPlay[i] == 0 && counter > 1)) { //&& counter > 1) {
                vx[i] = (rand() << 1)&0x8FFFF - 0x78000; //random x velocity
                vy[i] = -((rand() << 2 + 0x20FFF)&0x8FFFF);//random y velocity, min speed set
                x[i] = int2fix16(40) + ((rand()<<1)&0x40FFFF);//random x coordinate
                y[i] = int2fix16(25);//always spawn from top of screen
                counter = 0;
                inPlay[i] = 1;//ball is now in play
                startTime[i] = PT_GET_TIME();
                break;
            }
        }

        for (i = 0; i < numBalls; i++) {
            if (hitCount[i] > 0) {
                hitCount[i]--;//decrement hit counter, when it reaches 0, ball can collide again
                continue;//ball had non-zero hit count, move on to next one for collision calc
            }

            if (inPlay[i] == 0) {
                continue;//if it is not in play, move on to next one for collision calc
            }

            int j = i + 1;
            for (j; j < numBalls; j++) {
                if (inPlay[j] == 0) {
                    continue;//if it is not in play, move on to next one for collision calc
                }
                if (hitCount[j] > 0) {
                    continue;//ball had non-zero hit count, move on to next one for collision calc

                }
                fix16 rijx = x[i] - x[j];// x distance
                fix16 rijy = y[i] - y[j];// y distance
                if (((abs(fix2int16(rijx)) + abs(fix2int16(rijy))) < 5)) {//manhattan distance
                    fix16 vijx = vx[i] - vx[j];// relative x velocity
                    fix16 vijy = vy[i] - vy[j];// relative y velocity
                    fix16 dist2 = multfix16(rijx, rijx) + multfix16(rijy, rijy) + 0x01;//add small number to distance^2 to make sure divide by 0 does not occur
                    fix16 del_vix = -divfix16((multfix16(rijx, (multfix16(rijx, vijx) + multfix16(rijy, vijy)))), dist2);// calculate change in x velocity
                    fix16 del_viy = -divfix16((multfix16(rijy, (multfix16(rijx, vijx) + multfix16(rijy, vijy)))), dist2);// calculate change in y velocity
                    vx[i] += del_vix;//recalculate velocities
                    vy[i] += del_viy;//recalculate velocities
                    vx[j] -= del_vix;//recalculate velocities
                    vy[j] -= del_viy;//recalculate velocities
                    hitCount[i] = 5;//add hit count
                    hitCount[j] = 5;
                }

            }
        }

        adc = 160 - yTouch; // read the result of channel 9 conversion from the idle buffer
        if (adc > 160) {
            adc = 160;
        }
        lastBumperPos = bumperPos;
        bumperPos = adc - bumperWidth;//divide ADC value by 4, (makes max value 320, which is the screen width
 //subtract bumper width(so bumper cannot move off screen)
        if (bumperPos < 0) {
            bumperPos = 0;//make sure bumper does not move off of screen
        }

        bumperV = bumperPos - lastBumperPos;//calculate bumper velocity

        tft_fillRect(0, 118, 160, 10, ILI9340_BLACK); // x,y,w,h,color
        tft_fillRoundRect(bumperPos, 118, bumperWidth, 10, 1, ILI9340_MAGENTA); // draw bumper

        i = 0;
        for (i; i < numBalls; i++) {
            //erase ball
            if (!((fix2int16(x[i]) > bumperPos) && (fix2int16(x[i])<(bumperPos + bumperWidth)) && (fix2int16(y[i]) > 118)) && (y[i] > int2fix16(16))) {
                tft_fillCircle(fix2int16(x[i]), fix2int16(y[i]), radius, ILI9340_BLACK); //x, y, radius, color
            }

            // compute new velocities, after drag takes effect
            vy[i] = vy[i] - multfix16(vy[i], drag);
            vx[i] = vx[i] - multfix16(vx[i], drag);

            //change coordinates based on velocities
            x[i] = x[i] + vx[i];
            y[i] = y[i] - vy[i];
            if ((y[i] > int2fix16(129)) && (inPlay[i])) {//check if balls have fallen to bottom
                inPlay[i] = 0;
                if (i == 3 || i == 17 || i == 27 || i == 37) {//Special balls!
                    score -= 10;//special balls worth more points, you lose more for letting them go
                }
                if (i == 5) {//Power up ball!
                    bumperWidth -= 15;//Power up ball changes paddle size
                    if (bumperWidth <= 0) {//Check if bumper has disappeared
//                        PT_SPAWN(pt, &pt_end, protothread_end(&pt_end)); //end the game
                        ended = 1;
                            tft_setCursor(300, 50);
                            tft_writeString("Game Over");
                            if(score > highScore){//New high score!
                                tft_setCursor(0, 80);
                                tft_setTextColor(ILI9340_WHITE);
                                tft_setTextSize(1);
                                sprintf(buffer, "%s", "   New High Score!");//Print new high score!
                                tft_writeString(buffer);
                                highScore = score;
                            }

                            int i;
                            for (i = 0; i < numBalls; i++) {//remove all balls from play
                                inPlay[i] = 0;
                            }
                                tft_fillRect(20, 100, 120, 20, ILI9340_TEAL);
                                tft_setCursor(30, 105);
                                tft_writeString("Play again?");
                                CloseTimer4();

                            mPORTBSetBits(BIT_7);//vibration motor on
                            PT_YIELD_TIME_msec(500);
                            mPORTBClearBits(BIT_7);
                        while(ended){
                            //wait for user to press play again
                            if (xTouch > 100 && lastxTouch > 100 && yTouch > 20 && lastyTouch > 20 && yTouch <140 && lastyTouch < 140 && touch_point.Z) {
                                drawstate4();
                                state = 4;
                                score = 0;
                                ended = 0;
                                time = gameTime;
                                stateTransition = 0;
                                bumperWidth = 50;
                            }
                            PT_YIELD_TIME_msec(30);
                        }
                    }
                } else {
                    score--;//normal ball has reached bottom, decrement score
                }

            } else if ((fix2int16(x[i]) > bumperPos) && (fix2int16(x[i])<(bumperPos + bumperWidth)) && (fix2int16(y[i]) > 116)) {//Check if balls have hit paddle
                vy[i] = -vy[i];//reflect y velocity
                vx[i] += (int2fix16(bumperV) >> 2); // paddle friction, make coefficient of friction 0.25
                hitCount[i] = 5;

            }

            if (x[i] < int2fix16(2) || x[i] > int2fix16(158)) {//if x out of bounds
                if ((y[i] < int2fix16(88) && y[i] > int2fix16(40)) && (inPlay[i])) {//if it has gone through the gate
                    inPlay[i] = 0;
                    if (i == 3 || i == 17 || i == 27 || i == 37) {//Special balls
                        score += 10;//Extra points!
                    }
                    if (i == 5) {//Power up Ball
                        bumperWidth += 15;//paddle is larger now!
                    } else {
                        score++;//normal ball, increment score
                    }


                } else {//it has hit the wall
                    vx[i] = -vx[i];//reflect x velocity
                }
            }

            if (y[i] < int2fix16(18)) vy[i] = -vy[i];//if ball has hit top of screen, reflect y velocity

            if((PT_GET_TIME()  > startTime[i] + 15000)&& inPlay[i]){
                inPlay[i] = 0;//remove balls that have been in play for 15 seconds
                score ++;//increment score
            }

            if (inPlay[i] && !((fix2int16(x[i]) > bumperPos) && (fix2int16(x[i])<(bumperPos + bumperWidth)) && (fix2int16(y[i]) > 118)) && (y[i] > int2fix16(16))) {//draw balls
                if ((i == 3 || i == 17 || i == 27 || i == 37)) {
                    tft_fillCircle(fix2int16(x[i]), fix2int16(y[i]), radius, ILI9340_RED);
		     //special balls have a special color
                }
                else if (i == 5) {
                    tft_fillCircle(fix2int16(x[i]), fix2int16(y[i]), radius, ILI9340_BLUE);
		     //special color for powerup ball
                } else {
                    tft_fillCircle(fix2int16(x[i]), fix2int16(y[i]), radius, ILI9340_GREEN);
			//draw green ball
                }
            }
        }

        counter++;
        numFrames++;

        if (time == 0) {//spawn end game thread if no time yet]
            ended = 1;
                            tft_setCursor(300, 50);
                            tft_writeString("Game Over");
                            if(score > highScore){//New high score!
                                tft_setCursor(0, 80);
                                tft_setTextColor(ILI9340_WHITE);
                                tft_setTextSize(1);
                                sprintf(buffer, "%s", "   New High Score!");//Print new high score!
                                tft_writeString(buffer);
                                highScore = score;
                            }

                            int i;
                            for (i = 0; i < numBalls; i++) {//remove all balls from play
                                inPlay[i] = 0;
                            }
                                tft_fillRect(20, 100, 120, 20, ILI9340_TEAL);
                                tft_setCursor(30, 105);
                                tft_writeString("Play again?");
                                CloseTimer4();

                            stateTransition = 0;
                            mPORTBSetBits(BIT_7);//vibration motor on
                            PT_YIELD_TIME_msec(500);
                            mPORTBClearBits(BIT_7);
                        while(ended){
                            //wait for the user to play again
                            if (xTouch > 100 && lastxTouch > 100 && yTouch > 20 && lastyTouch > 20 && yTouch <140 && lastyTouch < 140 && touch_point.Z) {
                                drawstate4();
                                state = 4;
                                ended = 0;
                                score = 0;
                                time = gameTime;
                                bumperWidth = 50;
                                stateTransition = 0;
                            }
                            PT_YIELD_TIME_msec(30);
                        }
        }
        if (msTimer >= 1000) {//if a second has elapsed
            time--;//decrement game time
            msTimer = 0;
            tft_fillRect(90, 0, 70, 20, ILI9340_TEAL); // x,y,w,h,color
            tft_setCursor(100, 0);
            tft_setTextColor(ILI9340_WHITE);
            tft_setTextSize(1);
            sprintf(buffer, "%s%d", "FPS:", numFrames);//output frame rate
            tft_writeString(buffer);
            numFrames = 0;


            tft_setCursor(100, 8);
            tft_setTextColor(ILI9340_WHITE);
            tft_setTextSize(1);
            sprintf(buffer, "%s%d", "Time:", time);
            tft_writeString(buffer);

        }
        PT_YIELD_TIME_msec(10);
        PT_SEM_WAIT(&pt_anim, &sem);//wait for rate-limiting thread to release semaphore
        // NEVER exit while
    } // END WHILE(1)

    PT_END(pt);
} // timer thread


void __ISR(_TIMER_4_VECTOR, ipl6) Timer4Handler(void) {
    mT4ClearIntFlag();
    if (ended) {//do nothing if game has ended
    }
    else {
        msTimer++;//keeps track of ms, used to determine when game ends
    }
}


volatile UINT16 XMIN = 130, YMIN = 95, XMAX = 820, YMAX = 780;


static unsigned char timeMod; //keeps track of which timer parameters are being modified
static unsigned char keyPress;
static unsigned char charPress;
static unsigned char newYr;
static unsigned char newMo;
static unsigned char newDay;
static unsigned char newHour;
static unsigned char newMin;
static unsigned char newSec;

static PT_THREAD(protothread_touch(struct pt *pt)) {
    PT_BEGIN(pt);

#define t_active	50
#define t_idle		4
    while (1) {

        // yield time 5 milliseconds
        PT_YIELD_TIME_msec(5);
        keyPress = 0;
        if (on) {//only look for touches if the screen is on
            lastxTouch = xTouch;
            lastyTouch = yTouch;
            touch_getPoint();// get X,Y coordinates of touch
            if (touch_point.DONE && touch_point.Z) {
                //				debug();
                if (touch_point.X >= XMIN && touch_point.X <= XMAX)
                    touch_point.X = touch_point.X - XMIN;
                else
                    touch_point.Z = 0; // make it an invalid touch

                if (touch_point.Y >= YMIN && touch_point.Y <= YMAX)
                    touch_point.Y = touch_point.Y - YMIN;
                else
                    touch_point.Z = 0; // make it an invalid touch
                if (stateTransition < 10) {
                    touch_point.Z = 0; // make it an invalid touch
                }
            }
            if (touch_point.Z) {
                //                tft_setRotation(0);
                xTouch = touch_point.X / 5.4;// map x value to pixel number on screen
                yTouch = touch_point.Y / 4.15;//map y value to pixel number on screen
                //                tft_fillCircle(xTouch, yTouch, 2, ILI9340_MAGENTA);
            }
            if (on) {
                OC1R = savePWM;//update screen brightness
                OC1RS = savePWM;//update screen brightness
            }
            if (stateTransition == 10) {//time has elapsed since previous transition
                switch (state) {//state machine
                    case 0:
                        if (touch_point.Z && on) {//transition to app menu
                            state = 1;
                            drawstate1();
                            stateTransition = 0;
                        }
                        break;
                    case 1://app selections

                        if (xTouch < 70 && lastxTouch < 70 && yTouch > 107 && lastyTouch > 107 && touch_point.Z) {
                            state = 2;
                            drawstate2();
                            stateTransition = 0;
                        }
                        else if (xTouch < 70 && lastxTouch < 70 && yTouch < 107 && lastyTouch < 107 && yTouch > 52 && lastyTouch > 52 && touch_point.Z) {
                            state = 4;
                            drawstate4();
                            stateTransition = 0;
                        }
                        else if (xTouch < 70 && lastxTouch < 70 && yTouch < 52 && lastyTouch < 52 && touch_point.Z) {
                            state = 6;
                            tft_fillScreen(ILI9340_BLACK);
                            drawstate6();
                            stateTransition = 0;
                        }
                        break;
                    case 2://settings menu interactions

                        //set date and time
                        if ((xTouch > 34) && (lastxTouch > 34)&&(xTouch < 60)&&(lastxTouch < 60) && stateTransition == 10 && touch_point.Z) {
                            drawstate3();
                            stateTransition = 0;
                            state = 3;
                            newYr = dt.year;
                            newMo = dt.mon;
                            newDay = dt.mday;
                            newHour = tm.hour;
                            newMin = tm.min;
                            newSec = tm.sec;

                            //go back to app menu
                        } else if ((xTouch > 105) && (lastxTouch > 105) && (yTouch > 130) && (lastxTouch < 130) && stateTransition == 10 && touch_point.Z) {
                            drawstate1();
                            stateTransition = 0;
                            state = 1;
                        }
                        break;

                        //                             tft_fillTriangle(20, 106, 20, 125, 10, 116, ILI9340_WHITE);

                    case 3://update date and time

                        //keypad presses
                        if ((xTouch > 60) && (lastxTouch > 60)&&(xTouch < 77)&&(lastxTouch < 77) && (stateTransition == 10) && touch_point.Z ) {
                            if (((yTouch > 106)&&(lastyTouch > 106))) {
                                keyPress = 1;//keypad pressed
                                stateTransition = 0;//by setting this = 0, touch is disabled briefly
                                charPress = 0x01;//1 was pressed
                            } else if ((yTouch < 107)&&(lastyTouch < 107)&&(yTouch > 54)&&(lastyTouch > 54)) {
                                keyPress = 1;//keypad pressed
                                stateTransition = 0;//by setting this = 0, touch is disabled briefly
                                charPress = 0x02;// 2 was pressed
                            } else if ((yTouch < 55)&&(lastyTouch < 55)) {
                                keyPress = 1;//keypad pressed
                                stateTransition = 0;//by setting this = 0, touch is disabled briefly
                                charPress = 0x03;// 3 was pressed
                            }
                        } else if ((xTouch > 76) && (lastxTouch > 76)&&(xTouch < 94)&&(lastxTouch < 94) && stateTransition == 10 && touch_point.Z) {
                            if (((yTouch > 106)&&(lastyTouch > 106))) {
                                keyPress = 1;//keypad pressed
                                stateTransition = 0;//by setting this = 0, touch is disabled briefly
                                charPress = 0x04;//4 was pressed
                            } else if ((yTouch < 107)&&(lastyTouch < 107)&&(yTouch > 54)&&(lastyTouch > 54)) {
                                keyPress = 1;//keypad pressed
                                stateTransition = 0;//by setting this = 0, touch is disabled briefly
                                charPress = 0x05;// 5 was pressed
                            } else if ((yTouch < 55)&&(lastyTouch < 55)) {
                                keyPress = 1;//keypad pressed
                                stateTransition = 0;//by setting this = 0, touch is disabled briefly
                                charPress = 0x06;// 6 was pressed
                            }
                        } else if ((xTouch > 93) && (lastxTouch > 93) &&(xTouch < 111)&&(lastxTouch < 111) && stateTransition == 10 && touch_point.Z) {
                            if (((yTouch > 106)&&(lastyTouch > 106))) {
                                keyPress = 1;//keypad pressed
                                stateTransition = 0;//by setting this = 0, touch is disabled briefly
                                charPress = 0x07;// 7 was pressed
                            } else if ((yTouch < 107)&&(lastyTouch < 107)&&(yTouch > 54)&&(lastyTouch > 54)) {
                                keyPress = 1;//keypad pressed
                                stateTransition = 0;//by setting this = 0, touch is disabled briefly
                                charPress = 0x08;// 8 was pressed
                            } else if ((yTouch < 55)&&(lastyTouch < 55)) {
                                keyPress = 1;//keypad pressed
                                stateTransition = 0;//by setting this = 0, touch is disabled briefly
                                charPress = 0x09;// 9 was pressed
                            }
                        } else if ((xTouch > 110) && (lastxTouch > 110) && stateTransition == 10 && touch_point.Z) {
                            if (((yTouch > 106)&&(lastyTouch > 106))) {
                                if (timeMod > 0) {
                                    timeMod--;//backspace was pressed, modify previous value
                                }
                                keyPress = 1;//keypad pressed
                                stateTransition = 0;//by setting this = 0, touch is disabled briefly
                            } else if ((yTouch < 107)&&(lastyTouch < 107)&&(yTouch > 54)&&(lastyTouch > 54)) {
                                keyPress = 1;//keypad pressed
                                stateTransition = 0;//by setting this = 0, touch is disabled briefly
                                charPress = 0x00;// 0 was pressed
                            } else if ((yTouch < 55)&&(lastyTouch < 55)) {
                                stateTransition = 0;
                                RTCCON |= 0xC6;//user is done editing date and time

                                //start up RTCC again
                                RtccEnable();
                                RtccInit(); // init the RTCC
                                RtccOpen(tm1.l, dt1.l, 0); // set time, date and calibration in a single operation
                                state = 2;// go back to settings app
                                drawstate2();//draw settings app
                                stateTransition = 0;
                                timeMod = 0;//reset mod variable so you start from beginning next time
                                keyPress = 1;
                            }
                        }
                        break;
                    case 4:// game start

                        //user wants to enter game
                        if (xTouch < 15 && lastxTouch < 15 && touch_point.Z) {
                            state = 5;
                            drawstate5();
                            stateTransition = 0;
                        }
                        //user wants to go back to app menu
                        else if (xTouch > 105 && lastxTouch > 105 && yTouch > 138 && lastyTouch > 138 && touch_point.Z) {
                            state = 1;
                            drawstate1();
                            stateTransition = 0;
                        }
                        break;
                    case 5://game
                        //user wants to go back to app menu
                        if (xTouch < 20 && lastxTouch < 20 && yTouch > 138 && lastyTouch > 138 && touch_point.Z) {
                            state = 1;
                            CloseTimer4();// not needed anymore
                            drawstate1();//redraw app menu

                            //set game up to be played again if needed
                            score = 0;
                            time = gameTime; // reset game time
                            bumperWidth = 50; //reset bumper width
                            stateTransition = 0;
                            int i;
                            for (i = 0; i < numBalls; i++) {//remove all balls from play
                                inPlay[i] = 0;
                            }

                        }
                        break;
                    case 6://draw app
                        tft_setRotation(2);
                        if(touch_point.Z){// if valid touch, draw circle where touched
                            tft_fillCircle(xTouch, yTouch, 2, color);
                        }

                        //go back to app menu
                        if (xTouch < 20 && lastxTouch < 20 && yTouch > 138 && lastyTouch > 138 && touch_point.Z) {
                            state = 1;
                            drawstate1();
                            stateTransition = 0;

                        }

                        //color selection
                        else if (xTouch > 108 && lastxTouch > 108&& yTouch > 140 && lastyTouch > 140 && touch_point.Z) {
                            color = ILI9340_MAGENTA;
                            drawstate6();//redraw colored rectangles
                        }

                        else if (xTouch > 108 && lastxTouch > 108&& yTouch > 120 && lastyTouch > 120 && yTouch < 140 && lastyTouch < 140 && touch_point.Z) {
                            color = ILI9340_RED;
                            drawstate6();

                        }
                        else if (xTouch > 108 && lastxTouch > 108&& yTouch > 100 && lastyTouch > 100 && yTouch < 120 && lastyTouch < 120 && touch_point.Z) {
                            color = 0xFD20;
                            drawstate6();

                        }
                        else if (xTouch > 108 && lastxTouch > 108&& yTouch > 80 && lastyTouch > 80 && yTouch < 100 && lastyTouch < 100 && touch_point.Z) {
                            color = ILI9340_YELLOW;
                            drawstate6();

                        }
                        else if (xTouch > 108 && lastxTouch > 108&& yTouch > 60 && lastyTouch > 60 && yTouch < 80 && lastyTouch < 80 && touch_point.Z) {
                            color = ILI9340_GREEN;
                            drawstate6();

                        }
                        else if (xTouch > 108 && lastxTouch > 108&& yTouch > 40 && lastyTouch > 40 && yTouch < 60 && lastyTouch < 60 && touch_point.Z) {
                            color = ILI9340_BLUE;
                            drawstate6();

                        }
                        else if (xTouch > 108 && lastxTouch > 108&& yTouch > 20 && lastyTouch > 20 && yTouch < 40 && lastyTouch < 40 && touch_point.Z) {
                            color = 0x780F;
                            drawstate6();

                        }
                        else if (xTouch > 108 && lastxTouch > 108&& yTouch < 20 && lastyTouch < 20 && touch_point.Z) {
                            color = ILI9340_WHITE;
                            drawstate6();

                        }
                        break;
                }
            }

        }
    }
    PT_END(pt);

} // touch thread
// ===================================================================




static int press1;

static PT_THREAD(protothread_setTime(struct pt *pt)) {
    PT_BEGIN(pt);

    while (1) {
        switch (timeMod) {
            case 0:
                bufferTime = tm1.l;
                bufferDate = dt1.l;
                RtccShutdown();//turn of RTCC so we can edit parameters
                tm.l = bufferTime;//save current time
                dt.l = bufferDate; //save current date
                tft_setCursor(0, 45);
                tft_setTextSize(1);
                tft_fillRect(0, 43, 160, 11, ILI9340_BLACK);
                tft_fillRect(34, 43, 14, 11, 0x03E0); //highlights parameters that are being edited
                sprintf(buffer, "%s%02x%s%02x%s%02x%s%02x%s%02x%s%02x", "Time: ", dt1.mon, "/", dt1.mday, "/", dt1.year, "  ", tm1.hour, ":", tm1.min, ":", tm1.sec);
                tft_writeString(buffer);
                PT_YIELD_UNTIL(pt, keyPress);
                if(timeMod != 0){
                    break;
                }
                keyPress = 0;
                press1 = charPress;

                //                tft_fillRect(40,40,40,40, 0x00FF);
                PT_YIELD_UNTIL(pt, keyPress);
                if(timeMod != 0){// if timemod has changes since yeild, break so new case can be found
                    break;
                }
                keyPress = 0;
                charPress = (press1 << 4) + charPress;
                newMo = charPress;
                dt.mon = charPress;//write new month value to RTCC
                dt1.mon = charPress;//save new month value in case RTCC edit ends prematurely
                timeMod = 1;//go to next case
                break;

            case 1:
                tft_setCursor(0, 45);
                tft_setTextSize(1);
                tft_fillRect(0, 43, 160, 11, ILI9340_BLACK);
                tft_fillRect(55, 43, 11, 11, 0x03E0); //dark green
                sprintf(buffer, "%s%02x%s%02x%s%02x%s%02x%s%02x%s%02x", "Time: ", newMo, "/", dt1.mday, "/", dt1.year, "  ", tm1.hour, ":", tm1.min, ":", tm1.sec);
                tft_writeString(buffer);
                PT_YIELD_UNTIL(pt, keyPress);
                if(timeMod != 1){
                    break;
                }
                keyPress = 0;
                press1 = charPress;

                //                tft_fillRect(40,40,40,40, 0x00FF);
                PT_YIELD_UNTIL(pt, keyPress);
                if(timeMod != 1){
                    break;
                }
                keyPress = 0;
                charPress = (press1 << 4) + charPress;
                newDay = charPress;
                dt.mday = charPress;
                dt1.mday = charPress;

                timeMod = 2;
                break;
            case 2:
                tft_setCursor(0, 45);
                tft_setTextSize(1);
                tft_fillRect(0, 43, 160, 11, ILI9340_BLACK);
                tft_fillRect(70, 43, 14, 11, 0x03E0); //dark green
                sprintf(buffer, "%s%02x%s%02x%s%02x%s%02x%s%02x%s%02x", "Time: ", newMo, "/", newDay, "/", dt1.year, "  ", tm1.hour, ":", tm1.min, ":", tm1.sec);
                tft_writeString(buffer);
                PT_YIELD_UNTIL(pt, keyPress);
                if(timeMod != 2){
                    break;
                }
                keyPress = 0;
                press1 = charPress;

                //                tft_fillRect(40,40,40,40, 0x00FF);
                PT_YIELD_UNTIL(pt, keyPress);
                if(timeMod != 2){
                    break;
                }
                keyPress = 0;
                charPress = (press1 << 4) + charPress;
                newYr = charPress;

                dt.year = charPress;
                dt1.year = charPress;
                timeMod = 3;
                break;
            case 3:
                tft_setCursor(0, 45);
                tft_setTextSize(1);
                tft_fillRect(0, 43, 160, 11, ILI9340_BLACK);
                tft_fillRect(95, 43, 14, 11, 0x03E0); //dark green
                sprintf(buffer, "%s%02x%s%02x%s%02x%s%02x%s%02x%s%02x", "Time: ", newMo, "/", newDay, "/", newYr, "  ", tm1.hour, ":", tm1.min, ":", tm1.sec);
                tft_writeString(buffer);
                PT_YIELD_UNTIL(pt, keyPress);
                if(timeMod != 3){
                    break;
                }
                keyPress = 0;
                press1 = charPress;

                //                tft_fillRect(40,40,40,40, 0x00FF);
                PT_YIELD_UNTIL(pt, keyPress);
                if(timeMod != 3){
                    break;
                }
                keyPress = 0;
                charPress = (press1 << 4) + charPress;
                newHour = charPress;

                tm.hour = charPress;
                tm1.hour = charPress;
                timeMod = 4;
                break;
            case 4:
                tft_setCursor(0, 45);
                tft_setTextSize(1);
                tft_fillRect(0, 43, 160, 11, ILI9340_BLACK);
                tft_fillRect(113, 43, 14, 11, 0x03E0); //dark green
                sprintf(buffer, "%s%02x%s%02x%s%02x%s%02x%s%02x%s%02x", "Time: ", newMo, "/", newDay, "/", newYr, "  ", newHour, ":", tm1.min, ":", tm1.sec);
                tft_writeString(buffer);
                PT_YIELD_UNTIL(pt, keyPress);
                if(timeMod != 4){
                    break;
                }
                keyPress = 0;
                press1 = charPress;

                //                tft_fillRect(40,40,40,40, 0x00FF);
                PT_YIELD_UNTIL(pt, keyPress);
                if(timeMod != 4){
                    break;
                }
                keyPress = 0;
                charPress = (press1 << 4) + charPress;
                newMin = charPress;

                tm.min = charPress;
                tm1.min = charPress;
                timeMod = 5;
                PT_YIELD_UNTIL(pt, keyPress);
                break;
            case 5:
                tft_setCursor(0, 45);
                tft_setTextSize(1);
                tft_fillRect(0, 43, 160, 11, ILI9340_BLACK);
                tft_fillRect(130, 43, 14, 11, 0x03E0); //dark green
                sprintf(buffer, "%s%02x%s%02x%s%02x%s%02x%s%02x%s%02x", "Time: ", newMo, "/", newDay, "/", newYr, "  ", newHour, ":", newMin, ":", tm1.sec);
                tft_writeString(buffer);
                PT_YIELD_UNTIL(pt, keyPress);
                if(timeMod != 5){
                    break;
                }
                keyPress = 0;
                press1 = charPress;

                //                tft_fillRect(40,40,40,40, 0x00FF);
                PT_YIELD_UNTIL(pt, keyPress);
                if(timeMod != 5){
                    break;
                }
                keyPress = 0;
                charPress = (press1 << 4) + charPress;
                newSec = charPress;
                tft_setCursor(0, 45);
                tft_setTextSize(1);
                tft_fillRect(0, 43, 160, 11, ILI9340_BLACK);
                //tft_fillRect(130, 43,14,11, 0x03E0);//dark green
                sprintf(buffer, "%s%02x%s%02x%s%02x%s%02x%s%02x%s%02x", "Time: ", newMo, "/", newDay, "/", newYr, "  ", newHour, ":", newMin, ":", newSec);
                tft_writeString(buffer);
                tm.sec = charPress;
                tm1.sec = charPress;
                timeMod = 6;
                break;
            case 6:
                //turn RTCC back on, initialize it with new values
                RTCCON |= 0xC6;
                RtccEnable();
                RtccInit(); // init the RTCC
                RtccOpen(tm.l, dt.l, 0); // set time, date and calibration in a single operation
                state = 2;
                drawstate2();
                stateTransition = 0;
                timeMod = 0;
        }



        PT_YIELD_TIME_msec(10);
    }
    PT_END(pt);
}
//static int lastNum;
static PT_THREAD(protothread_setBalls(struct pt *pt)){
    PT_BEGIN(pt);
    while(1){

        //slider for number of balls
        if ((xTouch > 45) && (lastxTouch > 45)&&(xTouch < 60)&&(lastxTouch < 60) && stateTransition == 10 && touch_point.Z) {
            if (!(numBalls == 30 - yTouch/6)) {
                numBalls = 30 - yTouch/6;//between 30 and 4 balls can be chosen

                //animate slider
                tft_setRotation(1);
                tft_fillRect(0, 50, 160, 15, ILI9340_BLACK);
                tft_fillRoundRect(5, 55, 150, 4, 2, ILI9340_WHITE);
                tft_fillCircle(160 - yTouch, 56, 6, 0x03EF);//teal
                tft_setCursor(60, 70);
                tft_setTextSize(3);
                tft_fillRect(40,70,160, 30,ILI9340_BLACK );
                sprintf(buffer, "%d", numBalls);
                tft_writeString(buffer);
            }
//

        }

        PT_YIELD_TIME_msec(30);
    }
    PT_END(pt);
}


//protothread for settings menu
static PT_THREAD(protothread_settings(struct pt *pt)) {
    PT_BEGIN(pt);
    static int bufferYTouch;//used to make sure y value not too high
    while (1) {
        if (yTouch > 155) {
            bufferYTouch = 155;
        } else {
            bufferYTouch = yTouch;
        }

        //slider for screen brightness
        if ((xTouch > 10) && (lastxTouch > 10)&&(xTouch < 34)&&(lastxTouch < 34) && stateTransition == 10 && touch_point.Z) {
            if (!(savePWM == 155 - bufferYTouch)) {
                savePWM = 155 - bufferYTouch;
                tft_setRotation(1);
                tft_fillRect(0, 15, 160, 15, ILI9340_BLACK);
                tft_fillRoundRect(5, 20, 150, 4, 2, ILI9340_WHITE);
                tft_fillCircle(savePWM + 5, 21, 6, ILI9340_TEAL);//teal
            }

        }

        //highlights current theme
        if((yTouch < 74 && lastyTouch < 74 && xTouch > 81 && lastxTouch > 81 && stateTransition == 10 && touch_point.Z)){
            tft_fillRect(86,81, 50, 46, ILI9340_BLACK);
            if((xTouch < 96) && (lastxTouch < 96 )){
                theme = 2;
                tft_fillRoundRect(86, 81, 50, 15, 2,ILI9340_TEAL);
            }
            else if((xTouch > 95) && (lastxTouch > 95)&&(xTouch < 111)&&(lastxTouch < 111)){
                theme = 0;
                tft_fillRoundRect(86, 96, 50, 15, 2,ILI9340_TEAL);
            }
            else if((xTouch > 110) && (lastxTouch > 110)){
                theme = 1;
                tft_fillRoundRect(86, 111, 50, 15,2, ILI9340_TEAL);

            }
            //print theme names
            tft_setTextSize(1);
            tft_setCursor(90, 98);
            tft_writeString("Space");
            tft_setCursor(90, 113);
            tft_writeString("Fallout");
            tft_setCursor(90, 83);
            tft_writeString("Bruce");
        }

        //change day of week
        if((xTouch > 60) && (lastxTouch > 60)&& (xTouch < 80) && (lastxTouch <80) && stateTransition == 10 && touch_point.Z){//&&(xTouch < 100)&&(lastxTouch < 100)
            bufferTime = tm1.l;
            bufferDate = dt1.l;
            RtccShutdown();//turn off RTCC to enable editing
            tm.l = bufferTime;
            dt.l = bufferDate;
            tft_fillRect(0,63, 160, 18, ILI9340_BLACK);

            //highlight current day of week
            if((yTouch >131) && (lastyTouch >131)){
                dt.wday = 0x00;
                tft_fillRoundRect(11, 67, 20, 13, 2,0x03EF);
            }
            else if((yTouch > 112) && (lastyTouch > 112)&&(yTouch < 131)&&(lastyTouch < 131)){
                dt.wday = 0x01;
                tft_fillRoundRect(30, 67, 20, 13, 2,0x03EF);
            }
            else if((yTouch > 93) && (lastyTouch > 93)&&(yTouch < 112)&&(lastyTouch < 112)){
                dt.wday = 0x02;
                tft_fillRoundRect(48, 67, 20, 13,2, 0x03EF);

            }
            else if((yTouch > 74) && (lastyTouch > 74)&&(yTouch < 93)&&(lastyTouch < 93)){
                dt.wday = 0x03;
                tft_fillRoundRect(66, 67, 20, 13,2, 0x03EF);

            }
            else if((yTouch > 55) && (lastyTouch > 55)&&(yTouch < 74)&&(lastyTouch < 74)){
                dt.wday = 0x04;
                tft_fillRoundRect(86,67, 20, 13,2, 0x03EF);

            }
            else if((yTouch > 36) && (lastyTouch > 36)&&(yTouch < 55)&&(lastyTouch < 55)){
                dt.wday = 0x05;
                tft_fillRoundRect(106, 67, 20, 13,2, 0x03EF);
            }
            else if((yTouch < 36 )&&(lastyTouch < 36)){
                dt.wday = 0x06;
                tft_fillRoundRect(128, 67, 20, 13, 2, 0x03EF);
            }

            tft_setCursor(0, 70);
            tft_setTextSize(1);
            tft_setTextColor(ILI9340_WHITE);
            tft_writeString("   S  M  T  W  Th  F  Sa");
            RTCCON |= 0xC6;
            RtccEnable();
            RtccInit(); // init the RTCC
            RtccOpen(tm.l, dt.l, 0); // set time, date and calibration in a single operation
    }

        PT_YIELD_TIME_msec(10);
    }

    PT_END(pt);
}


void getFilename(char * buffer) {
#define maxChars        20
#define _BACKSPACE_     8

    char c = 32; // 32 = space
    INT8 bufferCounter = 0;

    while (DataRdyUART2() == 0);
    c = getcUART2();

    while ((c != 10) & (c != 13)) {

        putcUART2(c);
        while (DataRdyUART2() == 0);

        *(buffer + bufferCounter) = c;

        if (c == _BACKSPACE_) {
            if (bufferCounter > 1) {
                bufferCounter -= 2;
            } else {
                bufferCounter = 0;
            }
        }

        if (++bufferCounter == maxChars)
            break;

        c = getcUART2();
    }

    printf("\n\r");

    *(buffer + bufferCounter) = '\0'; // terminate string
}

static char disableTimeout; //user has pressed button to turn screen off before timeout
#define disableTimeout state //timeout disabled in all states but starting state

void __ISR(_TIMER_3_VECTOR, ipl3) Timer3Handler(void) {//keeps track of timeout
    if (buttonPressed) {
        if (on && onTime) {//button pressed and screen is on, and it has been on
            onTime = 250;//increase onTime so screen turns off when button is released
        } else {
            on = 1;//screen was off, turn on
            onTime = 0;
        }
    } else if (onTime < 200) {
        on = 1;
        if (!disableTimeout) {
            onTime++;//increment on time if timeout is enabled (only enabled on home screen)
        }
    } else {//screen off
        on = 0;
        OC1R = 0;
        OC1RS = 0;
    }
    if (stateTransition < 10) {//increment this variable. Touch disabled until it hits 10
        stateTransition++;
    }
    mT3ClearIntFlag();
}

void main(void) {
    _width = ILI9340_TFTWIDTH;
    _height = ILI9340_TFTHEIGHT;


    SearchRec rec;
    UINT8 attributes = ATTR_MASK; // file can have any attributes

    SYSTEMConfigPerformance(SYS_CLK);
    INTEnableSystemMultiVectoredInt();


    on = 0; //screen is off by default
    ANSELA = 0;
    ANSELB = 0; // Disable analog inputs
    CM1CON = 0;
    CM2CON = 0;
    CM3CON = 0; // Disable analog comparators
    LATACLR = 1;
    //TRISACLR = 1;                           // RA0 for LED


    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);
    tft_setRotation(0);

    // timer interrupt //////////////////////////
    // Set up timer2 on,  interrupts, internal clock, prescalar 64, toggle rate
    // at 30 MHz PB clock 60 counts is two microsec
    // 400 is 100 ksamples/sec
    // 2000 is 20 ksamp/sec
    OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_64, 150);//open timer for output compare
    mT2ClearIntFlag(); // and clear the interrupt flag
    RPB15R = 5;
    OpenOC1(OC_ON | OC_TIMER2_SRC |
            OC_PWM_FAULT_PIN_DISABLE, 0x00, 0x00);

    mPORTBClearBits(BIT_7);
    mPORTBSetPinsDigitalOut(BIT_15| BIT_7);//for backlight and vibration motor

    onTime = 0; //keeps track of amount of time screen has been on
    OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_64, 31250); //0.2 s period

    // set up the timer interrupt with a priority of 2
    ConfigIntTimer3(T3_INT_ON | T3_INT_PRIOR_3);
    mT3ClearIntFlag(); // and clear the interrupt flag

    mPORTBSetPinsDigitalIn(BIT_9);

    if (!FSInit()) {
        tft_fillRect(0, 0, 20, 20, ILI9340_RED);//print this if init fails
        while (1);
    }

#pragma OSCCONSET = 0x02//, OSCIOFNC = ON // FSInit fails if this pragma is defined before it

    spicon_fs = SPI1CON;
    spicon2_fs = SPI1CON2;// reset configuration bits so that TFT writes can occur

    tft_setCursor(0, 0);
    tft_setTextSize(1);
    tft_setTextColor(ILI9340_BLUE);
    tft_writeString("Showing all BMP\nfiles in root\ndirectory:"); //
    tft_setCursor(0, 50);
    SPISTAT = 0;
    mSPI1ClearAllIntFlags();
    SD_Config();//resets SPI config and status bits
    int y = 50;
    if (FindFirst("*.bmp", attributes, &rec) == 0) {// find bmp file print name to screen
        sprintf(buffer, "%s\t%u KB\n\r", rec.filename, rec.filesize / 1024);
        tft_writeString(buffer);
        SD_Config();//resets SPI config and status bits

        while (FindNext(&rec) == 0) {//print all bmp file names
            y += 10;
            tft_setCursor(0, y);
            sprintf(buffer, "%s\t%u KB\n\r", rec.filename, rec.filesize / 1024);
            tft_writeString(buffer);
            SD_Config();//resets SPI config and status bits
        }
    } else {
        tft_writeString("No BMP files found");
    }

    drawstate0();

    tft_writecommand(0x36); //Set Scanning Direction
    tft_writedata(0xC8);

    //initialize threads
    PT_setup();
    PT_INIT(&pt_touch);
    PT_INIT(&pt_timer);
    PT_INIT(&pt_uart);
    PT_INIT(&pt_buttonPressed);
    PT_INIT(&pt_settings);
    PT_INIT(&pt_setTime);
    PT_INIT(&pt_setBalls);
    PT_INIT(&pt_fRate);
    PT_INIT(&pt_anim);
    PT_SEM_INIT(&sem, 1);


    touch_init();
    touch_reset();

    RTCCON |= 0xC6;
    RtccEnable();
    RtccInit(); // init the RTCC
    while (RtccGetClkStat() != RTCC_CLK_ON); // wait for the SOSC to be actually running and RTCC to have its clock source
    RtccSetTimeDate(0x10073000, 0x07011602); // time is MSb: hour, min, sec, rsvd. date is MSb: year, mon, mday, wday.
    RtccOpen(tm.l, dt.l, 0); // set time, date and calibration in a single operation
    savePWM = 100; //sets screen brightness
    while (1) {

        if (!on) {
            state = 0;
            drawstate0();// if screen is not on, revert state to home screen
        }
        switch (state) {
            case 1://schedule for state 1
                PT_SCHEDULE(protothread_touch(&pt_touch));
                PT_SCHEDULE(protothread_uart(&pt_uart));
                PT_SCHEDULE(protothread_button(&pt_buttonPressed));
                break;
            case 2://schedule for state 2
                PT_SCHEDULE(protothread_touch(&pt_touch));
                PT_SCHEDULE(protothread_uart(&pt_uart));
                PT_SCHEDULE(protothread_button(&pt_buttonPressed));
                PT_SCHEDULE(protothread_settings(&pt_settings));
                PT_SCHEDULE(protothread_timer(&pt_timer));
                break;
            case 3://schedule for state 3
                PT_SCHEDULE(protothread_touch(&pt_touch));
                PT_SCHEDULE(protothread_uart(&pt_uart));
                PT_SCHEDULE(protothread_button(&pt_buttonPressed));
                PT_SCHEDULE(protothread_setTime(&pt_setTime));
                break;
            case 4://schedule for state 4
                PT_SCHEDULE(protothread_touch(&pt_touch));
                PT_SCHEDULE(protothread_uart(&pt_uart));
                PT_SCHEDULE(protothread_button(&pt_buttonPressed));
                PT_SCHEDULE(protothread_setBalls(&pt_setBalls));
                break;
            case 5://schedule for state 5
                PT_SCHEDULE(protothread_uart(&pt_uart));
                PT_SCHEDULE(protothread_rate(&pt_fRate));
                PT_SCHEDULE(protothread_button(&pt_buttonPressed));
                PT_SCHEDULE(protothread_anim(&pt_anim));
                PT_SCHEDULE(protothread_touch(&pt_touch));
                break;
            default://schedule for state 0 and 6
                PT_SCHEDULE(protothread_uart(&pt_uart));
                PT_SCHEDULE(protothread_timer(&pt_timer));
                PT_SCHEDULE(protothread_button(&pt_buttonPressed));
                PT_SCHEDULE(protothread_touch(&pt_touch));
                break;
        }


    }


}

    }
}
