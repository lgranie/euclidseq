/*
* Author:       Laurent Granié
 * Copyright:    (C) 2014 Laurent Granié
 * Licence:      GNU General Public Licence version 3
 */
#include <TimerOne.h>
#include <SRIO.h>
#include <LiquidCrystal.h>

/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) (a[b/8] |= 1 << (b%8))
#define BIT_CLR(a,b) (a[b/8] &= ~(1 << (b%8)))
#define BIT_FLP(a,b) (a[b/8] ^= 1 << (b%8))
#define BIT_CHK(a,b) (a[b/8] & (1 << (b%8)))

#define NB_BANK 4
#define NB_INST 4
#define NB_INSTS 16 // NB_BANK x NB_INST
#define MAX_STEP 32

#define BANK (index / NB_INST)
#define INST (index % NB_INST)

byte index = 0;
boolean inst_changed = false;

#define NB_PARM 4

//Mode define
#define MODE_CONFIG_1 0
#define MODE_PLAY     1

#define NB_MODE     2

byte mode = MODE_CONFIG_1;
boolean play_pattern = 0;

#define SERIAL_MODE_MIDI   0
#define SERIAL_MODE_USB    1
byte serial_mode = SERIAL_MODE_USB;

const prog_char SERIAL_MODE_LABEL_01[] PROGMEM = "MID";
const prog_char SERIAL_MODE_LABEL_02[] PROGMEM = "USB";
const char *SERIAL_MODE_LABELS[] PROGMEM = {
  SERIAL_MODE_LABEL_01, SERIAL_MODE_LABEL_02
};
const prog_uint32_t SERIAL_MODE_VALUES[] PROGMEM = {
  31250, 115200
};

const byte COLS_ORDER[NB_INST] = {
  1, 0, 3, 2
};

#define RYTHM_MODE_POLY 0
#define RYTHM_MODE_SOLO 1
byte rythm_mode[NB_INSTS];

const prog_char RYTHM_MODE_00[] PROGMEM = "PL";
const prog_char RYTHM_MODE_01[] PROGMEM = "SL";
const char *RYTHM_MODE_LABELS[] PROGMEM = {
  RYTHM_MODE_00, RYTHM_MODE_01
};

LiquidCrystal lcd(2,3,4,5,6,7);

boolean lcd_mode_bitmap = false;

#define BITMAP_PULS_01 0
#define BITMAP_PULS_02 1
#define BITMAP_ROLL_01 2
#define BITMAP_ROLL_02 3

const prog_char BITMAP_CHAR_PULS_01[8] PROGMEM = {
  B00000, B00000, B00100, B00100, B00100, B00100, B11111, B00000};
const prog_char BITMAP_CHAR_PULS_02[8] PROGMEM = {
  B00100, B00100, B00100, B00100, B00100, B00100, B11111, B00000};
  
const prog_char BITMAP_CHAR_ROLL_01[8] PROGMEM = {
  B00000, B00000, B01010, B01010, B01010, B01010, B11111, B00000};
const prog_char BITMAP_CHAR_ROLL_02[8] PROGMEM = {
  B01010, B01010, B01010, B01010, B01010, B01010, B11111, B00000};

const prog_char *BITMAP_CHARS[] PROGMEM = {
  BITMAP_CHAR_PULS_01, BITMAP_CHAR_PULS_02,
  BITMAP_CHAR_ROLL_01, BITMAP_CHAR_ROLL_02
};

#define MAJOR      0
#define MINOR      1
#define IONIAN     2
#define DORIAN     3
#define PHRYGIAN   4
#define LYDIAN     5
#define MIXOLYDIAN 6
#define AEOLIAN    7
#define MAJOR_P    8
#define MINOR_P    9
#define BLUES      10
#define MONO       11
const prog_uchar MAJOR_SCALE[7]      PROGMEM = {0, 2, 4, 5,  7,  9, 11 };
const prog_uchar MINOR_SCALE[7]      PROGMEM = {0, 2, 3, 5,  7,  8, 10 };
const prog_uchar IONIAN_SCALE[7]     PROGMEM = {0, 2, 4, 5,  7,  9, 11 };
const prog_uchar DORIAN_SCALE[7]     PROGMEM = {0, 1, 3, 5,  7,  8, 10 };
const prog_uchar PHRYGIAN_SCALE[7]   PROGMEM = {0, 2, 4, 6,  7,  9, 11 };
const prog_uchar LYDIAN_SCALE[7]     PROGMEM = {0, 2, 4, 5,  7,  9, 10 };
const prog_uchar MIXOLYDIAN_SCALE[7] PROGMEM = {0, 2, 3, 5,  7,  8, 10 };
const prog_uchar AEOLIAN_SCALE[7]    PROGMEM = {0, 1, 3, 5,  6,  8, 10 };
const prog_uchar MAJOR_P_SCALE[7]    PROGMEM = {0, 2, 4, 7,  9, 12, 14 };
const prog_uchar MINOR_P_SCALE[7]    PROGMEM = {0, 2, 5, 7, 10, 12, 14 };
const prog_uchar BLUES_SCALE[7]      PROGMEM = {0, 2, 3, 4,  7,  9, 12 };

const prog_uchar *SCALES[] PROGMEM = {
  MAJOR_SCALE, MINOR_SCALE, IONIAN_SCALE, DORIAN_SCALE, PHRYGIAN_SCALE, LYDIAN_SCALE, 
  MIXOLYDIAN_SCALE, AEOLIAN_SCALE, MAJOR_P_SCALE, MINOR_P_SCALE, BLUES_SCALE
};

const prog_char SCALE_LABEL_00[] PROGMEM   = "MAJ";
const prog_char SCALE_LABEL_01[] PROGMEM   = "MIN";
const prog_char SCALE_LABEL_02[] PROGMEM   = "ION";
const prog_char SCALE_LABEL_03[] PROGMEM   = "DOR";
const prog_char SCALE_LABEL_04[] PROGMEM   = "PHR";
const prog_char SCALE_LABEL_05[] PROGMEM   = "LYD";
const prog_char SCALE_LABEL_06[] PROGMEM   = "MIX";
const prog_char SCALE_LABEL_07[] PROGMEM   = "AEO";
const prog_char SCALE_LABEL_08[] PROGMEM   = "MJP";
const prog_char SCALE_LABEL_09[] PROGMEM   = "MNP";
const prog_char SCALE_LABEL_10[] PROGMEM   = "BLU";
const prog_char SCALE_LABEL_11[] PROGMEM   = "MON";
const prog_char *SCALES_LABELS[] PROGMEM = {
  SCALE_LABEL_00, SCALE_LABEL_01, SCALE_LABEL_02,
  SCALE_LABEL_03, SCALE_LABEL_04, SCALE_LABEL_05,
  SCALE_LABEL_06, SCALE_LABEL_07, SCALE_LABEL_08,
  SCALE_LABEL_09, SCALE_LABEL_10, SCALE_LABEL_11
};

//-----MIDI-----//            
byte bpm = 120;//Default BPM
unsigned int old_clock_time = 0;
boolean midi_master = 1; //1 master sync  0 slave sync  Default MasterBPM

//Midi message define
#define MIDI_START 0xfa
#define MIDI_STOP  0xfc
#define MIDI_CLOCK 0xf8
#define DEFAULT_MIDI_VELO 100

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
int16_t count_ppqn[NB_INSTS];

byte count_step[NB_INSTS];
byte count_puls[NB_INSTS];
boolean updated_ppqn = false;
byte *step_chng;

byte stps[NB_INSTS];
byte puls[NB_INSTS];
byte offs[NB_INSTS];
byte roll[NB_INSTS];
byte roff[NB_INSTS];
byte acnt[NB_INSTS];
byte aoff[NB_INSTS];
byte swing[NB_INSTS];

byte arp_scale[NB_INSTS];
byte arp_step[NB_INSTS];

byte *noteOn;

// Default notes from MFB Tanzbar
byte  midi_note[NB_INSTS] = {
  36, 37, 38, 39,
  40, 41, 42, 43,
  44, 45, 46, 47, 
  48, 49, 50, 51
};
byte  midi_chnl[NB_INSTS];
byte  last_midi_note[NB_INSTS];

byte *beat_holder[NB_INSTS];
byte *roll_holder[NB_INSTS];
byte *acnt_holder[NB_INSTS];

byte scale[NB_INSTS];

// Licklogic
byte valuePot[5][NB_INSTS][NB_PARM] = {0};
// Smooz reading
uint16_t tot_valuePot[NB_INST][NB_PARM] = {0};

#define AVG_SIZE 10
byte avg_inst = 0;

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

const byte LED[NB_INST] = {
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
const prog_char LCD_PLAY_LABEL_00[] PROGMEM = "%u%u   STP PLS OFF";
const prog_char LCD_PLAY_LABEL_01[] PROGMEM = "%u%u  RL OF  AT OF";
const prog_char LCD_PLAY_LABEL_02[] PROGMEM = "%u%u              ";
const prog_char LCD_PLAY_LABEL_03[] PROGMEM = "%u%u SG RT  CN NT ";
const prog_char LCD_PLAY_LABEL_04[] PROGMEM = "%u%u SCL STP PAR  ";
const char *LCD_MODE_LABELS[] PROGMEM = {
  LCD_MODE_LABEL_00, LCD_PLAY_LABEL_04, LCD_PLAY_LABEL_03,
  LCD_PLAY_LABEL_02, LCD_PLAY_LABEL_01, LCD_PLAY_LABEL_00
};

#define _2U       0
#define _3U       1
#define _4S       2
#define _4S4U4S4U 3
#define _2x3U     4
#define _3x3U     5
#define _4x2U     6
#define _CLEAR_4  7

const prog_char LABEL_2U[] PROGMEM = "%2u";
const prog_char LABEL_3U[] PROGMEM = "%3u";
const prog_char LABEL_4S[] PROGMEM = "%-4s";
const prog_char LABEL_4S4U4S4U[] PROGMEM = "%4s%4u%4s%4u";
const prog_char LABEL_2x3U[] PROGMEM = "%3u %3u";
const prog_char LABEL_3x3U[] PROGMEM = "%3u %3u %3u";
const prog_char LABEL_4x2U[] PROGMEM = "%2u %2u  %2u %2u ";
const prog_char LABEL_CLEAR_4[] PROGMEM = "    ";
const char *LABELS[] PROGMEM = {
  LABEL_2U, LABEL_3U, LABEL_4S, LABEL_4S4U4S4U, LABEL_2x3U, LABEL_3x3U, LABEL_4x2U, LABEL_CLEAR_4
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
  byte *_buffer = (byte*) malloc(8);
  for(byte _i = 0; _i < 8; _i++) {
    memcpy_P(_buffer, (PGM_P) pgm_read_byte(&(BITMAP_CHARS[_i])), 8);
    lcd.createChar(_i, _buffer);
  }
  free(_buffer);
  lcd.clear();

  _updateSerialMode();

  // Init Potentiometres
  pinMode(8, OUTPUT);    // clock Pin du compteur CD4520
  pinMode(12, INPUT);    // Pin temoin d'initialisation du compteur binaire CD4520
  
  //Reset √† 0 du compteur binaire CD4520
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
  
  pinMode(12, OUTPUT);// Conflits with SRIO lib
  // Init Buttons
  SR.Initialize();
  SR.Led_All_On();

  step_chng = (byte*) malloc(NB_INSTS / 8);
  noteOn = (byte*) malloc(NB_INSTS / 8);
  
  // Init default value
  for(byte _i = 0; _i < NB_INSTS; _i++) {

    rythm_mode [_i] = RYTHM_MODE_POLY;

    beat_holder[_i] = NULL;
    roll_holder[_i] = NULL;
    acnt_holder[_i] = NULL;

    count_ppqn[_i] = -1;
    count_step[_i] = 0;
    count_puls[_i] = 0;

    stps[_i] = 0;
    puls[_i] = 0;
    offs[_i] = 0;
    roll[_i] = 0;
    roff[_i] = 0;
    acnt[_i] = 0;
    aoff[_i] = 0;
    
    arp_scale[_i] = MONO;
    arp_step[_i]  = 0;
    
    scale[_i] = 6;
    swing[_i] = 0;

    BIT_CLR(step_chng, _i);
    
    last_midi_note[_i] = midi_note[_i];
    midi_chnl[_i] = 1;
    BIT_CLR(noteOn, _i);
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
  updated_ppqn = false;
  Timer1.initialize(timer_time);
  
  checkButValues();
  // Smooz pot reading
  if(updatePotValues()) {
    checkPotValues();
  }
  
  updateLcd();
  updateMidi();
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
      serial_mode = (serial_mode + 1) % 2;
      _updateSerialMode();
      return;
    } 

    if(checkButPress(BUTTON_BLACK_04)) {
      // Change bank
      index = (index + NB_INST) % NB_INSTS;
      return;
    } 
  }

  if(checkButPress(BUTTON_RED)) {
    if(midi_master) {
      play_pattern = !play_pattern;
      SR.Led_Pin_Write(LED_RED, play_pattern);
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
  } else {
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
  
  byte _bank = index / NB_INST;
  
  for(byte _i = 0; _i < NB_INST; _i++) {
    for(byte _j = 0; _j < NB_PARM; _j++) {
      
      if(avg_inst > 0) {
        digitalWrite(8,LOW);
        tot_valuePot[_i][_j] += analogRead(_i / 2) >> 2; //convert 10bits 0->1024 to 8bits 0->255
        digitalWrite(8,HIGH);
      } else {
        tot_valuePot[_i][_j] = 0;
      }
    }
  }
  
  if(avg_inst >= AVG_SIZE) {
    avg_inst = 0;
    return true;
  }
  
  avg_inst++;
  return false;
}

void checkPotValues() {
  byte _pot_value, _diff;

  for(byte _i = 0; _i < NB_INST; _i++) {
    for(byte _j = 0; _j < NB_PARM; _j++) {
      // Current inst index
      byte _index = BANK * NB_INST + COLS_ORDER[_i];
      
      // Compute pot value
      _pot_value = byte(tot_valuePot[_i][_j] / AVG_SIZE);
      _pot_value = map(_pot_value, 6, 247, 0, 255);
      _pot_value = max(_pot_value, 0);
      _pot_value = constrain(_pot_value, 0, 255);
      
      // if diff
      _diff = abs(_pot_value - valuePot[lcd_mode_play][_index][_j]);
      if (_diff > 0 && _diff < 12) {
        // Change current instrument (ie. pot column)
        if(index != _index) {
          inst_changed = true;
          index = _index;
        }
        mode = MODE_PLAY;
        valuePot[lcd_mode_play][index][_j] = _pot_value;
        
        switch(lcd_mode_play) {
        case LCD_MODE_PLAY_00:
          switch(_j) {
          case 0 :
            break;
          case 1 : 
            if(checkValuePotChange(&stps[index], _pot_value, MAX_STEP, 255)) {
              // Change scale
              if((rythm_mode[index] == RYTHM_MODE_SOLO) && (stps[index] != 0)) { 
                scale[index] = (96 / stps[index]) * 2; // 96 = 24 ppqn x 4 noires = nb ppqm / bar
              } 
              else {
                scale[index] = 6;
              }
              
              puls[index] = valuePot[LCD_MODE_PLAY_00][index][2] * stps[index] / 255;
              
              offs[index] = valuePot[LCD_MODE_PLAY_00][index][3] * stps[index] / 255;
              roll[index] = valuePot[LCD_MODE_PLAY_01][index][0] * puls[index] / 255;
              roff[index] = valuePot[LCD_MODE_PLAY_01][index][1] * puls[index] / 255;
              acnt[index] = valuePot[LCD_MODE_PLAY_01][index][2] * puls[index] / 255;
              aoff[index] = valuePot[LCD_MODE_PLAY_01][index][3] * puls[index] / 255;
            }
            break;
          case 2 :
            if(checkValuePotChange(&puls[index], _pot_value, stps[index], 255)) {
              
              offs[index] = valuePot[LCD_MODE_PLAY_00][index][3] * stps[index] / 255;
              roll[index] = valuePot[LCD_MODE_PLAY_01][index][0] * puls[index] / 255;
              roff[index] = valuePot[LCD_MODE_PLAY_01][index][1] * puls[index] / 255;
              acnt[index] = valuePot[LCD_MODE_PLAY_01][index][2] * puls[index] / 255;
              aoff[index] = valuePot[LCD_MODE_PLAY_01][index][3] * puls[index] / 255;
            }
            break;
          case 3 :
            if(checkValuePotChange(&offs[index], _pot_value, stps[index], 255)) {
              roll[index] = valuePot[LCD_MODE_PLAY_01][index][0] * puls[index] / 255;
              roff[index] = valuePot[LCD_MODE_PLAY_01][index][1] * puls[index] / 255;
              acnt[index] = valuePot[LCD_MODE_PLAY_01][index][2] * puls[index] / 255;
              aoff[index] = valuePot[LCD_MODE_PLAY_01][index][3] * puls[index] / 255;
            }
            break;
          }
  
          if(one_pot_changed) {
            euclid(stps[index], puls[index], offs[index], &beat_holder[index]);
            euclid(puls[index], roll[index], roff[index], &roll_holder[index]);
            euclid(puls[index], acnt[index], aoff[index], &acnt_holder[index]);
            return;
          }
          break;
        case LCD_MODE_PLAY_01 :
          switch(_j) {
          case 0 :
            if(checkValuePotChange(&roll[index], _pot_value, puls[index], 255)) {
              euclid(puls[index], roll[index], roff[index], &roll_holder[index]);
              return;
            }
            break;
          case 1 :
            if(checkValuePotChange(&roff[index], _pot_value, puls[index], 255)) {
              euclid(puls[index], roll[index], roff[index], &roll_holder[index]);
              return;
            }
            break;
          case 2 :
            if(checkValuePotChange(&acnt[index], _pot_value, puls[index], 255)) {
              euclid(puls[index], acnt[index], aoff[index], &acnt_holder[index]);
              return;
            }
            break;
          case 3 :
            if(checkValuePotChange(&aoff[index], _pot_value, puls[index], 255)) {
              euclid(puls[index], acnt[index], aoff[index], &acnt_holder[index]);
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
            break;
          case 2 :
            break;
          case 3 :
            break;
          }
          break;
        case LCD_MODE_PLAY_03:
          switch(_j) {
          case 0 :
            if(checkValuePotChange(&swing[index], _pot_value, scale[index], 255)) {
              return;
            }
            break;
          case 1 :
            _pot_value = _pot_value * 64 / 255;
            if(_pot_value > 31 && 
              rythm_mode[index] == RYTHM_MODE_POLY) {
              one_pot_changed = true;
              rythm_mode[index] = RYTHM_MODE_SOLO; // == 1
            } 
            else if(_pot_value < 32 && 
              rythm_mode[index] == RYTHM_MODE_SOLO) {
              one_pot_changed = true;
              rythm_mode[index] = RYTHM_MODE_POLY; // == 0
            }
  
            if(one_pot_changed) {
              // Update scale
              if(rythm_mode[index] == RYTHM_MODE_SOLO && stps[index] != 0) {
                scale[index] = (96 / stps[index]) * 2; // 96 = 24 ppqn x 4 noires = nb ppqm / bar
              } 
              else {
                scale[index] = 6;
              }
              return;
            }
            break;
          case 2 :
            if(checkValuePotChange(&midi_chnl[index], _pot_value, 12, 255)) {
              return;
            }
            break;
          case 3 :
            if(checkValuePotChange(&midi_note[index], _pot_value, 127, 255)) {
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
            if(checkValuePotChange(&arp_scale[index], _pot_value, 11, 255)) {
              return;
            }
            break;
          case 2 :
            if(checkValuePotChange(&arp_step[index], _pot_value, 7, 255)) {
              return;
            }
      
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

void updateLcd() {
  boolean _lcd_mode_changed = false;
  
  if(lcd_mode != mode) {
    lcd_mode = mode;
    _lcd_mode_changed = true;
    lcd_mode_play_changed = true;
  }

  // MODE BITMAP
  if(lcd_mode_bitmap) {
    if(BIT_CHK(step_chng, index)) {
      Serial.println("UPDATE LCD");
    }
    if(_lcd_mode_changed || lcd_mode_play_changed 
      || (!play_pattern && one_pot_changed)
      || (play_pattern  && BIT_CHK(step_chng, index))) {
      lcd.clear();

      byte _pulse_step = 0;
      for(byte _step = 0; _step <  stps[index]; _step++) {
        lcd.setCursor(_step % 16, _step / 16);
        
        if(_step == count_step[index] && play_pattern) {
          lcd.write(char(219));
          
          if(BIT_CHK(beat_holder[index],_step)) {
            _pulse_step++;
          }
          continue;
        }
        
        if(BIT_CHK(beat_holder[index], _step)) {
          byte _step_char = 0;
          // Apply accent
          if(acnt_holder[index] != NULL 
            && BIT_CHK(acnt_holder[index], _pulse_step)) {
            _step_char++;
          }
          _step_char = min(_step_char, 1);
          
          // Apply roll
          if(roll_holder[index] != NULL 
            && BIT_CHK(roll_holder[index], _pulse_step)) {
            _step_char += 2;
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
    byte _mode;
    if(lcd_mode == MODE_CONFIG_1) {
      _mode = lcd_mode;
    } else if (mode == MODE_PLAY) {
      _mode = lcd_mode_play + 1;
    }
    sprintf_P(_line_buf, (PGM_P) pgm_read_word(&(LCD_MODE_LABELS[_mode])), BANK, INST);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(_line_buf);
  }

  if(lcd_mode == MODE_PLAY) {
    //MODE PLAY VALUES LINES
    if(one_pot_changed || lcd_mode_play_changed) {
      if(lcd_mode_play == LCD_MODE_PLAY_01) {
        char _label[13] = {'\0'};
        sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_4x2U])), roll[index], 
                                                                   roff[index], 
                                                                   acnt[index], 
                                                                   aoff[index]);
        lcd.setCursor(4, 1);
        lcd.print(_label);
      } 
      else if(lcd_mode_play == LCD_MODE_PLAY_02) {
        
      } 
      else if(lcd_mode_play == LCD_MODE_PLAY_03) {
        char _label[4] = {'\0'};
        sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_2U])), swing[index]); 
        lcd.setCursor(3, 1);
        lcd.print(_label);

        strcpy_P(_label, (PGM_P) pgm_read_word(&(RYTHM_MODE_LABELS[rythm_mode[index]])));
        lcd.setCursor(6, 1);
        lcd.print(_label);

        sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_2U])), midi_chnl[index]);
        lcd.setCursor(10, 1);
        lcd.print(_label);

        char _label2[4] = {'\0'};
        sprintf_P(_label, 
        (PGM_P) pgm_read_word(&(MIDI_NOTE_LABELS[midi_note[index] % 12])),
        midi_note[index] / 12);
        sprintf_P(_label2, (PGM_P) pgm_read_word(&(LABELS[_4S])), _label);
        lcd.setCursor(13, 1);
        lcd.print(_label2);
      } 
      else if(lcd_mode_play == LCD_MODE_PLAY_04) {
        char _label[4] = {'\0'};
        memcpy_P(_label, (PGM_P) pgm_read_word(&(SCALES_LABELS[arp_scale[index]])), 3);
        lcd.setCursor(3, 1);
        lcd.print(_label);
        
        sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_3U])), arp_step[index]);
        lcd.setCursor(7, 1);
        lcd.print(_label);
      } 
      else {
        char _label[12] = {'\0'};
        sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_3x3U])), stps[index],
                                                                   puls[index],
                                                                   offs[index]);
        lcd.setCursor(5, 1);
        lcd.print(_label);
      }
    }
      
    if(BIT_CHK(step_chng, index) || _lcd_mode_changed || lcd_mode_play_changed || one_pot_changed) {
      // Print Current Step
      lcd.setCursor(0, 1);
      char _label[4] = {'\0'};
      sprintf_P(_label, (PGM_P) pgm_read_word(&(LABELS[_2U])), count_step[index] + 1);
      lcd.print(_label);
      
      lcd.setCursor(2, 1);
      if(BIT_CHK(beat_holder[index], count_step[index]) && stps[index] != 0) {
        // Print "." if active step
        lcd.write(char(46));
      } else {
        // Print " " if inactive step
        lcd.write(char(32));
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
      _yesno, bpm, _ser, BANK
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
      for(byte _inst = 0; _inst < NB_INSTS; _inst++) {
        // Init all step_chng
        if(_inst % 8 == 0) {
          step_chng[_inst / 8] = B11111111;
        }
        count_step[_inst] = 0;
      }
      updated_ppqn = true;
    } 
    else {
      Serial.write(MIDI_STOP);
      for(byte _inst = 0; _inst < NB_INSTS; _inst++) {
        if(BIT_CHK(noteOn, _inst)) {
          Send_NoteOFF(_inst);
        }
        count_ppqn[_inst] = -1;
        count_step[_inst] = 0;
        count_puls[_inst] = 0;
        // Init all step_chng
        if(_inst % 8 == 0) {
          step_chng[_inst / 8] = B00000000;
        }
      }
    }
  }

  if(play_pattern && updated_ppqn) {
    for(byte _inst = 0; _inst < NB_INSTS; _inst++) {
      if(puls[_inst] > 0 && BIT_CHK(step_chng, _inst)) {
        if(BIT_CHK(noteOn, _inst)) {
          Send_NoteOFF(_inst);
        }
      
        // Play step
        if(BIT_CHK(beat_holder[_inst], count_step[_inst]) && !BIT_CHK(noteOn, _inst)) {
          Send_NoteON(_inst);
          count_puls[_inst]++;
        }
        BIT_CLR(step_chng, _inst);
      }
    }
    updated_ppqn = false;
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
        for(byte _inst = 0; _inst < NB_INSTS; _inst++) {
          count_step[_inst] = MAX_STEP + 1;
        }
      }

      else if(data == MIDI_STOP ) {
        play_pattern = false;
        for(byte _inst = 0; _inst < NB_INSTS; _inst++) {
          count_step[_inst] = 0;
          count_puls[_inst] = 0;
          count_ppqn[_inst] = -1;
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
  if(midi_master) {
    timer_time=2500000/bpm;
    Serial.write(MIDI_CLOCK);
    if(play_pattern) {  
      update_all_count_ppqn();
    }
  }
}

void update_all_count_ppqn() {
  updated_ppqn = true;
  for(byte _inst = 0; _inst < NB_INSTS; _inst++) {
    if(puls[_inst] > 0) {
      count_ppqn[_inst]++;
      
      byte _current_step = 0;
      // Apply swing
      // Delay the second 16th note within each 8th note.
      // ie. delay all the even-numbered 16th notes within the beat (2, 4, 6, 8, etc.)
      if(swing[_inst] != 0 && ((count_step[_inst] % 2) == 0)) {
        _current_step = (count_ppqn[_inst] - swing[_inst]) / scale[_inst];
      } 
      else {
        _current_step = count_ppqn[_inst] / scale[_inst]; //stretch
      }

      // New step
      if(count_step[_inst] != _current_step) {
        BIT_SET(step_chng, _inst);
        // Loop to the start of sequence
        if((count_ppqn[_inst] >= (stps[_inst] * scale[_inst]) - 1)) {
          count_ppqn[_inst] = -1;
          count_step[_inst] = 0;
          count_puls[_inst] = 0;
          continue;
        }
        count_step[_inst] = _current_step;
        continue;
      } else {
        // Apply roll on old active step
        if(roll_holder[_inst] != NULL && BIT_CHK(noteOn, _inst) 
          && ((count_ppqn[_inst] % scale[_inst]) == (scale[_inst] / 2))
          && BIT_CHK(roll_holder[_inst], (count_puls[_inst] - 1)) ) {
          
          BIT_SET(step_chng, _inst);
          count_puls[_inst]--;
          continue;
        }
      }
    }
  }
}

// Euclid calculation function
uint8_t eucl_pos;
int8_t eucl_remainder[MAX_STEP];
int8_t eucl_count[MAX_STEP];

void euclid(byte _step, byte _pulse, byte _offs, byte* *_tab) {
  // Init tab
  free(*_tab);
  if(_step > 0) {
    *_tab = (boolean*) malloc((_step / 8) + 1);
    for(byte _i = 0; _i < _step; _i++) {
      BIT_CLR((*_tab), _i);
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

  int8_t _divisor = (int8_t) _step - _pulse;
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
  build(_level, _step, _offs, *_tab);
}

void build(int8_t _level, byte _step, byte _offs, uint8_t* _tab) {
  uint8_t _newPos = eucl_pos++;

  // Apply offset
  _newPos = (uint8_t) (_newPos + _offs + 1)  % _step;  

  if(_level == -1) {
    BIT_CLR(_tab, _newPos);
  } 
  else if(_level == -2) {
    BIT_SET(_tab, _newPos);
  } 
  else {
    eucl_pos--;
    for(byte _p=0; _p < eucl_count[_level]; _p++) {
      build(_level-1, _step, _offs, _tab); 
    }

    if(eucl_remainder[_level] != 0) {
      build(_level-2, _step, _offs, _tab); 
    }
  }
}

void Send_NoteON(byte _inst) {
  // Compute midi note
  byte _midi_note = midi_note[_inst];
  
  // Apply ARP
  if(arp_scale[_inst] != MONO && arp_step[_inst] != 0) {
    byte* _scale_mods = (byte*) malloc(arp_step[_inst]);
    memcpy_P(_scale_mods, (PGM_P) pgm_read_byte(&(SCALES[arp_scale[_inst]])), arp_step[_inst]);
    byte _scale_mod = _scale_mods[count_puls[_inst] % arp_step[_inst]];
    free(_scale_mods);
    _midi_note += _scale_mod;
  }
  last_midi_note[_inst] = min(_midi_note, 127);
  
  // Compute midi velo
  byte _midi_velo = DEFAULT_MIDI_VELO;
  if(acnt_holder[_inst] != NULL && BIT_CHK(acnt_holder[_inst], count_puls[_inst])) {
     _midi_velo = 127;
  }
  
  Serial.write(144 + midi_chnl[_inst] - 1);//Note ON on selected channel
  Serial.write(last_midi_note[_inst]);
  Serial.write(_midi_velo);

  // Turn on inst led
  if((_inst / NB_INST) == BANK) {
    SR.Led_Pin_Write(LED[_inst % NB_INST], 1);
  }
  // Note On
  BIT_SET(noteOn, _inst);
}

void Send_NoteOFF(byte _inst) {
  Serial.write (128 + midi_chnl[_inst] - 1);//Note OFF on selected channel
  Serial.write(last_midi_note[_inst]);
  Serial.write (0x01);//velocite 0
  
  // Turn off inst led
  if((_inst / NB_INST) == BANK) {
    SR.Led_Pin_Write(LED[_inst % NB_INST], 0);
  }
  
  // Note Off
  BIT_CLR(noteOn, _inst);
}

