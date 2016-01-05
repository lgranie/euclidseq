#include <TimerOne.h>
#include <SRIO.h>
#include <PotMUX.h>
#include <LiquidCrystal.h>

//Initalize Library
LiquidCrystal lcd(2,3,4,5,6,7);

char lcd_buffer[16];
prog_char txt_EUCLI_SEQ[] PROGMEM = " EUCLIDIAN SEQ  ";
prog_char txt_EUCLI_VER[] PROGMEM = "     V 0.1      ";
PROGMEM const char* title_string[]={
  txt_EUCLI_SEQ, txt_EUCLI_VER};
  
void printLcd(char* l1, char* l2) {
  lcd.begin(16,2);
  strcpy_P(lcd_buffer, l1);
  lcd.print(lcd_buffer);
  lcd.setCursor(0,1);
  strcpy_P(lcd_buffer, l2);
  lcd.print(lcd_buffer);
}

//Special character to seperat to zone on the lcd
uint8_t space_left[8]={
  B00001,
  B00001,
  B00001,
  B00001,
  B00001,
  B00001,
  B00001,
  B00001};

uint8_t space_right[8]={
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000};
  
//Mode define
#define MODE_PATTERN_PLAY  0
#define MODE_PATTERN_WRITE 1
//#define MODE_SONG_PLAY     2
//#define MODE_SONG_WRITE    3
//#define MODE_INST_SELECT   4
#define MODE_CONFIG        5

uint8_t mode=MODE_PATTERN_WRITE;//We start on Pattern write mode

boolean play_pattern=0;//Flag pattern play 1=play ou 0=stop 
//boolean play_song=0;//Flag song play  1=play ou 0=stop 

//-----MIDI-----//            
uint8_t bpm=120;//Default BPM
boolean midi_sync=0;//0 master sync  1 slave sync  Default Master

//Midi message define
#define MIDI_START 0xfa
#define MIDI_STOP  0xfc
#define MIDI_CLOCK 0xf8

//Time in microsecond of the callback fuction
uint16_t timer_time=5000;

uint8_t count_ppqn=-1;//24 pulse par quart note counter
uint8_t count_step=0;//Step counter

uint8_t nbr_step=16;//Default number of step
uint8_t scale=1;//Default scale  1/16
uint8_t scale_value[]={
  3, 6, 8, 12,
  16, 24};

// Licklogic
int valuePot[2][8]    = {0}; //  valeur des potentiomètres stockées dans un array
int oldValuePot[2][8] = {0}; // ancienne valeur des potentiomètres stockées dans un array

byte preBoutonRead = 0;   //flag de la valeur de chaques boutons
byte led           = 0;   //valeur envoyer à chaques platines Dilicktal
byte stateBouton   = 0;   //état de chaques boutons
boolean stateLed[8]= {0}; //état de chaques leds

//--------------------------------------------SETUP----------------------------------------------//
void setup() {
  // Attach callback function to Timer1
  Timer1.attachInterrupt(Count_PPQN);
  
  // Startup screen
  lcd.createChar(0, space_left);//initialize special character
  lcd.createChar(1, space_right);

  printLcd((char*)pgm_read_word(&(title_string[0])), 
    (char*)pgm_read_word(&(title_string[1])));
  delay(1000);
  
  SR.Initialize();
  Pot.Initialize();
}
//--------------------------------------------SETUP----------------------------------------------//

//--------------------------------------------LOOP-----------------------------------------------//
void loop() {
  Timer1.initialize(timer_time);
  
  checkPotValues();
  checkButValues();

}

void checkPotValues() {
  for (int board=0; board<2; board++) {   
    for (int pot=0; pot<=7; pot++) {
      digitalWrite(8,LOW);
      valuePot[board][pot] = Pot.read(board*8+pot);     
      
      //Enregistre la valeur présente sur l'entrée analogique selectionnée
      if (abs(valuePot[board][pot] - oldValuePot[board][pot]) > 1) {
        oldValuePot[board][pot] = (valuePot[board][pot]);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Pot "); 
        lcd.setCursor(4,0);
        lcd.print(board,DEC);
        lcd.setCursor(6,0);
        lcd.print(pot,DEC);
        lcd.setCursor(0,1);
        lcd.print("Value ");
        lcd.setCursor(6,1);
        lcd.print(valuePot[board][pot],DEC);
      }
      digitalWrite(8,HIGH); 
    }
  }
}

void checkButValues() {
  
}

//////////////////////////////////////////////////////////////////
//This function is call by the timer depending Sync mode and BPM//
//////////////////////////////////////////////////////////////////
void Count_PPQN() {
//-----------------Sync SLAVE-------------------//  
  if(midi_sync){
    timer_time=5000;
    if(Serial.available() > 0) {
      byte data = Serial.read();
      if(data == MIDI_START){
        if(mode==MODE_PATTERN_PLAY || mode==MODE_PATTERN_WRITE ) {//|| mode==MODE_INST_SELECT){
          play_pattern = 1;
          count_ppqn=-1;
        }
//        if(mode==MODE_SONG_PLAY || mode==MODE_SONG_WRITE){
//          play_song = 1;
//          count_ppqn=-1;
//          song_position=0;
//        }
      }
      else if(data == MIDI_STOP ) {
        play_pattern = 0;
        //play_song = 0;
        count_step=0;
        count_ppqn=-1;
        //song_position=0;
      }
      else if(data == MIDI_CLOCK && (play_pattern == 1 )) {//|| play_song == 1)) {
        count_ppqn++;
        count_step=count_ppqn/scale_value[scale];
        if(count_ppqn>=(nbr_step*scale_value[scale])-1){
          count_ppqn=-1;      
          //song_position++;
          //if (song_position==16) song_position=0;
        }
//        if (count_ppqn>1) led_flag=0;//Led clignote reste ON 1 count sur 6
//        if (count_ppqn<=1) led_flag=1; 
//        led_flag=!led_flag;  
      }
//      if (data==MIDI_CLOCK && (play_pattern == 0)) {// || play_song==0)){
//        count_led++;
//        if(count_led==12){
//          count_led=0;
//          led_flag=!led_flag;
//        }
//      }
    }
  }
  //-----------------Sync MASTER-------------------//
  if(!midi_sync){
    timer_time=2500000/bpm;
    Serial.write(MIDI_CLOCK);
    if(play_pattern) { //||play_song){   
      count_ppqn++;    
      count_step=count_ppqn/scale_value[scale];   
      if(count_ppqn>=(nbr_step*scale_value[scale])-1){
        count_ppqn=-1;
//        song_position++;
//        if (song_position==16) song_position=0;
      }
//      if (count_ppqn>1) led_flag=0;//Led blink 1 count on 6
//      if (count_ppqn<=1) led_flag=1; 
//      led_flag=!led_flag;
    }
    else if(!play_pattern) {// &&!play_song){
      count_ppqn=-1;
      count_step=0;
//      count_led++;
//      song_position=0;
//      if(count_led==12){
//        count_led=0;
//        led_flag=!led_flag;
//      }
    }
  }
}

void SendToLed (byte data1,byte data2,byte data3,byte data4,byte data5,byte data6,byte data7,byte data8) {

  digitalWrite(latchPinOut, 0);
  delayMicroseconds(20);
  shiftOut(dataPinOut, clockPinOut, data8);  
  shiftOut(dataPinOut, clockPinOut, data7);  
  shiftOut(dataPinOut, clockPinOut, data6);
  shiftOut(dataPinOut, clockPinOut, data5);
  shiftOut(dataPinOut, clockPinOut, data4);  
  shiftOut(dataPinOut, clockPinOut, data3);  
  shiftOut(dataPinOut, clockPinOut, data2);
  shiftOut(dataPinOut, clockPinOut, data1);
  digitalWrite(latchPinOut, 1);
}

void shiftOut(int myDataPin, int myClockPin, byte myDataOut) {

  int i=0;
  int pinState;
  pinMode(myClockPin, OUTPUT);
  pinMode(myDataPin, OUTPUT);

  digitalWrite(myDataPin, 0);
  digitalWrite(myClockPin, 0);

  for (i=7; i>=0; i--)  {
    digitalWrite(myClockPin, 0);
    if ( myDataOut & (1<<i) ) {
      pinState= 1;
    }
    else {	
      pinState= 0;
    }

    digitalWrite(myDataPin, pinState);
    digitalWrite(myClockPin, 1);
    digitalWrite(myDataPin, 0);
  }
}

byte shiftIn(int myDataPin, int myClockPin) { 
  int i;
  int temp = 0;
  int pinState;
  byte myDataIn = 0;

  pinMode(myClockPin, OUTPUT);
  pinMode(myDataPin, INPUT);

  for (i=7; i>=0; i--)
  {
    digitalWrite(myClockPin, 0);
    delayMicroseconds(2);
    temp = digitalRead(myDataPin);
    if (temp) {
      pinState = 1;
      myDataIn = myDataIn | (1 << i);
    }
    else {
      pinState = 0;
    }
    digitalWrite(myClockPin, 1);
  }
  return myDataIn;
}
