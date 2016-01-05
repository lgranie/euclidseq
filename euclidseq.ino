/*
* Author:       Laurent Granié
 * Copyright:    (C) 2014 Laurent Granié
 * Licence:      GNU General Public Licence version 3
 */
#include <TimerOne.h>
#include <SmoozPotMUX.h>
#include <SRIO.h>
#include <LiquidCrystal.h>

/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) (a[b/8] |= 1 << (b%8))
#define BIT_CLR(a,b) (a[b/8] &= ~(1 << (b%8)))
#define BIT_FLP(a,b) (a[b/8] ^= 1 << (b%8))
#define BIT_CHK(a,b) (a[b/8] & (1 << (b%8)))
#define BIT_BYT(a,b) !!(a[b/8] & (1 << (b%8)))

#define NB_PARM  8
#define NB_INSTS 16 // NB_BANK x NB_INST
#define MAX_STEP 32

#define INST_CHANGE     0
#define PLAY_PATTERN    1
#define MIDI_MASTER     2
#define UPDATED_PPQN    3
#define BUTTON_PRESS    4
#define POT_CHANGE      5
byte state = B00000101;

#define LCD_MODE_BITMAP 0
#define LCD_PLAY_CHANGE 1
#define LCD_DRAW_BITMAP 2
#define LCD_MODE_CHANGE 3
byte lcd_state = B00000010;

byte inst = 0;

//Mode define
#define MODE_CONFIG_1 0
#define MODE_PLAY     1
#define NB_MODE     2

byte mode = MODE_PLAY;

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

#define RYTHM_MODE_POLY 0
#define RYTHM_MODE_SOLO 1
byte rythm_mode[NB_INSTS];

const prog_char RYTHM_MODE_00[] PROGMEM = "Pol";
const prog_char RYTHM_MODE_01[] PROGMEM = "Sol";
const char *RYTHM_MODE_LABELS[] PROGMEM = {
  RYTHM_MODE_00, RYTHM_MODE_01
};

LiquidCrystal lcd(2,3,4,5,6,7);

const prog_char BITMAP_PULS_01[8] PROGMEM = {
  B00000, B00000, B00100, B00100, B00100, B00100, B11111, B00000};
const prog_char BITMAP_PULS_02[8] PROGMEM = {
  B00100, B00100, B00100, B00100, B00100, B00100, B11111, B00000};
const prog_char BITMAP_ROLL_01[8] PROGMEM = {
  B00000, B00000, B01010, B01010, B01010, B01010, B11111, B00000};
const prog_char BITMAP_ROLL_02[8] PROGMEM = {
  B01010, B01010, B01010, B01010, B01010, B01010, B11111, B00000};
const prog_char *BITMAP_CHARS[] PROGMEM = {
  BITMAP_PULS_01, BITMAP_PULS_02,
  BITMAP_ROLL_01, BITMAP_ROLL_02
};


/////////////////////////////////////////////////////////
// ARPEGGIO
/////////////////////////////////////////////////////////
#define MAJOR      0
#define MINOR      1
#define IONIAN     2
#define DORIAN     3
#define PHRYGIAN   4
#define LYDIAN     5
#define MIXOLYDIAN 6
#define AEOLIAN    7
#define BLUES      8
#define OFF        9
const prog_uchar ARP_SCALE_MAJOR[7]      PROGMEM = {0, 2, 4, 5, 7, 9, 11};
const prog_uchar ARP_SCALE_MINOR[7]      PROGMEM = {0, 2, 3, 5, 7, 8, 10};
const prog_uchar ARP_SCALE_IONIAN[7]     PROGMEM = {0, 2, 4, 5, 7, 9, 11};
const prog_uchar ARP_SCALE_DORIAN[7]     PROGMEM = {0, 1, 3, 5, 7, 8, 10};
const prog_uchar ARP_SCALE_PHRYGIAN[7]   PROGMEM = {0, 2, 4, 6, 7, 9, 11};
const prog_uchar ARP_SCALE_LYDIAN[7]     PROGMEM = {0, 2, 4, 5, 7, 9, 10};
const prog_uchar ARP_SCALE_MIXOLYDIAN[7] PROGMEM = {0, 2, 3, 5, 7, 8, 10};
const prog_uchar ARP_SCALE_AEOLIAN[7]    PROGMEM = {0, 1, 3, 5, 6, 8, 10};
const prog_uchar ARP_SCALE_BLUES[7]      PROGMEM = {0, 2, 3, 4, 7, 9, 12};
const prog_uchar *ARP_SCALES[] PROGMEM = {
  ARP_SCALE_MAJOR,  ARP_SCALE_MINOR,    ARP_SCALE_IONIAN,
  ARP_SCALE_DORIAN, ARP_SCALE_PHRYGIAN, ARP_SCALE_LYDIAN, 
  ARP_SCALE_MIXOLYDIAN, ARP_SCALE_AEOLIAN, ARP_SCALE_BLUES
};

const prog_char ARP_SCALE_LABEL_00[] PROGMEM = "Maj";
const prog_char ARP_SCALE_LABEL_01[] PROGMEM = "Min";
const prog_char ARP_SCALE_LABEL_02[] PROGMEM = "Ion";
const prog_char ARP_SCALE_LABEL_03[] PROGMEM = "Dor";
const prog_char ARP_SCALE_LABEL_04[] PROGMEM = "Phr";
const prog_char ARP_SCALE_LABEL_05[] PROGMEM = "Lyd";
const prog_char ARP_SCALE_LABEL_06[] PROGMEM = "Mix";
const prog_char ARP_SCALE_LABEL_07[] PROGMEM = "Aeo";
const prog_char ARP_SCALE_LABEL_08[] PROGMEM = "Blu";
const prog_char ARP_SCALE_LABEL_09[] PROGMEM = "Off";
const prog_char *ARP_SCALE_LABELS[] PROGMEM = {
  ARP_SCALE_LABEL_00, ARP_SCALE_LABEL_01, ARP_SCALE_LABEL_02,
  ARP_SCALE_LABEL_03, ARP_SCALE_LABEL_04, ARP_SCALE_LABEL_05,
  ARP_SCALE_LABEL_06, ARP_SCALE_LABEL_07, ARP_SCALE_LABEL_08,
  ARP_SCALE_LABEL_09
};

#define ARP_TYPE_FULL 0
#define ARP_TYPE_7TH  1
const prog_uchar ARP_TYPE_LABEL_FULL[] PROGMEM = "Ful";
const prog_uchar ARP_TYPE_LABEL_7TH[]  PROGMEM = "7th";
const prog_uchar *ARP_TYPE_LABELS[] PROGMEM = {
  ARP_TYPE_LABEL_FULL, ARP_TYPE_LABEL_7TH
};

#define ARP_ARP_UP   0
#define ARP_ARP_DOWN 1
const prog_uchar ARP_ARP_LABEL_UP[]  PROGMEM = "Up";
const prog_uchar ARP_ARP_LABEL_DWN[] PROGMEM = "Dwn";
const prog_uchar *ARP_ARP_LABELS[] PROGMEM = {
  ARP_ARP_LABEL_UP, ARP_ARP_LABEL_DWN
};

//-----MIDI-----//            
byte bpm = 120; //Default BPM

#define MIDI_MASTER_PPQN 96
#define MIDI_SLAVE_PPQN  24
byte ppqn = MIDI_MASTER_PPQN; // 24, 96, (348?)

//Time in microsecond of the callback fuction
uint16_t old_clock_time = 0;
uint16_t timer_time = 5000;
uint32_t total_timer_time = 0;
uint16_t count_ppqn[NB_INSTS];

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

int8_t count_step[NB_INSTS];
int8_t count_puls[NB_INSTS];
byte *step_chng;

byte stps[NB_INSTS];
byte puls[NB_INSTS];
byte offs[NB_INSTS];
byte roll[NB_INSTS];
byte roff[NB_INSTS];
byte acnt[NB_INSTS];
byte aoff[NB_INSTS];
byte swing[NB_INSTS];
byte swing_final[NB_INSTS];
byte *swg_rnd;

byte arp_scale[NB_INSTS];
byte arp_type[NB_INSTS];
byte arp_arp[NB_INSTS];
byte *arp_rnd;
byte arp_step[NB_INSTS];
byte arp_oct[NB_INSTS];

byte *noteOn;

// Default notes from MFB Tanzbar
byte  midi_note[NB_INSTS] = {
  36, 37, 38, 39, 40, 41, 42, 43, 
  44, 45, 46, 47, 48, 49, 50, 51
};
byte  midi_chnl[NB_INSTS];
byte  last_midi_note[NB_INSTS];
byte  next_midi_note[NB_INSTS];
byte  next_midi_velo[NB_INSTS];

byte *beat_holder[NB_INSTS];
byte *roll_holder[NB_INSTS];
byte *acnt_holder[NB_INSTS];

uint16_t scale[NB_INSTS];

#define LCD_MODE_PLAY_01 0 // 7 - BUTTON_BLACK_01
#define LCD_MODE_PLAY_02 1 // 7 - BUTTON_BLACK_02
#define LCD_MODE_PLAY_00 2
#define NB_LCD_MODE_PLAY 3

const prog_char LCD_CONFIG_LINE1[] PROGMEM = "MST BPM BUS      ";
const prog_char LCD_CONFIG_LINE2[] PROGMEM = "%3s %3u %3s  ";

const char *LCD_CONFIG_LINES[] PROGMEM = {
  LCD_CONFIG_LINE1, LCD_CONFIG_LINE2
};

const prog_char LCD_PLAY_LINE1_00[] PROGMEM = "%2u %2u %2u %2u %-4s";
const prog_char LCD_PLAY_LINE2_00[] PROGMEM =     "%2u %2u %2u %2u ";

// INST SCL TYPE ARP 
//      STP  OCT RND
const prog_char LCD_PLAY_LINE1_01[] PROGMEM = "%2u %3s %3s %3s  ";
const prog_char LCD_PLAY_LINE2_01[] PROGMEM =     "Nt%u Oc%u %3s ";

// INST RYT SWG SWG_RND
//      CHN
const prog_char LCD_PLAY_LINE1_02[] PROGMEM = "%2u %3s Sw%2u %3s ";
const prog_char LCD_PLAY_LINE2_02[] PROGMEM = "        Ch%2u";


const char *LCD_LINES_01[] PROGMEM = {
  LCD_PLAY_LINE1_01, LCD_PLAY_LINE1_02, LCD_PLAY_LINE1_00
};

const char *LCD_LINES_02[] PROGMEM = {
  LCD_PLAY_LINE2_01, LCD_PLAY_LINE2_02, LCD_PLAY_LINE2_00
};

byte lcd_mode = MODE_PLAY;
byte lcd_mode_play = LCD_MODE_PLAY_00;

// Buttons et LED
#define BUTTON_BLACK_01    7
#define BUTTON_BLACK_02    6
#define BUTTON_BLACK_03    5
#define BUTTON_BLACK_04    4
#define BUTTON_RED         3
#define BUTTON_BLACK       2
#define BUTTON_WHITE_LEFT  1
#define BUTTON_WHITE_RIGHT 0

byte buttons;

#define LED_BLACK_01    BUTTON_BLACK_01
#define LED_BLACK_02    BUTTON_BLACK_02
#define LED_BLACK_03    BUTTON_BLACK_03
#define LED_BLACK_04    BUTTON_BLACK_04
#define LED_RED         BUTTON_RED
#define LED_BLACK       BUTTON_BLACK
#define LED_WHITE_LEFT  BUTTON_WHITE_LEFT
#define LED_WHITE_RIGHT BUTTON_WHITE_RIGHT

const byte LED[8] = {
  LED_BLACK_01, LED_BLACK_02, LED_BLACK_03, LED_BLACK_04, LED_WHITE_RIGHT, LED_WHITE_LEFT
};

const prog_char LABEL_NO[]  PROGMEM = "NO";
const prog_char LABEL_YES[] PROGMEM = "YES";
const char *YESNO_LABELS[] PROGMEM = {
  LABEL_NO, LABEL_YES
};

//--------------------------------------------SETUP----------------------------------------------//
void setup() {
  // Attach callback function to Timer1
  Timer1.attachInterrupt(Count_PPQN);

  //initialize special character
  lcd.begin(16,2);
  byte *_buffer = (byte*) malloc(8);
  for(register byte _i = 0; _i < 4; _i++) {
    memcpy_P(_buffer, (PGM_P) pgm_read_dword(&(BITMAP_CHARS[_i])), 8);
    lcd.createChar(_i, _buffer);
  }
  free(_buffer);
  lcd.clear();

  Serial.begin((uint32_t) pgm_read_dword(&(SERIAL_MODE_VALUES[serial_mode])));

  // Init Pot
  SmoozPot.Initialize();

  // Init Buttons
  SR.Initialize();
  SR.Led_All_On();

  step_chng = (byte*) malloc(NB_INSTS / 8);
  noteOn    = (byte*) malloc(NB_INSTS / 8);
  arp_rnd   = (byte*) malloc(NB_INSTS / 8);
  swg_rnd   = (byte*) malloc(NB_INSTS / 8);

  // Init default value
  for(register byte _inst = 0; _inst < NB_INSTS; _inst++) {
    beat_holder[_inst] = NULL;
    roll_holder[_inst] = NULL;
    acnt_holder[_inst] = NULL;

    count_ppqn[_inst] = 0;
    count_step[_inst] = 0;
    count_puls[_inst] = 0;

    stps[_inst] = 0;
    puls[_inst] = 0;
    offs[_inst] = 0;
    roll[_inst] = 0;
    roff[_inst] = 0;
    acnt[_inst] = 0;
    aoff[_inst] = 0;

    arp_scale[_inst] = MAJOR;
    arp_type[_inst]  = ARP_TYPE_FULL;
    arp_arp[_inst]   = ARP_ARP_UP;
    arp_step[_inst]  = 0;
    arp_oct[_inst]   = 1;

    rythm_mode [_inst] = RYTHM_MODE_POLY;
    scale[_inst] = ppqn / 4;
    swing[_inst] = 0;
    swing_final[_inst] = 0;

    BIT_CLR(step_chng, _inst);

    last_midi_note[_inst] = midi_note[_inst];
    next_midi_note[_inst] = 0;
    next_midi_velo[_inst] = DEFAULT_MIDI_VELO;

    midi_chnl[_inst] = 3;
    BIT_CLR(noteOn, _inst);
  }

  delay(500);
  SR.Led_All_Off();
}
//--------------------------------------------SETUP----------------------------------------------//

//--------------------------------------------LOOP-----------------------------------------------//
void loop() {
  Timer1.initialize(timer_time);
  
  checkButValues();
  // Smooz pot reading
  if(SmoozPot.Update()) {
    checkPotValues();
  }

  updateMidi();
  updateLcd();
}

void checkButValues() {
  bitClear(state, BUTTON_PRESS);
  bitClear(state, INST_CHANGE);
  bitClear(lcd_state, LCD_PLAY_CHANGE);
  bitClear(state, POT_CHANGE);

  byte _current_buttons = SR.Button_SR_Read(0);
  if(buttons != _current_buttons) {
    buttons = _current_buttons;
    bitSet(state, BUTTON_PRESS);

    if(mode == MODE_CONFIG_1) {
      if(bitRead(buttons, BUTTON_BLACK_01)) {
        state ^= 1 << MIDI_MASTER;
        (bitRead(state, MIDI_MASTER)) ? ppqn = MIDI_MASTER_PPQN : ppqn = MIDI_SLAVE_PPQN;

        updateTimerTime();
        return;
      }

      if(bitRead(state, MIDI_MASTER) && bitRead(buttons, BUTTON_BLACK_02)) {
        if(bitRead(buttons, BUTTON_WHITE_LEFT)) {
          bpm--;
        }

        if(bitRead(buttons, BUTTON_WHITE_RIGHT)) {
          bpm++;
        }

        updateTimerTime();
        return;
      }

      if(bitRead(buttons, BUTTON_BLACK_03)) {
        serial_mode = (serial_mode + 1) % 2;
        Serial.begin((uint32_t) pgm_read_dword(&(SERIAL_MODE_VALUES[serial_mode])));
        return;
      }
    }

    if(bitRead(buttons, BUTTON_RED)) {
      if(bitRead(state, MIDI_MASTER)) {
        state ^= 1 << PLAY_PATTERN;
        SR.Led_Pin_Write(LED_RED, bitRead(state, PLAY_PATTERN));
      }
      return;
    }

    if(bitRead(buttons, BUTTON_BLACK)) {
      // Mode Navigation
      if(bitRead(buttons, BUTTON_WHITE_LEFT)) {
        mode = (mode + NB_MODE - 1) % NB_MODE;
        switchOffLcdModeBitmap();
        return;
      }

      if(bitRead(buttons, BUTTON_WHITE_RIGHT)) {
        mode = (mode + 1) % NB_MODE;
        switchOffLcdModeBitmap();
        return;
      }
    }

    // Instrument navigation
    if(bitRead(buttons, BUTTON_WHITE_LEFT)) {
      inst = (inst + NB_INSTS - 1) % NB_INSTS;
      bitSet(state, INST_CHANGE);
      return;
    }

    if(bitRead(buttons, BUTTON_WHITE_RIGHT)) {
      inst = (inst + 1) % NB_INSTS;
      bitSet(state, INST_CHANGE);
      return;
    }

    if(mode == MODE_PLAY) {
      for(register byte _button = BUTTON_BLACK_01; _button > BUTTON_BLACK_03; _button--) {
        if(bitRead(buttons, _button)) {
          bitSet(lcd_state, LCD_PLAY_CHANGE);
          // leave mode bitmap
          bitClear(lcd_state, LCD_MODE_BITMAP);
          SR.Led_Pin_Write(LED_BLACK, bitRead(lcd_state, LCD_MODE_BITMAP));

          if((7 - _button) == lcd_mode_play) {
            SR.Led_Pin_Write(_button, 0); // BUTTON = LED
            lcd_mode_play = LCD_MODE_PLAY_00;
          } 
          else {
            SR.Led_Pin_Write(_button, 0); // BUTTON = LED
            SR.Led_Pin_Write(lcd_mode_play + 5, 1); // BUTTON = LED
            lcd_mode_play = 7 - _button;
          }
          return;
        }   
      }

      if(bitRead(buttons, BUTTON_BLACK) && (!bitRead(buttons, BUTTON_WHITE_LEFT) || !bitRead(buttons, BUTTON_WHITE_RIGHT))) {
        lcd_state ^= 1 << LCD_MODE_BITMAP;
        bitSet(lcd_state, LCD_PLAY_CHANGE);
        SR.Led_Pin_Write(LED_BLACK, bitRead(lcd_state, LCD_MODE_BITMAP));

        if(bitRead(lcd_state, LCD_MODE_BITMAP)) {
          SR.Led_Pin_Write(7 - lcd_mode_play, 0); // BUTTON = LED
          lcd_mode_play = LCD_MODE_PLAY_00;
        }

        return;
      }
    }
  }
}

inline void switchOffLcdModeBitmap() {
  bitClear(lcd_state, LCD_MODE_BITMAP);
  bitSet(lcd_state, LCD_PLAY_CHANGE);
  SR.Led_Pin_Write(LED_BLACK, 0);
}

void checkPotValues() {
  byte _pot_value;
  for(register byte _pot = 0; _pot < NB_PARM; _pot++) {
    _pot_value = SmoozPot.Read(_pot);
    
    if(lcd_mode_play == LCD_MODE_PLAY_00) {
      if(_pot == 0) {
        byte _old_stps = stps[inst];
        if(checkValuePotChange(&stps[inst], _pot_value, MAX_STEP)) {
          // Change scale
          updateScale(inst);
          
          byte _old_puls = puls[inst];
          if(checkValuePotChange(&puls[inst], (puls[inst] * 255 / _old_stps), stps[inst])) {
             checkValuePotChange(&offs[inst], (offs[inst] * 255 / _old_stps), stps[inst]);
             checkValuePotChange(&roll[inst], (roll[inst] * 255 / _old_puls), puls[inst]);
             checkValuePotChange(&roff[inst], (roff[inst] * 255 / _old_puls), puls[inst]);
             checkValuePotChange(&acnt[inst], (acnt[inst] * 255 / _old_puls), puls[inst]);
             checkValuePotChange(&aoff[inst], (aoff[inst] * 255 / _old_puls), puls[inst]);
          }
        }
      }
      else if(_pot == 1) {
        byte _old_puls = puls[inst];
        if(checkValuePotChange(&puls[inst], _pot_value, stps[inst])) {
           checkValuePotChange(&roll[inst], (roll[inst] * 255 / _old_puls), puls[inst]);
           checkValuePotChange(&roff[inst], (roff[inst] * 255 / _old_puls), puls[inst]);
           checkValuePotChange(&acnt[inst], (acnt[inst] * 255 / _old_puls), puls[inst]);
           checkValuePotChange(&aoff[inst], (aoff[inst] * 255 / _old_puls), puls[inst]);
        }
      }
      else if(_pot == 2) {
        checkValuePotChange(&offs[inst], _pot_value, stps[inst]);
      } 
      else if(_pot == 3) {
        checkValuePotChange(&midi_note[inst], _pot_value, 127);
      }
      else if(_pot == 4) {
        checkValuePotChange(&roll[inst], _pot_value, puls[inst]);
      } 
      else if(_pot == 5) {
        checkValuePotChange(&roff[inst], _pot_value, puls[inst]);
      } 
      else if(_pot == 6) {
        checkValuePotChange(&acnt[inst], _pot_value, puls[inst]);
      }
      else if(_pot == 7) {
        checkValuePotChange(&aoff[inst], _pot_value, puls[inst]);
      }

      if(bitRead(state, POT_CHANGE)) {
        euclid(stps[inst], puls[inst], offs[inst], &beat_holder[inst]);
        euclid(puls[inst], roll[inst], roff[inst], &roll_holder[inst]);
        euclid(puls[inst], acnt[inst], aoff[inst], &acnt_holder[inst]);
        computeNote(inst);
        continue;
      }

    }
    else if(lcd_mode_play == LCD_MODE_PLAY_01) { 
      if (_pot == 0) {          
        checkValuePotChange(&arp_scale[inst], _pot_value, OFF);
      }
      else if(_pot == 1) {
        checkValuePotChange(&arp_type[inst], _pot_value, 1);
      }
      else if(_pot == 2) {
        checkValuePotChange(&arp_arp[inst], _pot_value, 1);
      }
      else if(_pot == 4) {
        checkValuePotChange(&arp_step[inst], _pot_value, 7);
      }
      else if(_pot == 5) {
        checkValuePotChange(&arp_oct[inst], _pot_value, 4);
      }
      else if(_pot == 6) {
        if(_pot_value / 128 == 1) {
          if(!BIT_CHK(arp_rnd, inst)) { 
            {
              randomSeed(analogRead(0));
              BIT_SET(arp_rnd, inst);
              bitSet(state, POT_CHANGE);
            }
          } 
          else if(BIT_CHK(arp_rnd, inst)) {
            BIT_CLR(arp_rnd, inst);
            bitSet(state, POT_CHANGE);
          }
        }
      }

      if(bitRead(state, POT_CHANGE)) {
        computeNote(inst);
      }
    }
    else if(lcd_mode_play == LCD_MODE_PLAY_02) {
      if(_pot == 0) {
        if(checkValuePotChange(&rythm_mode[inst], _pot_value, 2)) {
          updateScale(inst);
          continue;
        }
      }
      else if(_pot == 1) {
        if(checkValuePotChange(&swing[inst], _pot_value, scale[inst] / 2)) {
          continue;
        }
      }
      else if(_pot == 2) {
        if(_pot_value / 128 == 1) {
          //if SWG_RND != YES, init random
          if(!BIT_CHK(swg_rnd, inst)) {
            randomSeed(analogRead(0));
            BIT_SET(swg_rnd, inst);
            bitSet(state, POT_CHANGE);
          }
        } 
        else {
          //if SWG_RND == YES
          if(BIT_CHK(swg_rnd, inst)) {
            BIT_CLR(swg_rnd, inst);
            swing_final[inst] = 0;
            bitSet(state, POT_CHANGE);
          }
        }
      }
      else if(_pot == 7) {
        if(checkValuePotChange(&midi_chnl[inst], _pot_value, 12)) {
          continue;
        }
      } 
    }
  }
} 

inline void updateTimerTime() {
  // Timer time = 60  / (BPM * PPQN)
  timer_time = 60000000 / (bpm * ppqn); // 24 ppqn = 2 500 000, 48 = 1 250 000, 96 = 625 000

  // Scale
  for(register byte _inst = 0; _inst < NB_INSTS; _inst++) {
    updateScale(_inst);
  }
}

inline void updateScale(const byte _inst) {
  ((rythm_mode[_inst] == RYTHM_MODE_SOLO) && (stps[_inst] != 0)) 
    ? scale[_inst] = (ppqn * 8) / stps[_inst] // 96 = 24 ppqn x 4 noires = nb ppqm / bar
    : scale[_inst] = ppqn / 4;
}

byte checkValuePotChange(byte *value, const byte _value_pot, const byte _max_value) {
  byte _value = (_value_pot * _max_value) / 255;
  _value = constrain(_value, 0, _max_value);
  if(abs(*value - _value) == 1) {
    *value = _value;
    bitSet(state, POT_CHANGE);
    return true;
  }
  return false;
}

void updateLcd() {
  bitClear(lcd_state, LCD_MODE_CHANGE);

  if(lcd_mode != mode) {
    lcd_mode = mode;
    bitSet(lcd_state, LCD_MODE_CHANGE);    
    bitSet(lcd_state, LCD_PLAY_CHANGE);
  }

  if(lcd_mode == MODE_PLAY) {
    // MODE BITMAP
    if(bitRead(lcd_state, LCD_MODE_BITMAP) ) {
      // Draw the full sequence
      if(bitRead(lcd_state, LCD_MODE_CHANGE) || bitRead(lcd_state, LCD_PLAY_CHANGE) || bitRead(state, POT_CHANGE)) {
        lcd.clear();
        // Draw all steps
        byte _pulse_step = 0;
        for(register byte _step = 0; _step <  stps[inst]; _step++) {
          lcd.setCursor(_step % 16, _step / 16);

          if(BIT_CHK(beat_holder[inst], _step)) {
            lcd.write(byte(getActiveStepChar(_pulse_step)));
            _pulse_step++;
          } 
          else {
            lcd.write(char(95));
          }
        }
      }

      // only draw active and last step
      if((bitRead(state, PLAY_PATTERN) && bitRead(lcd_state, LCD_DRAW_BITMAP) && BIT_CHK(step_chng, inst))) {
        bitClear(lcd_state, LCD_DRAW_BITMAP);
        // current step
        lcd.setCursor(count_step[inst] % 16, count_step[inst] / 16);
        lcd.write(char(219));

        // redraw last step
        byte _last_step = (stps[inst] + count_step[inst] - 1) % stps[inst];
        byte _last_puls = (puls[inst] + count_puls[inst] - 1) % puls[inst];
        if(BIT_CHK(beat_holder[inst], count_step[inst])) {
          _last_puls = (puls[inst] + _last_puls - 1) % puls[inst];
        }
        byte _active_char;
        lcd.setCursor(_last_step % 16, _last_step / 16);
        if(BIT_CHK(beat_holder[inst], _last_step)) {
          _active_char = byte(getActiveStepChar(_last_puls));
          lcd.write(_active_char);
        } 
        else {
          _active_char = 95;
          lcd.write(char(95));
        }
      }

      return;
    }

    //MODE PLAY VALUES LINES
    if(bitRead(state, POT_CHANGE) || bitRead(state, INST_CHANGE) || bitRead(lcd_state, LCD_PLAY_CHANGE)) {
      char _line1[17], _line2[14];

      bitClear(state, INST_CHANGE);

      // INST ST PL OF NT
      //      RL OF AT OF
      if(lcd_mode_play == LCD_MODE_PLAY_00) {
        // LINE 01
        char _midi_note[4];
        sprintf_P(_midi_note,
        (PGM_P) pgm_read_word(&(MIDI_NOTE_LABELS[midi_note[inst] % 12])),
        midi_note[inst] / 12);

        sprintf_P(_line1, (PGM_P) pgm_read_word(&(LCD_LINES_01[LCD_MODE_PLAY_00])), 
        inst + 1, stps[inst], puls[inst], offs[inst], _midi_note);

        // LINE 02 
        sprintf_P(_line2, (PGM_P) pgm_read_word(&(LCD_LINES_02[LCD_MODE_PLAY_00])), 
        roll[inst], roff[inst], acnt[inst], aoff[inst]);
      }
      // INST SCL TYPE ARP 
      //      STP  OCT RND
      else if(lcd_mode_play == LCD_MODE_PLAY_01) {
        // LINE 01
        char _arp_scale[4], _arp_type[4], _arp_arp[4];
        strcpy_P(_arp_scale, (PGM_P) pgm_read_word(&(ARP_SCALE_LABELS[arp_scale[inst]])));
        strcpy_P(_arp_type,  (PGM_P) pgm_read_word(&(ARP_TYPE_LABELS[arp_type[inst]])));
        strcpy_P(_arp_arp,   (PGM_P) pgm_read_word(&(ARP_ARP_LABELS[arp_arp[inst]])));

        sprintf_P(_line1, (PGM_P) pgm_read_word(&(LCD_LINES_01[LCD_MODE_PLAY_01])),
        inst + 1, _arp_scale, _arp_type, _arp_arp);

        // LINE 02
        char _yesno[4];
        byte _arp_rnd = BIT_CHK(arp_rnd, inst);
        strcpy_P(_yesno, (PGM_P) pgm_read_word(&(YESNO_LABELS[_arp_rnd])));
        sprintf_P(_line2, (PGM_P) pgm_read_word(&(LCD_LINES_02[LCD_MODE_PLAY_01])),
        arp_step[inst], arp_oct[inst], _yesno);
      }
      // INST SWG RYT SRND 
      //      CHN
      else if(lcd_mode_play == LCD_MODE_PLAY_02) {
        // LINE 01
        char _rythm_mode[3], _yesno[4];
        byte _swing_rnd = BIT_CHK(swg_rnd, inst);
        strcpy_P(_rythm_mode, (PGM_P) pgm_read_word(&(RYTHM_MODE_LABELS[rythm_mode[inst]])));
        strcpy_P(_yesno, (PGM_P) pgm_read_word(&(YESNO_LABELS[_swing_rnd]))); 

        sprintf_P(_line1, (PGM_P) pgm_read_word(&(LCD_LINES_01[LCD_MODE_PLAY_02])),
        inst + 1, _rythm_mode, swing[inst], _yesno);

        // LINE 02
        sprintf_P(_line2, (PGM_P) pgm_read_word(&(LCD_LINES_02[LCD_MODE_PLAY_02])),
        midi_chnl[inst]);

      }

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(_line1);

      lcd.setCursor(3, 1);
      lcd.print(_line2);
    }

    // Print Current Step
    if(BIT_CHK(step_chng, inst) || bitRead(lcd_state, LCD_MODE_CHANGE) || bitRead(lcd_state, LCD_PLAY_CHANGE) || bitRead(state, POT_CHANGE)) {
      lcd.setCursor(0, 1);
      char _step[2] = {
        '\0'                                                };
      sprintf_P(_step, PSTR("%2u"), count_step[inst] + 1);
      lcd.print(_step);

      lcd.setCursor(2, 1);
      if(BIT_CHK(beat_holder[inst], count_step[inst]) && stps[inst] != 0) {
        // Print "." if active step
        lcd.write(char(46));
      } 
      else {
        // Print " " if inactive step
        lcd.write(char(32));
      }
    }
    return;
  }

  if(lcd_mode == MODE_CONFIG_1) {
    if(bitRead(lcd_state, LCD_MODE_CHANGE) || bitRead(state, BUTTON_PRESS)) {
      char _line_buf[16] = {
        '\0'                                                };  

      strcpy_P(_line_buf, (PGM_P) pgm_read_word(&(LCD_CONFIG_LINES[0])));
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(_line_buf);

      char* _format = (char*) pgm_read_word(&(LCD_CONFIG_LINES[1]));
      char _ser[4], _yesno[4];
      byte _master = bitRead(state, MIDI_MASTER);
      strcpy_P(_yesno, (PGM_P) pgm_read_word(&(YESNO_LABELS[_master])));
      strcpy_P(_ser, (PGM_P) pgm_read_word(&(SERIAL_MODE_LABELS[serial_mode])));
      sprintf_P(_line_buf, _format, _yesno, bpm, _ser);

      lcd.setCursor(0,1);
      lcd.print(_line_buf);
    }
    return;
  }
}

byte getActiveStepChar(const byte _puls) {
  byte _step_char = 0;
  // Apply accent
  if((acnt_holder[inst] != NULL)  
    && BIT_CHK(acnt_holder[inst], _puls)) {
    _step_char++;
  }

  // Apply roll
  if((roll_holder[inst] != NULL) 
    && BIT_CHK(roll_holder[inst], _puls)) {
    _step_char += 2;
  }

  return _step_char;
}

void resetAlltrack() {
  for(register byte _inst = 0; _inst < NB_INSTS; _inst++) {
    if(BIT_CHK(noteOn, _inst)) {
      Send_NoteOFF(_inst);
    }
    count_ppqn[_inst] = 0;
    count_step[_inst] = 0;
    count_puls[_inst] = 0;
    if(puls[_inst] > 0 && BIT_CHK(beat_holder[_inst], 0)) {
      computeNote(_inst);
    }
    // Init all step_chng
    if(_inst % 8 == 0) {
      step_chng[_inst / 8] = B00000000;
    }
  }
}

void updateMidi() {
  if(bitRead(state, BUTTON_PRESS) && bitRead(buttons, BUTTON_RED)) {
    if(bitRead(state, PLAY_PATTERN)) {
      Serial.write(MIDI_START);
      for(register byte  _bank = 0; _bank < NB_INSTS / 8; _bank++) {
        step_chng[_bank] = B11111111;
      }
      bitSet(state, UPDATED_PPQN);
      bitSet(lcd_state, LCD_DRAW_BITMAP);
    } 
    else {
      Serial.write(MIDI_STOP);
      resetAlltrack();
    }
  }

  if(bitRead(state, PLAY_PATTERN) && bitRead(state, UPDATED_PPQN)) {
    for(register byte _inst = 0; _inst < NB_INSTS; _inst++) {

      if(puls[_inst] > 0 && BIT_CHK(step_chng, _inst)) {
        if(BIT_CHK(noteOn, _inst)) {
          Send_NoteOFF(_inst);
        }

        // Play step
        if(BIT_CHK(beat_holder[_inst], count_step[_inst])) {
          Send_NoteON(_inst);
          count_puls[_inst] = (count_puls[_inst] + 1) % puls[_inst];
        }
      }
    }
    bitClear(state, UPDATED_PPQN);
  }
}

inline void Send_NoteON(const byte _inst) {
  last_midi_note[_inst] = next_midi_note[_inst];

  Serial.write(144 + midi_chnl[_inst] - 1);//Note ON on selected channel
  Serial.write(last_midi_note[_inst]);
  Serial.write(next_midi_velo[_inst]);

  // Turn on inst led
  if(_inst == inst) {
    SR.Led_Pin_Write(LED[((count_puls[_inst] + 1) % 2) + 4], 1);
  }

  // Note On
  BIT_SET(noteOn, _inst);
}

inline void Send_NoteOFF(const byte _inst) {
  Serial.write (128 + midi_chnl[_inst] - 1);//Note OFF on selected channel
  Serial.write(last_midi_note[_inst]);
  Serial.write (0x01);//velocite 0

  // Turn off inst led
  if(_inst == inst) {
    SR.Led_Pin_Write(LED[((count_puls[_inst] + 1) % 2) + 4], 0);
  }

  // Note Off
  BIT_CLR(noteOn, _inst);
}

//////////////////////////////////////////////////////////////////
//This function is call by the timer depending Sync mode and BPM//
//////////////////////////////////////////////////////////////////
void Count_PPQN() {
  //-----------------Sync MASTER-------------------//  
  if(bitRead(state, MIDI_MASTER)) {
    updateTimerTime();
    // Send Midi Clock on 24 PPQN
    total_timer_time += timer_time;
    //if((total_timer_time >= (2500000 / bpm) - 16) { // MIDI Clock on 24 ppqn
    if((total_timer_time * bpm) >= 2500000) { // MIDI Clock on 24 ppqn
      Serial.write(MIDI_CLOCK);
      total_timer_time = 0;
    }
    if(bitRead(state, PLAY_PATTERN)) {
      update_all_count_ppqn();
    }
  }

  //-----------------Sync SLAVE------------------//
  else {
    timer_time=5000;
    if(Serial.available() > 0) {
      byte data = Serial.read();
      if(data == MIDI_START) {
        bitSet(state, PLAY_PATTERN);
      }
      else if(data == MIDI_STOP ) {
        bitClear(state, PLAY_PATTERN);
        resetAlltrack();
      }
      else if(data == MIDI_CLOCK && bitRead(state, PLAY_PATTERN)) {
        int time = millis();
        bpm = (byte) (time - old_clock_time) / MIDI_SLAVE_PPQN * 60000;
        old_clock_time = time;
        update_all_count_ppqn();
      }
    }
  }
}

void update_all_count_ppqn() {
  bitSet(state, UPDATED_PPQN);
  for(register byte _inst = 0; _inst < NB_INSTS; _inst++) {
    if(puls[_inst] > 0) {
      // Loop to the start of sequence
      if(count_ppqn[_inst] >= ((stps[_inst] * scale[_inst]) - 1)) {
        count_ppqn[_inst] = 0;
        count_puls[_inst] = 0;
      } 
      else {
        count_ppqn[_inst]++;
      }

      byte _current_step = count_ppqn[_inst] / scale[_inst]; //straight
      // Apply swing
      // Delay the second 16th note within each 8th note.
      // ie. delay all the even-numbered 16th notes within the beat (2, 4, 6, 8, etc.)
      if(swing[_inst] != 0 
        && (((_current_step + 1) % 4) == 0)) {
        _current_step = (count_ppqn[_inst] - swing_final[_inst]) / scale[_inst];
      }

      // New step
      if(count_step[_inst] != _current_step) {
        BIT_SET(step_chng, _inst);
        bitSet(lcd_state, LCD_DRAW_BITMAP);

        (swing[_inst] != 0 && BIT_CHK(swg_rnd, _inst))
          ? swing_final[inst] = swing[_inst] - random(0, swing[_inst])
          : swing_final[_inst] = swing[_inst];
        
        count_step[_inst] = _current_step % stps[_inst];
        if(BIT_CHK(beat_holder[_inst], count_step[_inst])) {
          computeNote(_inst);
        }
      } 
      else {
        BIT_CLR(step_chng, _inst);

        // Half step
        if((count_ppqn[_inst] % scale[_inst]) == (scale[_inst] / 2)) {
          // Apply roll on old active step
          byte _last_puls = (puls[_inst] + count_puls[_inst] - 1) % puls[_inst];
          if(roll_holder[_inst] != NULL && BIT_CHK(noteOn, _inst) && BIT_CHK(roll_holder[_inst], _last_puls)) {
            // Replay step  
            BIT_SET(step_chng, _inst);
            count_puls[_inst] = _last_puls;
          }
        }
      }
    }
  }
}

void computeNote(const byte _inst) {
  // Compute next note and velo
  if(puls[_inst] > 0) {
    // Compute midi note
    int8_t _midi_note = midi_note[_inst];

    // Apply ARP
    if(arp_scale[_inst] != OFF && arp_step[_inst] > 1 && arp_oct[_inst] > 0) {
      // SCALE
      int8_t _scale_mod = 0;
      byte* _scale_mods = (byte*) malloc(arp_step[_inst]);
      memcpy_P(_scale_mods, (PGM_P) pgm_read_dword(&(ARP_SCALES[arp_scale[_inst]])), arp_step[_inst]);

      int8_t _arp_puls, _arp_oct;
      // RANDOM
      if(BIT_CHK(arp_rnd, _inst)) {
        _arp_puls = random(0, 7) % arp_step[_inst];
        // FULL STRAIGHT
      } 
      else if(arp_type[_inst] == ARP_ARP_UP) {
        _arp_puls = count_puls[_inst] % arp_step[_inst];
      }

      // TYPE FULL
      if(arp_type[_inst] == ARP_TYPE_FULL) {
        _arp_oct = 12 * ((count_puls[_inst] / min(arp_step[_inst], 8) % arp_oct[_inst]));

        if(arp_arp[_inst] == ARP_ARP_UP) {
          // 0, 1, 2, 3, 4, 5, 6, 0, 1, 2...
          _scale_mod = _scale_mods[_arp_puls] + _arp_oct;
        } 
        else if (arp_arp[_inst] == ARP_ARP_DOWN) {
          // 0, 6, 5, 4, 3, 2, 1, 0, 6, 5, ...
          _arp_puls   = (14 - (_arp_puls)) % 7;
          _scale_mod -= _scale_mods[_arp_puls] + _arp_oct;
        }
      }
      // TYPE 7TH 
      else if(arp_type[_inst] == ARP_TYPE_7TH) {
        _arp_oct = 12 * ((count_puls[_inst] / min(arp_step[_inst], 4) % arp_oct[_inst]));

        if(arp_arp[_inst] == ARP_ARP_UP) {
          // 0, 2, 4, 6, 0, 2, 4...
          _arp_puls = 2 * (_arp_puls % 4);
          _scale_mod = _scale_mods[_arp_puls] + _arp_oct;
        } 
        else if (arp_arp[_inst] == ARP_ARP_DOWN) {
          // 0, 6, 4, 2, 0, 6, 4...
          _arp_puls   = (16 - (2 * (_arp_puls % 4))) % 8;
          _scale_mod -= _scale_mods[_arp_puls] + _arp_oct;
        }
      } 

      free(_scale_mods);

      _midi_note += _scale_mod;
    }
    next_midi_note[_inst] = min(_midi_note, 127);
    next_midi_note[_inst] = max(_midi_note, 0);

    // Compute midi velo
    (acnt_holder[_inst] != NULL && BIT_CHK(acnt_holder[_inst], count_puls[_inst])) 
      ? next_midi_velo[_inst] = 127
      : next_midi_velo[_inst] = DEFAULT_MIDI_VELO;
  }
}

// Euclid calculation function
uint8_t eucl_pos;
int8_t eucl_remainder[MAX_STEP];
int8_t eucl_count[MAX_STEP];
void euclid(const byte _step, const byte _pulse, const byte _offs, byte* *_tab) {
  // Init tab
  free(*_tab);
  if(_step > 0) {
    *_tab = (byte*) malloc((_step / 8) + 1);
    for(register byte _i = 0; _i < _step; _i++) {
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

void build(const int8_t _level, const byte _step, const byte _offs, uint8_t* _tab) {
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
    for(register byte _p = 0; _p < eucl_count[_level]; _p++) {
      build(_level-1, _step, _offs, _tab); 
    }

    if(eucl_remainder[_level] != 0) {
      build(_level-2, _step, _offs, _tab); 
    }
  }
}
