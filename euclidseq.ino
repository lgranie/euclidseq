/*
* Author:       Laurent Granié
 * Copyright:    (C) 2014 Laurent Granié
 * Licence:      GNU General Public Licence version 3
 */
#include <TimerOne.h>
#include <SRIO.h>
#include <LiquidCrystal.h>

#define NB_BANK 4
#define NB_INST 4
byte bank = 0;
byte inst = 0;
boolean inst_changed = false;

#define NB_PARM 4

//Mode define
#define MODE_CONFIG_1 0
#define MODE_PLAY     1

#define NB_MODE     2

byte mode = MODE_CONFIG_1;
boolean play_pattern = 0;

#define SERIAL_MODE_SERIAL 0
#define SERIAL_MODE_MIDI   1
#define SERIAL_MODE_USB    2
byte serial_mode = SERIAL_MODE_USB;

const prog_char SERIAL_MODE_LABEL_01[] PROGMEM = "SER";
const prog_char SERIAL_MODE_LABEL_02[] PROGMEM = "MID";
const prog_char SERIAL_MODE_LABEL_03[] PROGMEM = "USB";
const char *SERIAL_MODE_LABELS[] PROGMEM = {
  SERIAL_MODE_LABEL_01, SERIAL_MODE_LABEL_02, SERIAL_MODE_LABEL_03
};
const prog_uint32_t SERIAL_MODE_VALUES[] PROGMEM = {
  9600, 31250, 115200
};

const byte COLS_ORDER[NB_INST] = {
  1, 0, 3, 2
};

#define RYTHM_MODE_POLY 0
#define RYTHM_MODE_SOLO 1
byte rythm_mode[NB_BANK][NB_INST];

const prog_char RYTHM_MODE_00[] PROGMEM = "PL";
const prog_char RYTHM_MODE_01[] PROGMEM = "SL";
const char *RYTHM_MODE_LABELS[] PROGMEM = {
  RYTHM_MODE_00, RYTHM_MODE_01
};

LiquidCrystal lcd(2,3,4,5,6,7);

boolean lcd_mode_bitmap = false;

#define BITMAP_PULS_01 0
#define BITMAP_PULS_02 1
#define BITMAP_PULS_03 2
#define BITMAP_PULS_04 3
#define BITMAP_ROLL_01 4
#define BITMAP_ROLL_02 5
#define BITMAP_ROLL_03 6
#define BITMAP_ROLL_04 7

const prog_char BITMAP_CHAR_PULS_01[] PROGMEM = {
  B00000, B00000, B00000, B00000, B00000, B00100, B11111, B00000};
const prog_char BITMAP_CHAR_PULS_02[] PROGMEM = {
  B00000, B00000, B00000, B00100, B00100, B00100, B11111, B00000};
const prog_char BITMAP_CHAR_PULS_03[] PROGMEM = {
  B00000, B00100, B00100, B00100, B00100, B00100, B11111, B00000};
const prog_char BITMAP_CHAR_PULS_04[] PROGMEM = {
  B00100, B00100, B00100, B00100, B00100, B00100, B11111, B00000};
  
const prog_char BITMAP_CHAR_ROLL_01[] PROGMEM = {
  B00000, B00000, B00000, B00000, B00000, B01010, B11111, B00000};
const prog_char BITMAP_CHAR_ROLL_02[] PROGMEM = {
  B00000, B00000, B00000, B01010, B01010, B01010, B11111, B00000};
const prog_char BITMAP_CHAR_ROLL_03[] PROGMEM = {
  B00000, B01010, B01010, B01010, B01010, B01010, B11111, B00000};
const prog_char BITMAP_CHAR_ROLL_04[] PROGMEM = {
  B01010, B01010, B01010, B01010, B01010, B01010, B11111, B00000};

const char *BITMAP_CHARS[] PROGMEM = {
  BITMAP_CHAR_PULS_01, BITMAP_CHAR_PULS_02, BITMAP_CHAR_PULS_03, BITMAP_CHAR_PULS_04,
  BITMAP_CHAR_ROLL_01, BITMAP_CHAR_ROLL_02, BITMAP_CHAR_ROLL_03, BITMAP_CHAR_ROLL_04
};

//-----MIDI-----//            
byte bpm = 120;//Default BPM
unsigned int old_clock_time = 0;
boolean midi_master = 1; //1 master sync  0 slave sync  Default MasterBPM

//Midi message define
#define MIDI_START 0xfa
#define MIDI_STOP  0xfc
#define MIDI_CLOCK 0xf8

const prog_char MIDI_NOTE_LABEL_01[] PROGMEM   = "C%d";
const prog_char MIDI_NOTE_LABEL_02[] PROGMEM   = "C%d#";
const prog_char MIDI_NOTE_LABEL_03[] PROGMEM   = "D%d";
const prog_char MIDI_NOTE_LABEL_04[] PROGMEM   = "D%d#";
const prog_char MIDI_NOTE_LABEL_05[] PROGMEM   = "E%d";
const prog_char MIDI_NOTE_LABEL_06[] PROGMEM   = "F%d";
const prog_char MIDI_NOTE_LABEL_07[] PROGMEM   = "F%d#";
const prog_char MIDI_NOTE_LABEL_08[] PROGMEM   = "G%d";
const prog_char MIDI_NOTE_LABEL_09[] PROGMEM   = "G%d#";
const prog_char MIDI_NOTE_LABEL_10[] PROGMEM   = "A%d";
const prog_char MIDI_NOTE_LABEL_11[] PROGMEM   = "A%d#";
const prog_char MIDI_NOTE_LABEL_12[] PROGMEM   = "B%d";

const char *MIDI_NOTE_LABELS[] PROGMEM = {
  MIDI_NOTE_LABEL_01, MIDI_NOTE_LABEL_02, MIDI_NOTE_LABEL_03, MIDI_NOTE_LABEL_04, MIDI_NOTE_LABEL_05, MIDI_NOTE_LABEL_06,
  MIDI_NOTE_LABEL_07, MIDI_NOTE_LABEL_08, MIDI_NOTE_LABEL_09, MIDI_NOTE_LABEL_10, MIDI_NOTE_LABEL_11, MIDI_NOTE_LABEL_12
};

//Time in microsecond of the callback fuction
uint16_t timer_time = 5000;
int16_t count_ppqn[NB_BANK][NB_INST];
int16_t old_count_ppqn[NB_BANK][NB_INST];

byte count_step[NB_BANK][NB_INST];
byte count_puls[NB_BANK][NB_INST];
byte track_last_step[NB_BANK][NB_INST];
boolean track_step_chng[NB_BANK][NB_INST];

byte track_step[NB_BANK][NB_INST];
byte track_puls[NB_BANK][NB_INST];
byte track_offs[NB_BANK][NB_INST];
byte track_roll[NB_BANK][NB_INST];
byte track_roff[NB_BANK][NB_INST];
byte track_acnt[NB_BANK][NB_INST];
byte track_aoff[NB_BANK][NB_INST];
byte track_swing[NB_BANK][NB_INST];

boolean noteOn[NB_BANK][NB_INST];

byte  midi_note[NB_BANK][NB_INST] = {
  {36, 37, 38, 39},
  {40, 41, 42, 43},
  {44, 45, 46, 47}, 
  {48, 49, 50, 51}
};
byte  midi_chnl[NB_BANK][NB_INST];
byte *midi_velo[NB_BANK][NB_INST];

byte *beat_holder[NB_BANK][NB_INST];
byte *roll_holder[NB_BANK][NB_INST];
byte *acnt_holder[NB_BANK][NB_INST];

byte track_scale[NB_BANK][NB_INST];

// Licklogic
byte valuePot[5][NB_BANK][NB_INST][NB_PARM] = {0};
// Smooz reading
uint16_t tot_valuePot[NB_INST][NB_PARM] = {0};

#define AVG_SIZE 4
byte avg_index = 0;

// Buttons et LED
#define BUTTON_RED         7
#define BUTTON_BLACK       6
#define BUTTON_WHITE_LEFT  5
#define BUTTON_WHITE_RIGHT 4
#define BUTTON_BLACK_01    3
#define BUTTON_BLACK_02    2
#define BUTTON_BLACK_03    1
#define BUTTON_BLACK_04    0

boolean valueBut[8];
boolean one_but_press = false;
boolean one_pot_changed = false;

#define LED_RED         BUTTON_RED
#define LED_BLACK       BUTTON_BLACK
#define LED_WHITE_LEFT  BUTTON_WHITE_LEFT
#define LED_WHITE_RIGHT BUTTON_WHITE_RIGHT
#define LED_BLACK_01    BUTTON_BLACK_01
#define LED_BLACK_02    BUTTON_BLACK_02
#define LED_BLACK_03    BUTTON_BLACK_03
#define LED_BLACK_04    BUTTON_BLACK_04

const byte LED_inst[NB_INST] = {
  LED_BLACK_01, LED_BLACK_02, LED_BLACK_03, LED_BLACK_04
};

const prog_char LABEL_NO[]  PROGMEM = "NO";
const prog_char LABEL_YES[] PROGMEM = "YES";
const char *YESNO_LABELS[] PROGMEM = {
  LABEL_NO, LABEL_YES
};

#define LCD_MODE_PLAY_01 BUTTON_BLACK_01
#define LCD_MODE_PLAY_02 BUTTON_BLACK_02
#define LCD_MODE_PLAY_03 BUTTON_BLACK_03
#define LCD_MODE_PLAY_04 BUTTON_BLACK_04
#define NB_LCD_MODE_PLAY 4
#define LCD_MODE_PLAY_00 NB_LCD_MODE_PLAY

const prog_char LCD_MODE_LABEL_00[] PROGMEM = " MST BPM BUS BNK";
const prog_char LCD_PLAY_LABEL_00[] PROGMEM = "%u%u STP PLS OFF  ";
const prog_char LCD_PLAY_LABEL_01[] PROGMEM = "%u%u     ROL OFF  ";
const prog_char LCD_PLAY_LABEL_02[] PROGMEM = "%u%u VEL ACT OFF  ";
const prog_char LCD_PLAY_LABEL_03[] PROGMEM = "%u%u SG RT CN NT  ";
const prog_char LCD_PLAY_LABEL_04[] PROGMEM = "%u%u ARP TYP PAR  ";
const char *LCD_MODE_LABELS[] PROGMEM = {
  LCD_MODE_LABEL_00, LCD_PLAY_LABEL_04, LCD_PLAY_LABEL_03,
  LCD_PLAY_LABEL_02, LCD_PLAY_LABEL_01, LCD_PLAY_LABEL_00
};

#define _2U 0
#define _3U 1
#define _4S 2
#define _4S4U4S4U 3
#define _CLEAR_4 4

const prog_char LABEL_2U[] PROGMEM = "%2u";
const prog_char LABEL_3U[] PROGMEM = "%3u";
const prog_char LABEL_4S[] PROGMEM = "%-4s";
const prog_char LABEL_4S4U4S4U[] PROGMEM = "%4s%4u%4s%4u";
const prog_char LABEL_CLEAR_4[] PROGMEM = "    ";
const char *LABELS[] PROGMEM = {
  LABEL_2U, LABEL_3U, LABEL_4S, LABEL_4S4U4S4U, LABEL_CLEAR_4
};

byte lcd_mode = MODE_PLAY;
byte lcd_mode_play = LCD_MODE_PLAY_00;
boolean lcd_mode_play_changed = true;

//--------------------------------------------SETUP----------------------------------------------//
void setup() {
  // Attach callback function to Timer1
  Timer1.attachInterrupt(Count_PPQN);

  //initialize special character
  lcd.begin(16,2);
  for(byte _i = 0; _i < 8; _i++) {
    byte *_buffer = (byte*) malloc(8);
    memcpy_P(_buffer, (PGM_P) pgm_read_byte(&(BITMAP_CHARS[_i])), 8);
    lcd.createChar(_i, _buffer);
  }
  lcd.clear();

  _updateSerialMode();

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
  while (digitalRead (12)==LOW);
  
  pinMode(12, OUTPUT);// ceci afin déviter les conflits avec la bibliothèque SRIO

  // Init Buttons
  SR.Initialize();
  SR.Led_All_On();

  // Init default value
  for(byte _i = 0; _i < NB_BANK; _i++) {
    for(byte _j = 0; _j < NB_INST; _j++) {
      beat_holder[_i][_j] = NULL;
      roll_holder[_i][_j] = NULL;
      acnt_holder[_i][_j] = NULL;

      rythm_mode [_i][_j] = RYTHM_MODE_POLY;
      count_ppqn [_i][_j] = -1;
      old_count_ppqn [_i][_j] = -1;
      count_step [_i][_j] = 0;
      count_puls [_i][_j] = 0;

      track_step [_i][_j] = 0;
      track_puls [_i][_j] = 0;
      track_offs [_i][_j] = 0;
      track_roll [_i][_j] = 0;
      track_roff [_i][_j] = 0;
      track_acnt [_i][_j] = 0;
      track_aoff [_i][_j] = 0;

      track_scale[_i][_j] = 6;
      track_swing[_i][_j] = 0;

      track_last_step[_i][_j] = 0;
      track_step_chng[_i][_j] = false;

      midi_velo  [_i][_j] = NULL;
      midi_chnl  [_i][_j] = 1;
      noteOn     [_i][_j] = false;
    }
  }
  delay(500);
  SR.Led_All_Off();
}
//--------------------------------------------SETUP----------------------------------------------//

void _updateSerialMode() {
  Serial.begin((uint32_t) pgm_read_dword(&(SERIAL_MODE_VALUES[serial_mode])));
}

//--------------------------------------------LOOP-----------------------------------------------//
void loop() {
  Timer1.initialize(timer_time);

  checkButValues();
  if(updatePotValues()) {
    checkPotValues();
  }

  updateMidi();
  updateLcd();
}

void checkButValues() {
  one_but_press = false;
  lcd_mode_play_changed = false;

  if(mode == MODE_PLAY) {
    for(int _i = 0; _i < NB_LCD_MODE_PLAY; _i++) {
      if(checkButPress(_i)) {
        update_lcd_mode_play(_i);
        return;
      }   
    }
    
    if(lcd_mode_play == LCD_MODE_PLAY_02 && !play_pattern) {
      if(checkButPress(BUTTON_WHITE_LEFT)) {
        count_step[bank][inst] = (count_step[bank][inst] + track_step[bank][inst] - 1) % track_step[bank][inst];
        if(beat_holder[bank][inst][count_step[bank][inst] / 8] & (1 << (count_step[bank][inst] % 8))) {
          count_puls[bank][inst] = (count_puls[bank][inst] + track_puls[bank][inst] - 1) % track_puls[bank][inst];
        }
        track_step_chng[bank][inst] = true;
        return;
      }
      if(checkButPress(BUTTON_WHITE_RIGHT)) {
        count_step[bank][inst] = (count_step[bank][inst] + 1) % track_step[bank][inst];
        if(beat_holder[bank][inst][count_step[bank][inst] / 8] & (1 << (count_step[bank][inst] % 8))) {
          count_puls[bank][inst] = (count_puls[bank][inst] + 1) % track_puls[bank][inst];
        }
        track_step_chng[bank][inst] = true;
        return;
      }
      return;
    }
  }

  if(mode == MODE_CONFIG_1) {
    if(checkButPress(BUTTON_BLACK_01)) {
      midi_master = !midi_master;
      return;
    }

    checkButPress(BUTTON_BLACK_02);
    if(midi_master && valueBut[BUTTON_BLACK_02]) {
      if(checkButPress(BUTTON_WHITE_LEFT)) {
        bpm--;
        return;
      }

      if(checkButPress(BUTTON_WHITE_RIGHT)) {
        bpm++;
        return;
      }
    }

    if(checkButPress(BUTTON_BLACK_03)) {
      serial_mode = (serial_mode + 1) % 3;
      _updateSerialMode();
      return;
    } 

    if(checkButPress(BUTTON_BLACK_04)) {
      bank = (bank + 1) % NB_BANK;
      return;
    } 
  }

  if(checkButPress(BUTTON_RED)) {
    if(midi_master) {
      play_pattern = !play_pattern;
      SR.Led_Pin_Write(LED_RED, play_pattern);
      if(!play_pattern) {
        for(byte _i = 0; _i < NB_BANK; _i++) {
          for(byte _j = 0; _j < NB_INST; _j++) {
            count_ppqn[_i][_j] = -1;
            count_step[_i][_j] = 0;
            count_puls[_i][_j] = 0;
          }
        }
      }
    }
    return;
  }

  if(checkButPress(BUTTON_BLACK)) {
    lcd_mode_bitmap = !lcd_mode_bitmap;
    lcd_mode_play_changed = true;
    SR.Led_Pin_Write(LED_BLACK, byte(lcd_mode_bitmap));
    return;
  }

  // Navigation
  if(checkButPress(BUTTON_WHITE_LEFT)) {
    mode = (mode + NB_MODE - 1) % NB_MODE;
    return;
  }

  if(checkButPress(BUTTON_WHITE_RIGHT)) {
    mode = (mode + 1) % NB_MODE;
    return;
  }
}

boolean checkButPress(byte _button) {
  if(SR.Button_Pin_Read(_button)) {
    if(!valueBut[_button]) {
      valueBut[_button] = true;
      one_but_press = true;
      return true;
    }
  } 
  else {
    valueBut[_button] = false;
  }
  return false;
}

void update_lcd_mode_play(byte _new_lcd_mode_play) {
  lcd_mode_play_changed = true;
  if(_new_lcd_mode_play == lcd_mode_play) {
    SR.Led_Pin_Write(lcd_mode_play, 0);
    lcd_mode_play = LCD_MODE_PLAY_00;
  } 
  else {
    SR.Led_Pin_Write(lcd_mode_play, 0);
    SR.Led_Pin_Write(_new_lcd_mode_play, 1);
    lcd_mode_play = _new_lcd_mode_play;
  }
}

boolean updatePotValues() {
  one_pot_changed = false;
  inst_changed = false;
  
  for(byte _i = 0; _i < NB_INST; _i++) {
    byte _inst = COLS_ORDER[_i];
    for(byte _j = 0; _j < NB_PARM; _j++) {
      if(avg_index > 0) {
        digitalWrite(8,LOW);
        tot_valuePot[_inst][_j] += analogRead(_inst / 2) >> 2; //convert 10bits 0->1024 to 8bits 0->255
        digitalWrite(8,HIGH);
      } else {
        tot_valuePot[_inst][_j] = 0;
      }
    }
  }
  
  if(avg_index >= AVG_SIZE) {
    avg_index = 0;
    return true;
  }
  
  avg_index++;
  return false;
}

void checkPotValues() {
  byte _pot_value, _diff, _old_track_puls;

  for(byte _i = 0; _i < NB_INST; _i++) {
    byte _inst = COLS_ORDER[_i];
    for(byte _j = 0; _j < NB_PARM; _j++) {
      _pot_value = byte(tot_valuePot[_inst][_j] / AVG_SIZE);
      _pot_value = map(_pot_value, 6, 247, 0, 255);
      _pot_value = max(_pot_value, 0);
      _pot_value = constrain(_pot_value, 0, 255);
      
      _diff = abs(_pot_value - valuePot[lcd_mode_play][bank][_inst][_j]);
      if (_diff > 0 && _diff < 16) {
        // Change current instrument (ie. pot column)
        if(inst != _inst) {
          inst = _inst;
          inst_changed = true;
        }
        mode = MODE_PLAY;
        valuePot[lcd_mode_play][bank][inst][_j] = _pot_value;
        
        switch(lcd_mode_play) {
        case LCD_MODE_PLAY_00:
          switch(_j) {
          case 0 :
            break;
          case 1 : 
            if(checkValuePotChange(&track_step[bank][inst], _pot_value, 32, 255)) {
              // Change scale
              if((rythm_mode[bank][inst] == RYTHM_MODE_SOLO) && (track_step[bank][inst] != 0)) { 
                track_scale[bank][inst] = (96 / track_step[bank][inst]) * 2; // 96 = 24 ppqn x 4 noires = nb ppqm / bar
              } 
              else {
                track_scale[bank][inst] = 6;
              }
              
              _old_track_puls = track_puls[bank][inst];
              track_puls[bank][inst] = valuePot[LCD_MODE_PLAY_00][bank][inst][2] * track_step[bank][inst] / 255;
              _updateMidiVelo(_old_track_puls);
              
              track_offs[bank][inst] = valuePot[LCD_MODE_PLAY_00][bank][inst][3] * track_step[bank][inst] / 255;
              track_roll[bank][inst] = valuePot[LCD_MODE_PLAY_01][bank][inst][2] * track_puls[bank][inst] / 255;
              track_roff[bank][inst] = valuePot[LCD_MODE_PLAY_01][bank][inst][3] * track_puls[bank][inst] / 255;
              track_acnt[bank][inst] = valuePot[LCD_MODE_PLAY_02][bank][inst][2] * track_puls[bank][inst] / 255;
              track_aoff[bank][inst] = valuePot[LCD_MODE_PLAY_02][bank][inst][3] * track_puls[bank][inst] / 255;
            }
            break;
          case 2 :
            _old_track_puls = track_puls[bank][inst];
            if(checkValuePotChange(&track_puls[bank][inst], _pot_value, track_step[bank][inst], 255)) {
              _updateMidiVelo(_old_track_puls);
              
              track_offs[bank][inst] = valuePot[LCD_MODE_PLAY_00][bank][inst][3] * track_step[bank][inst] / 255;
              track_roll[bank][inst] = valuePot[LCD_MODE_PLAY_01][bank][inst][2] * track_puls[bank][inst] / 255;
              track_roff[bank][inst] = valuePot[LCD_MODE_PLAY_01][bank][inst][3] * track_puls[bank][inst] / 255;
              track_acnt[bank][inst] = valuePot[LCD_MODE_PLAY_02][bank][inst][2] * track_puls[bank][inst] / 255;
              track_aoff[bank][inst] = valuePot[LCD_MODE_PLAY_02][bank][inst][3] * track_puls[bank][inst] / 255;
            }
            break;
          case 3 :
            if(checkValuePotChange(&track_offs[bank][inst], _pot_value, track_step[bank][inst], 255)) {
              track_roll[bank][inst] = valuePot[LCD_MODE_PLAY_01][bank][inst][2] * track_puls[bank][inst] / 255;
              track_roff[bank][inst] = valuePot[LCD_MODE_PLAY_01][bank][inst][3] * track_puls[bank][inst] / 255;
              track_acnt[bank][inst] = valuePot[LCD_MODE_PLAY_02][bank][inst][2] * track_puls[bank][inst] / 255;
              track_aoff[bank][inst] = valuePot[LCD_MODE_PLAY_02][bank][inst][3] * track_puls[bank][inst] / 255;
            }
            break;
          }
  
          if(one_pot_changed) {
            euclid(track_step[bank][inst], track_puls[bank][inst], track_offs[bank][inst], &beat_holder[bank][inst]);
            euclid(track_puls[bank][inst], track_roll[bank][inst], track_roff[bank][inst], &roll_holder[bank][inst]);
            euclid(track_puls[bank][inst], track_acnt[bank][inst], track_aoff[bank][inst], &acnt_holder[bank][inst]);
            return;
          }
          break;
        case LCD_MODE_PLAY_01 :
          switch(_j) {
          case 0 :
            break;
          case 1 :
            break;
          case 2 :
            if(checkValuePotChange(&track_roll[bank][inst], _pot_value, track_puls[bank][inst], 255)) {
              euclid(track_puls[bank][inst], track_roll[bank][inst], track_roff[bank][inst], &roll_holder[bank][inst]);
              return;
            }
            break;
          case 3 :
            if(checkValuePotChange(&track_roff[bank][inst], _pot_value, track_puls[bank][inst], 255)) {
              euclid(track_puls[bank][inst], track_roll[bank][inst], track_roff[bank][inst], &roll_holder[bank][inst]);
              return;
            }
            break;
          }
          break;
        case LCD_MODE_PLAY_02:
          switch(_j) {
          case 0 :
            break;
          case 1 :
            if(!play_pattern) {
              if(checkValuePotChange(&midi_velo[bank][inst][count_puls[bank][inst]], _pot_value, 3, 255)) {
                return;
              }
            }
            break;
          case 2 :
            if(checkValuePotChange(&track_acnt[bank][inst], _pot_value, track_puls[bank][inst], 255)) {
              euclid(track_puls[bank][inst], track_acnt[bank][inst], track_aoff[bank][inst], &acnt_holder[bank][inst]);
              return;
            }
            break;
          case 3 :
            if(checkValuePotChange(&track_aoff[bank][inst], _pot_value, track_puls[bank][inst], 255)) {
              euclid(track_puls[bank][inst], track_acnt[bank][inst], track_aoff[bank][inst], &acnt_holder[bank][inst]);
              return;
            }
            break;
          }
          break;
        case LCD_MODE_PLAY_03:
          switch(_j) {
          case 0 :
            if(checkValuePotChange(&track_swing[bank][inst], _pot_value, track_scale[bank][inst], 255)) {
              return;
            }
            break;
          case 1 :
            _pot_value = _pot_value * 64 / 255;
            if(_pot_value > 31 && 
              rythm_mode[bank][inst] == RYTHM_MODE_POLY) {
              one_pot_changed = true;
              rythm_mode[bank][inst] = RYTHM_MODE_SOLO; // == 1
            } 
            else if(_pot_value < 32 && 
              rythm_mode[bank][inst] == RYTHM_MODE_SOLO) {
              one_pot_changed = true;
              rythm_mode[bank][inst] = RYTHM_MODE_POLY; // == 0
            }
  
            if(one_pot_changed) {
              // Update scale
              if(rythm_mode[bank][inst] == RYTHM_MODE_SOLO && track_step[bank][inst] != 0) {
                track_scale[bank][inst] = (96 / track_step[bank][inst]) * 2; // 96 = 24 ppqn x 4 noires = nb ppqm / bar
              } 
              else {
                track_scale[bank][inst] = 6;
              }
              return;
            }
            break;
          case 2 :
            if(checkValuePotChange(&midi_chnl[bank][inst], _pot_value, 12, 255)) {
              return;
            }
            break;
          case 3 :
            if(checkValuePotChange(&midi_note[bank][inst], _pot_value, 127, 255)) {
              return;
            }
            break;
          }
          break;
        case LCD_MODE_PLAY_04:
          switch(_j) {
          case 0 :
            break;
          case 1 :
            break;
          case 2 :
            break;
          case 3 :
            break;
          }
          break;
        }
      }
    }
  } 
}

byte checkValuePotChange(byte *value, byte _value_pot, byte _precision, byte _plage) {
  byte _pot_value = _value_pot * _precision / _plage;
  if(*value != _pot_value) {
    *value = _pot_value;
    one_pot_changed = true;
    return true;
  }
  return false;
}

void _updateMidiVelo(byte _old_track_puls) {
  // Change midi_velo
  if(track_puls[bank][inst] != 0) {
    byte* _tmp_midi_velo = (byte*) malloc(track_puls[bank][inst] * sizeof(byte));

    for(int _k = 0; _k < track_puls[bank][inst]; _k++) {
      if(_old_track_puls != 0 && _k < _old_track_puls) {
        _tmp_midi_velo[_k] = midi_velo[bank][inst][_k];
      } 
      else {
        _tmp_midi_velo[_k] = 2;
      }
    }
    free(midi_velo[bank][inst]);
    midi_velo[bank][inst] = _tmp_midi_velo;
  } 
  else {
    free(midi_velo[bank][inst]);
    midi_velo[bank][inst] = NULL;
  }
}

void updateLcd() {
  boolean _lcd_mode_changed = false;
  
  if(lcd_mode != mode) {
    lcd_mode = mode;
    _lcd_mode_changed = true;
    lcd_mode_play_changed = true;
  }

  // MODE BITMAP
  if(lcd_mode_bitmap) {
    if(_lcd_mode_changed || lcd_mode_play_changed
      || (play_pattern  && track_step_chng[bank][inst]) 
      || (!play_pattern && one_pot_changed)) {
      lcd.clear();

      byte _pulse_step = 0;
      for(byte _i = 0; _i <  track_step[bank][inst]; _i++) {
        lcd.setCursor(_i % 16, _i / 16);
        
        if(_i == count_step[bank][inst] && play_pattern) {
          lcd.write(char(219));
          
          if(!beat_holder[bank][inst][_i / 8] & (1 << (_i % 8))) {
            continue;
          }
        }
        if(beat_holder[bank][inst][_i / 8] & (1 << (_i % 8))) {
          if(_i == count_step[bank][inst] && play_pattern) {
            _pulse_step++;
            continue;
          }
          
          byte _step_char = 0;
          // Apply velocity and accent
          _step_char += midi_velo[bank][inst][_pulse_step];
          
          if(acnt_holder[bank][inst] != NULL 
            && acnt_holder[bank][inst][_pulse_step / 8] & (1 << (_pulse_step % 8))) {
            _step_char++;
          }
          _step_char = min(_step_char, 3);
          
          // Apply roll
          if(roll_holder[bank][inst] != NULL 
            && roll_holder[bank][inst][_pulse_step / 8] & (1 << (_pulse_step % 8))) {
            _step_char += 4;
          }
          
          lcd.write(byte(_step_char));
          _pulse_step++;
        } else {
          // Write '_' step empty
          lcd.write(char(95));
        }
      }
    }
    return;
  } 

  // MODE FIRST LINE
  char _line_buf[16] = {'\0'};
  if(_lcd_mode_changed || lcd_mode_play_changed || inst_changed) {
    if(lcd_mode == MODE_CONFIG_1) {
      sprintf_P(_line_buf, (PGM_P) pgm_read_word(&(LCD_MODE_LABELS[lcd_mode])),
        bank, inst);
    } else if (mode == MODE_PLAY) {
      sprintf_P(_line_buf, (PGM_P) pgm_read_word(&(LCD_MODE_LABELS[lcd_mode_play + 1])),
        bank, inst);
    }
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(_line_buf);
  }

  char _label[4] = {'\0'};
  if(lcd_mode == MODE_PLAY) {
    //MODE PLAY VALUES LINES
    if(one_pot_changed || lcd_mode_play_changed) {
      if(lcd_mode_play == LCD_MODE_PLAY_01) {
        sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_3U])), track_roll[bank][inst]); 
        lcd.setCursor(7, 1);
        lcd.print(_label);
        
        sprintf_P(_label,(PGM_P) pgm_read_word(&(LABELS[_3U])), track_roff[bank][inst]); 
        lcd.setCursor(11, 1);
        lcd.print(_label);
      } 
      else if(lcd_mode_play == LCD_MODE_PLAY_02) {
        sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_3U])), track_acnt[bank][inst]); 
        lcd.setCursor(7, 1);
        lcd.print(_label);
        
        sprintf_P(_label,(PGM_P) pgm_read_word(&(LABELS[_3U])), track_aoff[bank][inst]); 
        lcd.setCursor(11, 1);
        lcd.print(_label);
      } 
      else if(lcd_mode_play == LCD_MODE_PLAY_03) {
        sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_2U])), track_swing[bank][inst]); 
        lcd.setCursor(3, 1);
        lcd.print(_label);

        strcpy_P(_label, (PGM_P) pgm_read_word(&(RYTHM_MODE_LABELS[rythm_mode[bank][inst]])));
        lcd.setCursor(6, 1);
        lcd.print(_label);

        sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_2U])), midi_chnl[bank][inst]);
        lcd.setCursor(9, 1);
        lcd.print(_label);

        char _label2[4] = {'\0'};
        sprintf_P(_label, 
        (PGM_P) pgm_read_word(&(MIDI_NOTE_LABELS[midi_note[bank][inst] % 12])),
        midi_note[bank][inst] / 12);
        sprintf_P(_label2, (PGM_P) pgm_read_word(&(LABELS[_4S])), _label);
        lcd.setCursor(12, 1);
        lcd.print(_label2);
      } 
      else if(lcd_mode_play == LCD_MODE_PLAY_04) {

      } 
      else {
        sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_3U])), track_step[bank][inst]);
        lcd.setCursor(3, 1);
        lcd.print(_label);
        
        sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_3U])), track_puls[bank][inst]);
        lcd.setCursor(7, 1);
        lcd.print(_label);
        
        sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_3U])), track_offs[bank][inst]);
        lcd.setCursor(11, 1);
        lcd.print(_label);
      }
    }
      
    if(track_step_chng[bank][inst] || _lcd_mode_changed || lcd_mode_play_changed || one_pot_changed) {
      // Print Current Step
      lcd.setCursor(0, 1);
      sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_2U])), count_step[bank][inst] + 1);
      lcd.print(_label);
      
      lcd.setCursor(2, 1);
      if((beat_holder[bank][inst][count_step[bank][inst] / 8] & (1 << (count_step[bank][inst] % 8)))
        && track_step[bank][inst] != 0) {
        // Print "." if active step
        lcd.write(char(46));
      } else {
        // Print " " if inactive step
        lcd.write(char(32));
      }
      
      if(lcd_mode_play == LCD_MODE_PLAY_02) {
        // Print current pulse velocity
        lcd.setCursor(3, 1);  
        if(beat_holder[bank][inst][count_step[bank][inst] / 8] & (1 << (count_step[bank][inst] % 8))) {
          sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_3U])), 
            min((midi_velo[bank][inst][count_puls[bank][inst]] + 1) * 32, 127));
        } else {
          sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_CLEAR_4])));
        }
        lcd.print(_label);
      }
    }
    return;
  }

  if(lcd_mode == MODE_CONFIG_1) {
    if(_lcd_mode_changed || one_but_press) {
      strcpy_P(_line_buf, (char*) pgm_read_word(&(LCD_MODE_LABELS[lcd_mode])));
      lcd.setCursor(0,0);
      lcd.print(_line_buf);

      char _ser[4], _yesno[4];
      strcpy_P(_yesno, (PGM_P) pgm_read_word(&(YESNO_LABELS[midi_master])));
      strcpy_P(_ser, (PGM_P) pgm_read_word(&(SERIAL_MODE_LABELS[serial_mode])));
      sprintf_P(_line_buf, (PGM_P) pgm_read_word(&(LABELS[_4S4U4S4U])),
      _yesno, bpm, _ser, bank
        );
  
      lcd.setCursor(0,1);
      lcd.print(_line_buf);
    }
    return;
  }
}

void updateMidi() {
  if(one_but_press && valueBut[BUTTON_RED]) {
    if(play_pattern) {
      Serial.write(MIDI_START);
    } 
    else {
      Serial.write(MIDI_STOP);
      for(byte _i = 0; _i < NB_BANK; _i++) {
        for(byte _j = 0; _j < NB_INST; _j++) {
          Send_NoteOFF(_i, _j);
        }
      }
    }
  }

  if(play_pattern) {
    for(byte _i = 0; _i < NB_BANK; _i++) {
      for(byte _j = 0; _j < NB_INST; _j++) {
        if(track_puls[_i][_j] > 0 && old_count_ppqn[_i][_j] != count_ppqn[_i][_j]) {
          old_count_ppqn[_i][_j] = count_ppqn[_i][_j];
          // New step
          if(track_step_chng[_i][_j]) {
            Send_NoteOFF(_i, _j);
            if(count_step[_i][_j] == 0) {
              count_puls[_i][_j] = 0;
            }
          }
          // Apply roll
          else if(noteOn[_i][_j] 
            && (roll_holder[_i][_j] != NULL 
                && roll_holder[_i][_j][(count_puls[_i][_j] - 1) / 8] & (1 << ((count_puls[_i][_j] - 1) % 8)))
            && (count_ppqn[_i][_j] % track_scale[_i][_j] == track_scale[_i][_j] / 2)) {
            if(Send_NoteOFF(_i, _j)) {
              count_puls[_i][_j]--;
            }
          }

          if(beat_holder[bank][inst][count_step[_i][_j] / 8] & (1 << (count_step[_i][_j] % 8))) {
            if(!noteOn[_i][_j]) {
              byte _midi_velo = midi_velo[_i][_j][count_puls[_i][_j]];
              
              if(acnt_holder[_i][_j] != NULL 
                && acnt_holder[_i][_j][count_puls[_i][_j] / 8] & (1 << (count_puls[_i][_j] % 8))) {
                _midi_velo++;
              }
              
              _midi_velo = min(_midi_velo, 3);
              if(Send_NoteON(_i, _j, min((_midi_velo + 1) * 32, 127))) { 
                count_puls[_i][_j]++;
              }
            }
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
  if(!midi_master) {
    timer_time=5000;
    if(Serial.available() > 0) {
      byte data = Serial.read();
      if(data == MIDI_START) {
        play_pattern = true;
        for(byte _i = 0; _i < NB_BANK; _i++) {
          for(byte _j = 0; _j < NB_INST; _j++) {
            count_ppqn[_i][_j] = -1;
          }
        }
      }

      else if(data == MIDI_STOP ) {
        play_pattern = false;
        for(byte _i = 0; _i < NB_BANK; _i++) {
          for(byte _j = 0; _j < NB_INST; _j++) {
            count_step[_i][_j] = 0;
            count_ppqn[_i][_j] = -1;
          }
        }
      }
      else if(data == MIDI_CLOCK && play_pattern) {
        int time = millis();
        bpm = (byte) (time - old_clock_time) / 24 * 60000;
        old_clock_time = time;
        update_all_count_ppqn();
      }
    }
  }

  //-----------------Sync MASTER-------------------//
  if(midi_master){
    timer_time=2500000/bpm;
    Serial.write(MIDI_CLOCK);
    if(play_pattern) {  
      update_all_count_ppqn();
    }
  }
}

void update_all_count_ppqn() { 
  for(byte _i = 0; _i < NB_BANK; _i++) {
    for(byte _j = 0; _j < NB_INST; _j++) {
      if(track_puls[_i][_j] > 0) {
        count_ppqn[_i][_j]++;
        byte current_track_step = 0;

        // Apply swing
        // Delay the second 16th note within each 8th note.
        // ie. delay all the even-numbered 16th notes within the beat (2, 4, 6, 8, etc.)
        if(track_swing[_i][_j] != 0 && current_track_step % 2 == 0) {
          current_track_step = (count_ppqn[_i][_j] - track_swing[_i][_j]) / track_scale[_i][_j];
        } 
        else {
          current_track_step = count_ppqn[_i][_j] / track_scale[_i][_j]; //stretch
        }

        if(count_step[_i][_j] != current_track_step) {
          if(count_ppqn[_i][_j] >= (track_step[_i][_j] * track_scale[_i][_j]) - 1) {
            count_ppqn[_i][_j] = -1;
            current_track_step = 0;
          }
          count_step[_i][_j] = current_track_step;
        }

        if(track_last_step[_i][_j] != count_step[_i][_j]) {
          track_step_chng[_i][_j] = true;
          track_last_step[_i][_j] = count_step[_i][_j];
        } 
        else {
          track_step_chng[_i][_j] = false;
        }
      }
    }
  }
}
// Euclid calculation function
uint8_t eucl_pos;
int8_t eucl_remainder[32];
int8_t eucl_count[32];

void euclid(byte _steps, byte _pulse, byte _offs, byte* *_tab) {
  // Init tab
  free(*_tab);
  if(_steps > 0) {
    *_tab = (boolean*) malloc((_steps / 8) + 1);
    for(byte _i = 0; _i < _steps; _i++) {
      (*_tab)[_i / 8] &= ~(1 << (_i % 8));
    }
  } 
  else {
    *_tab = NULL;
  }

  if(_pulse == 0) {
    return;
  }

  eucl_pos = 0;
  eucl_remainder[0] = _pulse; 

  int8_t _divisor = (int8_t) _steps - _pulse;
  int8_t _level = 0; 
  int8_t _cycleLength = 1; 
  int8_t _remLength = 1;
  do { 
    eucl_count[_level] = _divisor / eucl_remainder[_level]; 
    eucl_remainder[_level+1] = _divisor % eucl_remainder[_level]; 
    _divisor = eucl_remainder[_level];

    int8_t _newLength = (_cycleLength * eucl_count[_level]) + _remLength; 
    _remLength = _cycleLength; 
    _cycleLength = _newLength; 
    _level++;
  }
  while(eucl_remainder[_level] > 1);

  eucl_count[_level] = _divisor; 
  if(eucl_remainder[_level] > 0) {
    _cycleLength = (_cycleLength * eucl_count[_level]) + _remLength;
  }
  build(_level, _steps, _offs, *_tab);
}

void build(int8_t _level, byte _steps, byte _offs, uint8_t* _tab) {
  uint8_t _newPos = eucl_pos++;

  // Apply offset
  _newPos = (uint8_t) (_newPos + _offs + 1)  % _steps;  

  if(_level == -1) {
    _tab[_newPos / 8] &= ~(1 << (_newPos % 8));
  } 
  else if(_level == -2) {
    _tab[_newPos / 8] |= 1 << (_newPos % 8); 
  } 
  else {
    eucl_pos--;
    for(byte _p=0; _p < eucl_count[_level]; _p++) {
      build(_level-1, _steps, _offs, _tab); 
    }

    if(eucl_remainder[_level] != 0) {
      build(_level-2, _steps, _offs, _tab); 
    }
  }
}

boolean Send_NoteON(byte _i, byte _j, byte velo) {
  if(!noteOn[_i][_j]) {
    Serial.write (144 + midi_chnl[_i][_j] - 1);//Note ON on selected channel 
    Serial.write (midi_note[_i][_j]);
    Serial.write (velo);

    if(_i == bank) {
      SR.Led_Pin_Write(LED_inst[_j], 1);
    }
    noteOn[_i][_j] = true;
    return true;
  }
  return false;
}

boolean Send_NoteOFF(byte _i, byte _j) {
  if(noteOn[_i][_j]) {
    Serial.write (128 + midi_chnl[_i][_j] - 1);//Note OFF on selected channel
    Serial.write (midi_note[_i][_j]);
    Serial.write (0x01);//velocite 0

    if(_i == bank) {
      SR.Led_Pin_Write(LED_inst[_j], 0);
    }
    noteOn[_i][_j] = false;
    return true;
  }
  return false;
}




