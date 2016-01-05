#include <TimerOne.h>
#include <SRIO.h>
#include <LiquidCrystal.h>

  
//Mode define
#define MODE_PLAY   0
#define MODE_BANK   1
#define MODE_CONFIG 2

#define SERIAL_MODE_SERIAL 9600
#define SERIAL_MODE_MIDI   31250
#define SERIAL_MODE_USB    115200

double serial_mode = SERIAL_MODE_MIDI;

uint8_t mode = MODE_PLAY;
boolean play_pattern=0;

#define BANK_A 0
#define BANK_B 1
#define BANK_C 2
#define BANK_D 3

uint8_t bank = BANK_A;

//Initalize Library
LiquidCrystal lcd(2,3,4,5,6,7);

char lcd_buffer[16];
uint8_t lcd_mode = MODE_PLAY;

//-----MIDI-----//            
uint8_t bpm = 120;//Default BPM
unsigned int old_clock_time = 0;
boolean midi_sync = 0; //0 master sync  1 slave sync  Default MasterBPM
uint8_t midi_channel=1;//Default Midi Channel


//Midi message define
#define MIDI_START 0xfa
#define MIDI_STOP  0xfc
#define MIDI_CLOCK 0xf8

//Time in microsecond of the callback fuction
uint16_t timer_time = 5000;

uint8_t count_ppqn[4][4] = {-1};//24 pulse par quart note counter
uint8_t count_step[4][4] = {0};//Step counter
uint8_t nbr_step[4][4] = {0};//Step counter
uint8_t nbr_puls[4][4] = {0};
uint8_t nbr_offs[4][4] = {0};

uint8_t midi_note[4][4] = {{36, 37, 38, 39},{40,41,42,43},{44,45,46,47},{48,49,50,51}};
uint8_t midi_velo[4][4] = {{127,127,127,127}, {127,127,127,127}, {127,127,127,127}, {127,127,127,127}};

unsigned long beat_holder[4][4] = {0};

uint8_t scale[4][4]={1};//Default scale  1/16
uint8_t scale_value[]={
  3, 6, 8, 12,
  16, 24};

// Licklogic
uint8_t valuePot[4][4][4]    = {0}; //  valeur des potentiomètres stockées dans un array / bank

// Buttons et LED
#define BUTTON_RED         7
#define BUTTON_BLACK       6
#define BUTTON_WHITE_LEFT  5
#define BUTTON_WHITE_RIGHT 4
#define BUTTON_BLACK_01    3
#define BUTTON_BLACK_02    2
#define BUTTON_BLACK_03    1
#define BUTTON_BLACK_04    0

#define LED_RED         7
#define LED_BLACK       6
#define LED_WHITE_LEFT  5
#define LED_WHITE_RIGHT 4
#define LED_BLACK_01    3
#define LED_BLACK_02    2
#define LED_BLACK_03    1
#define LED_BLACK_04    0

uint8_t LED_INST[4] = {LED_BLACK_01, LED_BLACK_02, LED_BLACK_03, LED_BLACK_04};

//--------------------------------------------SETUP----------------------------------------------//
void setup() {
  // Attach callback function to Timer1
  Timer1.attachInterrupt(Count_PPQN);

  lcd.begin(16,2);
  lcd.clear();
  lcd.print("   EUCLIDE SEQ  ");
  
  Serial.begin(serial_mode);
  
  // Init Potentiometres
  pinMode(8, OUTPUT);    // clock Pin du compteur CD4520
  pinMode(12, INPUT);    // Pin temoin d'initialisation du compteur binaire CD4520
  //Reset à 0 du compteur binaire CD4520
  do{ 
    digitalWrite(8,LOW); 
    digitalWrite(8,HIGH);
    digitalWrite(12,LOW);
  }
  while (digitalRead(12)==HIGH);
  do{
    digitalWrite(8,HIGH);
    digitalWrite(8,LOW); 
  }
  while (digitalRead (12) ==LOW);
  pinMode(12, OUTPUT);// ceci afin déviter les conflits avec la bibliothèque SRIO
  
  // Init Buttons
  SR.Initialize();
  SR.Led_All_On();
  delay(1000);
  SR.Led_All_Off();
}
//--------------------------------------------SETUP----------------------------------------------//

//--------------------------------------------LOOP-----------------------------------------------//
void loop() {
  Timer1.initialize(timer_time);
  
  checkButValues();
  
  checkPotValues();
  
  updateLeds();
  
  updateLcd();
  
  updateMidi();

}

void checkPotValues() {
  int cols[4] = {1,0,3,2};
  uint8_t current_pot_value, diff;
  for (int ci=0; ci < 4; ci++) {
    int c = cols[ci];
    for (int l=0; l< 4; l++) {
      digitalWrite(8,LOW);
      current_pot_value = (analogRead(c / 2) >> 2);//convert 10bits 0->1024 to 8bits 0->255
      digitalWrite(8,HIGH);
 
      if(mode == MODE_PLAY) {     
        diff = abs(current_pot_value - valuePot[bank][c][l]);
        if (diff > 0 && diff < 16) {
          valuePot[bank][c][l] = current_pot_value;
          switch(l) {
            case 0 :
              midi_note[bank][c] = current_pot_value >> 1; // 0->128
              break;
            case 1 :
              nbr_step[bank][c] = current_pot_value * 32 / 255;
              nbr_puls[bank][c] = valuePot[bank][c][2] * nbr_step[bank][c] / 255;
              nbr_offs[bank][c] = valuePot[bank][c][3] * nbr_puls[bank][c] / 255;
              break;
            case 2 :
              nbr_puls[bank][c] = current_pot_value * nbr_step[bank][c] / 255;
              nbr_offs[bank][c] = valuePot[bank][c][3] * nbr_puls[bank][c] / 255;
              break;
            case 3 :
              nbr_offs[bank][c] = current_pot_value * nbr_step[bank][c] / 255; 
              break;
          }
          
          beat_holder[bank][c] = euclid(nbr_step[bank][c], nbr_puls[bank][c]);
          
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("NOT STP PLS OFF ");
          lcd.setCursor(0,1);
          lcd.print(midi_note[bank][c],DEC);
          lcd.setCursor(4,1);
          lcd.print(nbr_step[bank][c],DEC);
          lcd.setCursor(8,1);
          lcd.print(nbr_puls[bank][c],DEC);
          lcd.setCursor(12,1);
          lcd.print(beat_holder[bank][c],DEC);
        }
      }
    }
  }
}

void checkButValues() {
  if(mode == MODE_PLAY) {
    if(SR.Button_Pin_Read(BUTTON_RED)) {
      play_pattern = !play_pattern;
      if(play_pattern) {
        Serial.write(MIDI_START);
      } else {
        Serial.write(MIDI_STOP);
      }
       
      lcd.clear();
      delay(200);
    }
  }
    
  if(mode == MODE_BANK) {
    if(SR.Button_Pin_Read(BUTTON_BLACK_01)) {
      bank = BANK_A;
      delay(200);
    }
    if(SR.Button_Pin_Read(BUTTON_BLACK_02)) {
      bank = BANK_B;
      delay(200);
    }
    if(SR.Button_Pin_Read(BUTTON_BLACK_03)) {
      bank = BANK_C;
      delay(200);
    }
    if(SR.Button_Pin_Read(BUTTON_BLACK_04)) {
      bank = BANK_D;
      delay(200);
    }
  }
  
  if(mode == MODE_CONFIG) {
    if(SR.Button_Pin_Read(BUTTON_BLACK_01)) {
      midi_sync = !midi_sync;
      delay(200);
    }
    if(SR.Button_Pin_Read(BUTTON_BLACK_02)) {
      if(serial_mode == SERIAL_MODE_USB)
        serial_mode = SERIAL_MODE_MIDI;
      else if(serial_mode == SERIAL_MODE_MIDI)
        serial_mode = SERIAL_MODE_SERIAL;
      else 
        serial_mode = SERIAL_MODE_USB;
      
      Serial.begin(serial_mode);
      delay(200);
    }
    if(SR.Button_Pin_Read(BUTTON_BLACK_03)) {
      delay(200);
    }
    
    if(SR.Button_Pin_Read(BUTTON_BLACK_04)) {
      if(SR.Button_Pin_Read(BUTTON_WHITE_LEFT)) {
        if(!midi_sync) {
          bpm--;
        }
        delay(200);
        return;
      }
      if(SR.Button_Pin_Read(BUTTON_WHITE_RIGHT)) {
        if(!midi_sync) {
          bpm++;
        }
        delay(200);
        return;
      }
    }
  }
  
  // Navigation
  if(SR.Button_Pin_Read(BUTTON_WHITE_LEFT)) {
    if(mode == 0)
      mode = 2;
    else
      mode = (mode - 1) % 3;
    delay(200);
  }  
  if(SR.Button_Pin_Read(BUTTON_WHITE_RIGHT)) {
    mode = (mode + 1) % 3;
    delay(200);
  }

}

void updateLeds() {
  if(mode == MODE_PLAY) {
    if(play_pattern) {
      SR.Led_Pin_Write(LED_RED, 1);
    } else {
      SR.Led_Pin_Write(LED_RED, 0); 
    }
    
    for(int inst = 0; inst < 4; inst++) {
      if (bitRead (beat_holder[bank][inst], count_step[bank][inst])) {   
        SR.Led_Pin_Write(LED_INST[inst], 1);
      } else {
        SR.Led_Pin_Write(LED_INST[inst], 0);
      }
    }
  }
  
  if(mode == MODE_BANK) {
    SR.Led_All_Off();
    if(bank == BANK_A) {
      SR.Led_Pin_Write(LED_BLACK_01, 1);
    }
    if(bank == BANK_B) {
      SR.Led_Pin_Write(LED_BLACK_02, 1);
    }
    if(bank == BANK_C) {
      SR.Led_Pin_Write(LED_BLACK_03, 1);
    }
    if(bank == BANK_D) {
      SR.Led_Pin_Write(LED_BLACK_04, 1);
    }
  }
  
  if(mode == MODE_CONFIG) {
    SR.Led_All_Off();
  }
}

void updateLcd() {
  if(mode == MODE_PLAY) {
    if(lcd_mode != mode) {
      lcd_mode = MODE_PLAY;
      lcd.clear();
      
      lcd.setCursor(0,0);
      lcd.print("PLAY");
      lcd.setCursor(0,1);
      if(play_pattern) {
        lcd.print("ON  ");
      } else {
        lcd.print("OFF ");
      }
    } 
  }
  
  if(mode == MODE_BANK) {
    if(lcd_mode != mode) {
      lcd_mode = MODE_BANK;
      lcd.clear();
    }
    lcd.setCursor(0,0);
    lcd.print("BANK");
    lcd.setCursor(0,1);
    lcd.print(bank);
  }
  
  if(mode == MODE_CONFIG) {
    if(lcd_mode != mode) {
      lcd_mode = MODE_CONFIG;
      lcd.clear();
    }
    lcd.setCursor(0,0);
    lcd.print("SYNC");
    lcd.setCursor(0,1);
    if(midi_sync)
      lcd.print("YES ");
    else
      lcd.print("NO  ");
      
    lcd.setCursor(4,0);
    lcd.print("SER");
    lcd.setCursor(4,1);
    if(serial_mode == SERIAL_MODE_USB)
      lcd.print("USB ");
    else if(serial_mode == SERIAL_MODE_MIDI)
      lcd.print("MID");
    else
      lcd.print("SER");
      
    lcd.setCursor(12,0);
    lcd.print("BPM");
    lcd.setCursor(12,1);
    lcd.print(bpm, DEC);
  }
  
}
  
void updateMidi() {
  //
  
  for(int inst = 0; inst < 4; inst++) {
    if (bitRead (beat_holder[bank][inst], count_step[bank][inst])) {  
      Send_NoteON(midi_note[bank][inst], midi_velo[bank][inst]);
    } else {
      Send_NoteOFF(midi_note[bank][inst]);
    }
  }
  
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
      if(data == MIDI_START) {
          play_pattern = 1;
          for(int i = 0; i < 4; i++) {
            for(int j = 0; j < 4; j++) {
              count_ppqn[i][j] = -1;
            }
          }
        
      }
      else if(data == MIDI_STOP ) {
        play_pattern = 0;
        for(int i = 0; i < 4; i++) {
          for(int j = 0; j < 4; j++) {
            count_step[i][j] = 0;
            count_ppqn[i][j] = -1;
          }
        }
      }
      else if(data == MIDI_CLOCK && (play_pattern == 1 )) {
        int time = millis();
        lcd.clear();
        if(time > old_clock_time) {
          bpm = (1000 / (time - old_clock_time) / 24) * 60;
          old_clock_time = time;
        }
        for(int i = 0; i < 4; i++) {
          for(int j = 0; j < 4; j++) {
            count_ppqn[i][j]++;
            count_step[i][j] = count_ppqn[i][j] / scale_value[scale[i][j]];
            if(count_ppqn[i][j] >= (nbr_step[i][j] * scale_value[scale[i][j]]) -1){
              count_ppqn[i][j] = -1;
            }
          }
        }
      }
    }
  }
  
  //-----------------Sync MASTER-------------------//
  if(!midi_sync){
    timer_time=2500000/bpm;
    Serial.write(MIDI_CLOCK);
    if(play_pattern) {   
      for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
          count_ppqn[i][j]++;    
          count_step[i][j] = count_ppqn[i][j] / scale_value[scale[i][j]];   
          if(count_ppqn[i][j] >= (nbr_step[i][j] * scale_value[scale[i][j]])-1){
            count_ppqn[i][j] = -1;
          }
        }
      }
    }
    else if(!play_pattern) {
      for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
          count_ppqn[i][j] = -1;
          count_step[i][j] = 0;
        }
      }
    }
  }
}

// Euclid calculation function 
byte[] euclid(int steps, int pulses){ // inputs: n=total, k=beats, o = offset
  if(pulses == 0) {
    return 0;
  }
  
  byte workbeat[steps];
  
  int a; 
  for (a = 0; a < steps; a++) { // Populate workbeat with unsorted pulses and pauses 
    if (a<pulses)
      workbeat[a] = 1;
    else 
      workbeat[a] = 0;
  }
  
  int pauses = steps - pulses;
  int per_pulse = pauses / pulses;
  int remainder = pauses % pulses;  
  unsigned int outbeat;
  unsigned int working;
  int workbeat_count=steps;
  int b; 
  int trim_count;

  if (per_pulse>0 && remainder < 2) { // Handle easy cases where there is no or only one remainer  
    for (a=0;a<pulses;a++){
      for (b=workbeat_count-1; b>workbeat_count-per_pulse-1;b--){
        workbeat[a]  = ConcatBin (workbeat[a], workbeat[b]);
      }
      workbeat_count = workbeat_count-per_pulse;
    }

    outbeat = 0; // Concatenate workbeat into outbeat - according to workbeat_count 
    for (a=0;a < workbeat_count;a++){
      outbeat = ConcatBin(outbeat,workbeat[a]);
    }
    return outbeat;
  }

  else { 

    int groupa = pulses;
    int groupb = pauses; 
    
    while(groupb>1){ //main recursive loop
      if (groupa>groupb){ // more Group A than Group B
        int a_remainder = groupa-groupb; // what will be left of groupa once groupB is interleaved 
        trim_count = 0;
        for (a=0; a<groupa-a_remainder;a++){ //count through the matching sets of A, ignoring remaindered
          workbeat[a]  = ConcatBin (workbeat[a], workbeat[workbeat_count-1-a]);
          trim_count++;
        }
        workbeat_count = workbeat_count-trim_count;

        groupa=groupb;
        groupb=a_remainder;
      }

      else if (groupb>groupa){ // More Group B than Group A
        int b_remainder = groupb-groupa; // what will be left of group once group A is interleaved 
        trim_count=0;
        for (a = workbeat_count-1;a>=groupa+b_remainder;a--){ //count from right back through the Bs
          workbeat[workbeat_count-a-1] = ConcatBin (workbeat[workbeat_count-a-1], workbeat[a]);

          trim_count++;
        }
        workbeat_count = workbeat_count-trim_count;
        groupb=b_remainder;
      }
      
      else if (groupa == groupb){ // groupa = groupb 
        trim_count=0;
        for (a=0;a<groupa;a++){
          workbeat[a] = ConcatBin (workbeat[a],workbeat[workbeat_count-1-a]);
          trim_count++;
        }
        workbeat_count = workbeat_count-trim_count;
        groupb=0;
      }

      else {
        //        Serial.println("ERROR");
      }
    }


    outbeat = 0; // Concatenate workbeat into outbeat - according to workbeat_count 
    for (a=0;a < workbeat_count;a++){
      outbeat = ConcatBin(outbeat,workbeat[a]);
    }
    return outbeat;

  }
}

// Function to find the binary length of a number by counting bitwise 
int findlength(unsigned int bnry){
  boolean lengthfound = false;
  int length=1; // no number can have a length of zero - single 0 has a length of one, but no 1s for the sytem to count
  for (int q=32;q>=0;q--){
    int r=bitRead(bnry,q);
    if(r==1 && lengthfound == false){
      length=q+1;
      lengthfound = true;
    }
  }
  return length;
}

// Function to concatenate two binary numbers bitwise 
unsigned int ConcatBin(unsigned int bina, unsigned int binb){
  int binb_len=findlength(binb);
  unsigned int sum=(bina<<binb_len);
  sum = sum | binb;
  return sum;
}



void Send_NoteON(byte data, uint8_t velo){
  Serial.write (144+midi_channel-1);//Note ON on selected channel 
  Serial.write (data);
  Serial.write (velo);
}

void Send_NoteOFF(byte data){
  Serial.write (128+midi_channel-1);//Note OFF on selected channel
  Serial.write (data);
  Serial.write (0x01);//velocite 0
}
