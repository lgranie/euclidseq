#include <TimerOne.h>
#include <SRIO.h>
#include <LiquidCrystal.h>

//Mode define
#define MODE_PLAY     0
#define MODE_CONFIG_1 1
#define MODE_CONFIG_2 2

#define NB_MODE     3
uint8_t mode = MODE_PLAY;

boolean play_pattern=0;

#define SERIAL_MODE_SERIAL 9600
#define SERIAL_MODE_MIDI   31250
#define SERIAL_MODE_USB    115200
uint32_t serial_mode = SERIAL_MODE_MIDI;

#define NB_BANK 4
uint8_t bank = 0;
uint8_t inst = 0;

#define RYTHM_MODE_POLY 1
#define RYTHM_MODE_SOLO 2
uint8_t rythm_mode[NB_BANK][4];

LiquidCrystal lcd(2,3,4,5,6,7);

#define LCD_MODE_PLAY_00 0
#define LCD_MODE_PLAY_01 1
#define LCD_MODE_PLAY_02 2
#define LCD_MODE_PLAY_03 3
#define LCD_MODE_PLAY_04 4

uint8_t lcd_mode = MODE_PLAY;
uint8_t lcd_mode_play = LCD_MODE_PLAY_00;
uint8_t old_lcd_mode_play = LCD_MODE_PLAY_00;

boolean bitmap_mode = false;
boolean pot_change  = true;

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

uint8_t count_ppqn[NB_BANK][4];
uint8_t count_step[NB_BANK][4];
uint8_t track_step[NB_BANK][4];
uint8_t track_last_step[NB_BANK][4];
uint8_t track_puls[NB_BANK][4];
int8_t  track_offs[NB_BANK][4];
float   track_swing[NB_BANK][4];

boolean noteOn[NB_BANK][4];

uint8_t midi_note[NB_BANK][4] = {{36, 37, 38, 39}, {40,41,42,43}, {44,45,46,47}, {48,49,50,51}};
uint8_t midi_velo[NB_BANK][4];

boolean ***beat_holder;

uint8_t track_scale[NB_BANK][4];

// Licklogic
uint8_t valuePot[NB_BANK][4][4]; 

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
  beat_holder = (boolean***) malloc(NB_BANK * sizeof * beat_holder);
  for(int i = 0; i < 4; i++) {
    beat_holder[i] = (boolean**) malloc(4 * sizeof * beat_holder[i]);
    
    for(int j = 0; j < 4; j++) {
      rythm_mode [i][j] = RYTHM_MODE_POLY;
      count_ppqn [i][j] = -1;
      count_step [i][j] = 0;
      track_step [i][j] = 0;
      track_puls [i][j] = 0;
      track_offs [i][j] = 0;
      track_scale[i][j] = 6;
      track_swing[i][j] = 1.0f;
      track_last_step[i][j] = 0;
      midi_velo  [i][j] = 100;
      noteOn     [i][j] = false;
      valuePot   [i][j][0] = 0;
      valuePot   [i][j][1] = 0;
      valuePot   [i][j][2] = 0;
      valuePot   [i][j][3] = 0;
    }
  }
  delay(500);
  SR.Led_All_Off();
}
//--------------------------------------------SETUP----------------------------------------------//

//--------------------------------------------LOOP-----------------------------------------------//
void loop() {
  Timer1.initialize(timer_time);
  
  checkButValues();
  
  checkPotValues();
  
  updateMidi();
  
  updateLeds();
  
  updateLcd();

}

void checkPotValues() {
  float valuef, d;
  int cols[4] = {1,0,3,2};
  uint8_t current_pot_value, diff;
  for (int ci=0; ci < 4; ci++) {
    int c = cols[ci];
    int value = 0;
    for (int l=0; l< 4; l++) {
      digitalWrite(8,LOW);
      uint32_t data = (analogRead(c / 2) >> 2);//convert 10bits 0->1024 to 8bits 0->255
      current_pot_value = map(data,6,253,0,255);// map to right midi value 0,127 due to the bad ADC and bad power stability
      current_pot_value = max(current_pot_value,0);  
      current_pot_value = constrain(current_pot_value,0,255);
      digitalWrite(8,HIGH);
 
      if(mode == MODE_PLAY) {
        if(SR.Button_Pin_Read(BUTTON_BLACK_01)) {
          lcd_mode_play = LCD_MODE_PLAY_01;
          bitmap_mode = false;
          switch(l) {
            case 0 :
              break;
            case 1 :
              current_pot_value = current_pot_value >> 1; // 0->128
              diff = abs(current_pot_value - midi_note[bank][c]);
              if(diff > 0 && diff < 2) {
                inst = c;
                midi_note[bank][inst] = current_pot_value;
                pot_change = true;          
              }
              break;
            case 2 :
              valuef = ((float) current_pot_value / 255) + 1.f;  // 1 <= x <= 2
              d = abs(valuef - track_swing[bank][c]);
              if(d > 0.0f && d < 0.02f) {
                inst = c;
                track_swing[bank][inst] = valuef;
                pot_change = true;
              }
              break;
            case 3 :
              current_pot_value = (current_pot_value / 128) + 1; // 0->1
              if (rythm_mode[bank][c] != current_pot_value) {
                inst = c;
                rythm_mode[bank][c] = current_pot_value;
                pot_change = true;
              }
              break;
          }
        } else {
          lcd_mode_play = LCD_MODE_PLAY_00;
          diff = abs(current_pot_value - valuePot[bank][c][l]);
          if (diff > 0 && diff < 16) {
            inst = c;
            valuePot[bank][inst][l] = current_pot_value;
            
            switch(l) {
              case 0 :
                continue;
              case 1 :
                value = current_pot_value * 32 / 255;
                if(track_step[bank][inst] == value) {
                  continue;
                } else {
                  track_step[bank][inst] = value;
                  pot_change = true;
                }
                
                if(rythm_mode[bank][inst] == RYTHM_MODE_SOLO && track_step[bank][inst] != 0) {
                  track_scale[bank][inst] = (96 / track_step[bank][inst]) * 2; // 96 = 24 ppqn x 4 noires = nb ppqm / bar
                } else
                  track_scale[bank][inst] = 6;
                
                track_puls[bank][inst] = valuePot[bank][inst][2] * track_step[bank][inst] / 255;
                track_offs[bank][inst] = valuePot[bank][inst][3] * track_step[bank][inst] / 255;
                break;
              case 2 :
                value = current_pot_value * track_step[bank][inst] / 255;
                if(track_puls[bank][inst] == value) {
                  continue;
                } else {
                  track_puls[bank][inst] = value;
                  pot_change = true;
                }
              
                track_offs[bank][inst] = valuePot[bank][inst][3] * track_step[bank][inst] / 255;
                break;
              case 3 :
                value = current_pot_value * track_step[bank][inst] / 255;
                if(track_offs[bank][inst] == value) {
                  continue;
                } else {
                  track_offs[bank][inst] = value;
                  pot_change = true;
                }
                
                track_offs[bank][inst] = current_pot_value * track_step[bank][inst] / 255; 
                break;
            }
            
            euclid(bank, inst);
          }
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
 
  if(mode == MODE_PLAY) {
    if(SR.Button_Pin_Read(BUTTON_BLACK)) {
      bitmap_mode = !bitmap_mode;
      pot_change = true;
      delay(200);
    }
  }
 
  if(mode == MODE_CONFIG_1) {
    if(SR.Button_Pin_Read(BUTTON_BLACK_01)) {
      midi_sync = !midi_sync;
      delay(200);
      return;
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
      return;
      
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
        if(midi_channel > 0)
          midi_channel--;
      }
      if(SR.Button_Pin_Read(BUTTON_WHITE_RIGHT)) {
        midi_channel++;
      }
      delay(200);
      return;
    }
  }
  
  if(mode == MODE_CONFIG_2) {
    if(SR.Button_Pin_Read(BUTTON_BLACK_04)) {
      if(!midi_sync) {
        if(SR.Button_Pin_Read(BUTTON_WHITE_LEFT)) {
          if(!midi_sync) {
            bpm--;
          }
          delay(100);
          return;
        }
        if(SR.Button_Pin_Read(BUTTON_WHITE_RIGHT)) {
          if(!midi_sync) {
            bpm++;
          }
          delay(100);
          return;
        }
      }
      return;
    }
  }
  
  // Navigation
  if(SR.Button_Pin_Read(BUTTON_WHITE_LEFT)) {
    mode--;
    mode = (mode + NB_MODE) % NB_MODE;
    // Refresh play screen
    pot_change = mode == MODE_PLAY;
    delay(200);
  }
  
  if(SR.Button_Pin_Read(BUTTON_WHITE_RIGHT)) {
    mode++;
    mode = mode % NB_MODE;
    // Refresh play screen
    pot_change = mode == MODE_PLAY;
    delay(200);
  }
  
}

void updateLeds() {  
  if(play_pattern) {
    SR.Led_Pin_Write(LED_RED, 1);
  } else {
     SR.Led_Pin_Write(LED_RED, 0); 
  } 
  if(bitmap_mode) {
    SR.Led_Pin_Write(LED_BLACK, 1);
  } else {
     SR.Led_Pin_Write(LED_BLACK, 0); 
  }
}

void updateLcd() {
  if(mode == MODE_PLAY) {
    if(lcd_mode != mode) {
      lcd_mode = MODE_PLAY;
      lcd.clear();
    }
    
    if(bitmap_mode) {
      lcd.clear();
      lcd.setCursor(0,1);
      for(int i = 0; i <  track_step[bank][inst]; i++) {
        lcd.setCursor(i % 16, i / 16);
        if(i == count_step[bank][inst]) {
          lcd.print((char) 255);
        } else if(beat_holder[bank][inst][i]) {
          lcd.print((char) 124);
        } else {
          lcd.print((char) 95);
        }
      }
      delay(100);
    } else {
      if(pot_change || old_lcd_mode_play != lcd_mode_play) {
        lcd.clear();
        pot_change = false;
        old_lcd_mode_play = lcd_mode_play;
      }
      if(lcd_mode_play == LCD_MODE_PLAY_01) {
        lcd.setCursor(0,0);
        lcd.print("NOT SWG RYT");
        lcd.setCursor(0, 1);
        lcd.print(midi_note[bank][inst],DEC);
        lcd.setCursor(4,1);
        lcd.print(track_swing[bank][inst]);
        lcd.setCursor(8,1);
        if(rythm_mode[bank][inst] == RYTHM_MODE_POLY) 
          lcd.print("POL ");
        else
          lcd.print("SOL ");
      } else {
        lcd.setCursor(0,0);
        lcd.print("B   STP PLS OFF ");
        lcd.setCursor(2,0);
        lcd.print(bank,DEC);
        lcd.setCursor(0,1);
        lcd.print("I   ");
        lcd.setCursor(2,1);
        lcd.print(inst,DEC);
        lcd.setCursor(4,1);
        lcd.print(track_step[bank][inst],DEC);
        lcd.setCursor(8,1);
        lcd.print(track_puls[bank][inst],DEC);
        lcd.setCursor(12,1);
        lcd.print((int) track_offs[bank][inst],DEC);
      }
    }
  }
  
  if(mode == MODE_CONFIG_1) {
    if(lcd_mode != mode) {
      lcd_mode = MODE_CONFIG_1;
      lcd.clear();
    }
    lcd.setCursor(0,0);
    lcd.print("SYC BUS BNK CHN ");
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
    lcd.print(midi_channel, DEC);
    
    return;
  }
  
  if(mode == MODE_CONFIG_2) {
    if(lcd_mode != mode) {
      lcd_mode = MODE_CONFIG_2;
      lcd.clear();
    }
    lcd.setCursor(0,0);
    lcd.print("             BPM ");
    lcd.setCursor(12,1);
    lcd.print(bpm, DEC);
    
    return;
  }
  
}
  
void updateMidi() {
  if(play_pattern) { 
    for(int inst = 0; inst < 4; inst++) {
      if(track_puls[bank][inst] > 0) {
        if((track_last_step[bank][inst] != count_step[bank][inst])) {
            Send_NoteOFF(midi_note[bank][inst]);  
            SR.Led_Pin_Write(LED_INST[inst], 0);
            noteOn[bank][inst] = false;
            track_last_step[bank][inst] = count_step[bank][inst];
        }
        
        if (beat_holder[bank][inst][count_step[bank][inst]]) {
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
  }
}

//////////////////////////////////////////////////////////////////
//This function is call by the timer depending Sync mode and BPM//
//////////////////////////////////////////////////////////////////
void Count_PPQN() {
//-----------------Sync SLAVE-------------------//  
  if(midi_sync) {
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
            int current_track_step = 0;
            // Apply swing
            if( count_ppqn[i][j] <= track_scale[i][j] )
              current_track_step = track_swing[i][j] * (count_ppqn[i][j] / track_scale[i][j]); //stretch
            else
              current_track_step = (2-track_swing[i][j]) * (count_ppqn[i][j] / track_scale[i][j])
                                 + (track_scale[i][j])   * (track_swing[i][j]-1); //compress 
              
            count_step[i][j] = current_track_step;   
            if(count_ppqn[i][j] >= (track_step[i][j] * track_scale[i][j]) - 1) {
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
          int current_track_step = 0;
          // Apply swing
          if( count_ppqn[i][j] <= track_scale[i][j] )
            current_track_step = track_swing[i][j] * (count_ppqn[i][j] / track_scale[i][j]); //stretch
          else
            current_track_step = (2-track_swing[i][j]) * (count_ppqn[i][j] / track_scale[i][j])
                               + (track_scale[i][j])   * (track_swing[i][j]-1); //compress 
              
          count_step[i][j] = current_track_step;   
          if(count_ppqn[i][j] >= (track_step[i][j] * track_scale[i][j]) - 1) {
            count_ppqn[i][j] = -1;
          }
        }
      }
    }
  }
}

// Euclid calculation function
uint8_t pos;
int8_t remainder[32];
int8_t count[32];
  
void euclid(int8_t bank, int8_t inst) {
    // Init beat_holder
    free(beat_holder[bank][inst]);
    if(track_step[bank][inst] > 0) {
      beat_holder[bank][inst] = (boolean *) malloc(track_step[bank][inst]);
      for(int i = 0; i < track_step[bank][inst]; i++) {
        beat_holder[bank][inst][i] = false;
      }
    } else  
      beat_holder[bank][inst] = 0UL;
    
    if(track_puls[bank][inst] == 0) {
      return;
    }
    
    pos = 0;
    
    int8_t divisor = track_step[bank][inst] - track_puls[bank][inst];
    remainder[0] = track_puls[bank][inst]; 
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
    build(level, bank, inst);
  }

  void build(int8_t level, int8_t bank, int8_t inst) {
    int newPos = pos++;
    // Apply offset
    if(track_offs[bank][inst] != 0)  {
      newPos += track_offs[bank][inst];
      newPos = newPos % track_step[bank][inst];
    }
    
    if(level == -1) {
      beat_holder[bank][inst][newPos] = false;
    }else if(level == -2){
      beat_holder[bank][inst][newPos] = true;
    }else{ 
      pos--;
      for(int8_t i=0; i < count[level]; i++)
	build(level-1, bank, inst); 
      if(remainder[level] != 0)
	build(level-2, bank, inst); 
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
