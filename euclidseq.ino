#include <TimerOne.h>
#include <SRIO.h>
#include <LiquidCrystal.h>

//Mode define
#define MODE_PLAY   0
#define MODE_CONFIG_1 1

#define NB_MODE     2
uint8_t mode = MODE_PLAY;

boolean play_pattern=0;

#define SERIAL_MODE_SERIAL 9600
#define SERIAL_MODE_MIDI   31250
#define SERIAL_MODE_USB    115200
uint32_t serial_mode = SERIAL_MODE_MIDI;

#define NB_BANK 4
uint8_t bank = 0;

#define RYTHM_MODE_POLY 1
#define RYTHM_MODE_SOLO 2
uint8_t rythm_mode[NB_BANK][4];

LiquidCrystal lcd(2,3,4,5,6,7);

uint8_t lcd_mode = MODE_PLAY;

//-----MIDI-----//            
uint8_t bpm = 120;//Default BPM
unsigned int old_clock_time = 0;
boolean midi_sync = 0; //0 master sync  1 slave sync  Default MasterBPM
uint8_t midi_channel=3;//Default Midi Channel

//Midi message define
#define MIDI_START 0xfa
#define MIDI_STOP  0xfc
#define MIDI_CLOCK 0xf8

//Time in microsecond of the callback fuction
uint16_t timer_time = 5000;

uint8_t count_ppqn[NB_BANK][4];
uint8_t count_step[NB_BANK][4];
uint8_t track_step[NB_BANK][4];
uint8_t track_puls[NB_BANK][4];
int8_t  track_offs[NB_BANK][4];

boolean noteOn[NB_BANK][4];

uint8_t midi_note[NB_BANK][4] = {{36, 37, 38, 39}, {40,41,42,43}, {44,45,46,47}, {48,49,50,51}};
uint8_t midi_velo[NB_BANK][4];

uint32_t beat_holder[NB_BANK][4];

uint8_t scale[NB_BANK][4]={16};//Default scale  1/16

// Licklogic
uint8_t valuePot[NB_BANK][4][4] = {0}; 

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
  lcd.print("  EUCLIDE SEQ   ");
  
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
  delay(100);
  
  SR.Led_All_On();
  // Init default value
  for(int i = 0; i < 4; i++) {
    for(int j = 1; j < 4; j++) {
      rythm_mode[i][j] = RYTHM_MODE_POLY;
      count_ppqn[i][j] = -1;
      count_step[i][j] = 0;
      track_step  [i][j] = 0;
      track_puls  [i][j] = 0;
      track_offs  [i][j] = 0;
      midi_velo [i][j] = 127;
      scale     [i][j] = 16;
      noteOn    [i][j] = false;
    }
  }
  delay(100);
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
              track_step[bank][c] = current_pot_value * 32 / 255;
              
              if(rythm_mode[bank][c] == RYTHM_MODE_SOLO)
                scale[bank][c] = (96 / track_step[bank][c]) * 2; // 96 = 24 ppqn x 4 noires = nb ppqm / bar
              else
                scale[bank][c] = 6;
              
              track_puls[bank][c] = valuePot[bank][c][2] * track_step[bank][c] / 255;
              track_offs[bank][c] = valuePot[bank][c][3] * track_puls[bank][c] / 255;
              break;
            case 2 :
              track_puls[bank][c] = current_pot_value * track_step[bank][c] / 255;
              track_offs[bank][c] = valuePot[bank][c][3] * track_puls[bank][c] / 255;
              break;
            case 3 :
              track_offs[bank][c] = current_pot_value * track_step[bank][c] / 255 - track_step[bank][c] / 2; 
              break;
          }
          
          beat_holder[bank][c] = euclid(track_step[bank][c], track_puls[bank][c], track_offs[bank][c]);
          
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("NOT STP PLS OFF ");
          lcd.setCursor(0,1);
          lcd.print(midi_note[bank][c],DEC);
          lcd.setCursor(4,1);
          lcd.print(track_step[bank][c],DEC);
          lcd.setCursor(8,1);
          lcd.print(track_puls[bank][c],DEC);
          lcd.setCursor(12,1);
          lcd.print((int) track_offs[bank][c],DEC);
        }
      }
    }
  }
}

void checkButValues() {
  if(SR.Button_Pin_Read(BUTTON_RED)) {
    play_pattern = !play_pattern;
    if(play_pattern) {
      Serial.write(MIDI_START);
    } else {
      Serial.write(MIDI_STOP);
    }
    delay(200);
    return;
  }
 
  if(mode == MODE_CONFIG_1) {
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
    
    if(SR.Button_Pin_Read(BUTTON_BLACK_03)) {
      if(SR.Button_Pin_Read(BUTTON_WHITE_LEFT)) {
        bank--;
        bank = (bank + NB_BANK) % NB_BANK;
        delay(200);
        return;
      }
      if(SR.Button_Pin_Read(BUTTON_WHITE_RIGHT)) {
        bank++;
        bank = bank % NB_BANK;
        delay(200);
        return;
      }
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
    mode--;
    mode = (mode + NB_MODE) % NB_MODE;
    delay(200);
    return;
  }
  
  if(SR.Button_Pin_Read(BUTTON_WHITE_RIGHT)) {
    mode++;
    mode = mode % NB_MODE;
    delay(200);
    return;
  }

}

void updateLeds() {  
  if(play_pattern) {
    SR.Led_Pin_Write(LED_RED, 1);
  } else {
     SR.Led_Pin_Write(LED_RED, 0); 
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
  
  if(mode == MODE_CONFIG_1) {
    if(lcd_mode != mode) {
      lcd_mode = MODE_CONFIG_1;
      lcd.clear();
    }
    lcd.setCursor(0,0);
    lcd.print("SYC BUS BNK BPM ");
    lcd.setCursor(0,1);
    if(midi_sync)
      lcd.print("YES ");
    else
      lcd.print("NO  ");
    lcd.setCursor(4,1);
    if(serial_mode == SERIAL_MODE_USB)
      lcd.print("USB ");
    else if(serial_mode == SERIAL_MODE_MIDI)
      lcd.print("MID ");
    else
      lcd.print("SER ");
    
    lcd.setCursor(8,1);
    lcd.print(bank, DEC);
    
    lcd.setCursor(12,1);
    lcd.print(bpm, DEC);
  }
  
}
  
void updateMidi() {
  for(int inst = 0; inst < 4; inst++) {
    if (bitRead (beat_holder[bank][inst], count_step[bank][inst])) {
      if(!noteOn[bank][inst]) {
        Send_NoteON(midi_note[bank][inst], midi_velo[bank][inst]);  
        SR.Led_Pin_Write(LED_INST[inst], 1);
        noteOn[bank][inst] = true;
      } else {
        //Send_NoteCONT(midi_note[bank][inst]);
      }
    } else {
      if(noteOn[bank][inst]) {
        Send_NoteOFF(midi_note[bank][inst]);  
        SR.Led_Pin_Write(LED_INST[inst], 0);
        noteOn[bank][inst] = false;
      }
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
          for(int i = 0; i < NB_BANK; i++) {
            for(int j = 0; j < 4; j++) {
              count_ppqn[i][j] = -1;
            }
          }
        
      }
      else if(data == MIDI_STOP ) {
        play_pattern = 0;
        for(int i = 0; i < NB_BANK; i++) {
          for(int j = 0; j < 4; j++) {
            count_step[i][j] = 0;
            count_ppqn[i][j] = -1;
          }
        }
      }
      else if(data == MIDI_CLOCK && play_pattern) {
        int time = millis();
        bpm = (time - old_clock_time) / 24 * 60000;
        old_clock_time = time;
        for(int i = 0; i < NB_BANK; i++) {
          for(int j = 0; j < 4; j++) {
            count_ppqn[i][j]++;    
            count_step[i][j] = count_ppqn[i][j] / scale[i][j];   
            if(count_ppqn[i][j] >= (track_step[i][j] * scale[i][j]) - 1) {
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
    if(!play_pattern) {
      for(int i = 0; i < NB_BANK; i++) {
        for(int j = 0; j < 4; j++) {
          count_ppqn[i][j] = -1;
          count_step[i][j] = 0;
        }
      }
    } else {   
      for(int i = 0; i < NB_BANK; i++) {
        for(int j = 0; j < 4; j++) {
          count_ppqn[i][j]++;    
          count_step[i][j] = count_ppqn[i][j] / scale[i][j];   
          if(count_ppqn[i][j] >= (track_step[i][j] * scale[i][j]) - 1) {
            count_ppqn[i][j] = -1;
          }
        }
      }
    }
  }
}

// Euclid calculation function
uint32_t bits;
uint8_t pos;
int8_t remainder[32];
int8_t count[32];
  
uint32_t euclid(int8_t slots, int8_t pulses, int8_t offset) {
    bits = 0UL;
    pos = 0;
    if(!pulses)
      return bits;
    
    int8_t divisor = slots - pulses;
    remainder[0] = pulses; 
    int8_t level = 0; 
    int8_t cycleLength = 1; 
    int8_t remLength = 1;
    do { 
      count[level] = divisor / remainder[level]; 
      remainder[level+1] = divisor % remainder[level]; 
      divisor = remainder[level]; 
      int8_t newLength = (cycleLength * count[level]) + remLength; 
      remLength = cycleLength; 
      cycleLength = newLength; 
      level = level + 1;
    }while(remainder[level] > 1);
    
    count[level] = divisor; 
    if(remainder[level] > 0)
      cycleLength = (cycleLength * count[level]) + remLength;
    build(level);
    
    // Apply offset
    if(offset < 0) {
       return (bits << offset) | (bits >> (sizeof(bits) + offset));
    } else if(offset > 0) {
       return (bits >> offset) | (bits << (sizeof(bits) - offset));
    }
    
    return bits;
  }

  void build(int8_t level){
    if(level == -1){
      bits &= ~(1UL<<pos++);
    }else if(level == -2){
      bits |= 1UL<<pos++;
    }else{ 
      for(int8_t i=0; i < count[level]; i++)
	build(level-1); 
      if(remainder[level] != 0)
	build(level-2); 
    }
  }

void Send_NoteON(byte data, uint8_t velo) {
  Serial.write (144+midi_channel-1);//Note ON on selected channel 
  Serial.write (data);
  Serial.write (velo);
}

void Send_NoteOFF(byte data){
  Serial.write (128+midi_channel-1);//Note OFF on selected channel
  Serial.write (data);
  Serial.write (0x01);//velocite 0
}
