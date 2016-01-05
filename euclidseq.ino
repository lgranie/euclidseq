/*
* Author:       Laurent Granié
 * Copyright:    (C) 2014 Laurent Granié
 * Licence:      GNU General Public Licence version 3
 */
#include <avr/pgmspace.h>

#include <TimerOne.h>
#include <SmoozPotMUX.h>
#include <SRIO.h>
#include <LiquidCrystal.h>

/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) (a[(b>>3)] |= 1 << (b&7))  // b/8 == i>>3, b%8 == b&7
#define BIT_CLR(a,b) (a[(b>>3)] &= ~(1 << (b&7)))
#define BIT_FLP(a,b) (a[(b>>3)] ^= 1 << (b&7))
#define BIT_CHK(a,b) (a[(b>>3)] & (1 << (b&7)))
#define BIT_BYT(a,b) !!(a[(b>>3)] & (1 << (b&7)))

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

byte inst = 0;

//Mode define
#define MODE_CONFIG_1 0
#define MODE_PLAY     1
#define NB_MODE     2

byte mode = MODE_PLAY;

// Serial Mode
const char SERIAL_MODE_LABELS[2][4] PROGMEM = { 
 { "Mid" }, { "Usb" }
};
const uint32_t SERIAL_MODE_VALUES[2] PROGMEM = {
  31250, 115200
};
#define SERIAL_MODE_MIDI   0
#define SERIAL_MODE_USB    1
byte serial_mode = SERIAL_MODE_USB;

// Rythm Mode
const char RYTHM_MODE_LABELS[2][4] PROGMEM = {
  { "Pol" }, { "Sol" }
};
#define RYTHM_MODE_POLY 0
#define RYTHM_MODE_SOLO 1
byte rythm_mode[NB_INSTS];

// Lcd
LiquidCrystal lcd(2,3,4,5,6,7);

#define LCD_MODE_BITMAP 0
#define LCD_PLAY_CHANGE 1
#define LCD_DRAW_BITMAP 2
#define LCD_MODE_CHANGE 3
byte lcd_state = B00000010;

const byte BITMAP_CHARS[4][8] PROGMEM = {
  {B00000, B00000, B00100, B00100, B00100, B00100, B11111, B00000}, 
  {B00100, B00100, B00100, B00100, B00100, B00100, B11111, B00000},
  {B00000, B00000, B01010, B01010, B01010, B01010, B11111, B00000}, 
  {B01010, B01010, B01010, B01010, B01010, B01010, B11111, B00000}
};


/////////////////////////////////////////////////////////
// ARPEGGIO
/////////////////////////////////////////////////////////
const char ARP_SCALES[9][7] PROGMEM = {
  {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 4, 5, 7, 9, 11},
  {0, 1, 3, 5, 7, 8, 10}, {0, 2, 4, 6, 7, 9, 11}, {0, 2, 4, 5, 7, 9, 10}, 
  {0, 2, 3, 5, 7, 8, 10}, {0, 1, 3, 5, 6, 8, 10}, {0, 2, 3, 4, 7, 9, 12}
};

const char ARP_SCALE_LABELS[10][4] PROGMEM = { 
  {"Maj"}, {"Min"}, {"Ion"}, 
  {"Dor"}, {"Phr"}, {"Lyd"}, 
  {"Mix"}, {"Aeo"}, {"Blu"}, 
  {"Off"}
};

#define OFF 9

#define ARP_TYPE_FULL 0
#define ARP_TYPE_7TH  1
const char ARP_TYPE_LABELS[3][4] PROGMEM = {
  {"Ful"}, {"7th"}
};

#define ARP_ARP_UP  0
#define ARP_ARP_DWN 1
const char ARP_ARP_LABELS[2][4] PROGMEM = {
  {" Up"}, {"Dwn"}
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

const char MIDI_NOTE_LABELS[12][5] PROGMEM = {
  {"C%d"},  {"C%d#"}, {"D%d"},  {"D%d#"}, 
  {"E%d"},  {"F%d"},  {"F%d#"}, {"G%d"}, 
  {"G%d#"}, {"A%d"},  {"A%d#"}, {"B%d"}
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

const char LCD_CONFIG_LINES[2][18] PROGMEM = {
  {"MST BPM BUS      "}, 
  {"%3s %3u %3s  "}
};

const char LCD_LINES_01[3][21] PROGMEM = {
  {"%2u %3s %3s %3s  "}, 
  {"%2u %3s Sw%2u %3s "}, 
  {"%2u %2u %2u %2u %-4s"}
};

const char LCD_LINES_02[3][17] PROGMEM = {
  {"Nt%u Oc%u %3s "}, 
  {"        Ch%2u"}, 
  {"%2u %2u %2u %2u "}
};

byte lcd_mode = MODE_PLAY;
byte lcd_mode_play = LCD_MODE_PLAY_00;

// Buttons et LED
#define BUTTON_BLACK_01    0
#define BUTTON_BLACK_02    1
#define BUTTON_BLACK_03    2
#define BUTTON_BLACK_04    3
#define BUTTON_RED         4
#define BUTTON_BLACK       5
#define BUTTON_WHITE_LEFT  6
#define BUTTON_WHITE_RIGHT 7

byte buttons;

const char YESNO_LABELS[2][4] PROGMEM = {
  {" No"}, {"Yes"}
};

//--------------------------------------------SETUP----------------------------------------------//
void setup() {
  // Attach callback function to Timer1
  Timer1.attachInterrupt(Count_PPQN);

  //initialize special character
  lcd.begin(16,2);
  byte *_buffer = (byte*) malloc(8);
  for(register byte _i = 0; _i < 4; _i++) {
    memcpy_P(_buffer, BITMAP_CHARS[_i], 8);
    lcd.createChar(_i, _buffer);
  }
  free(_buffer);
  lcd.clear();
  
  // Serial init
  Serial.begin(pgm_read_dword(&(SERIAL_MODE_VALUES[serial_mode])));
  
  // Init Pot
  SmoozPot.Initialize();

  // Init Buttons
  SR.Initialize();
  SR.Led_All_On();

  step_chng = (byte*) malloc(NB_INSTS >> 3); // x/8 == x>>3
  noteOn    = (byte*) malloc(NB_INSTS >> 3);
  arp_rnd   = (byte*) malloc(NB_INSTS >> 3);
  swg_rnd   = (byte*) malloc(NB_INSTS >> 3);

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

    arp_scale[_inst] = OFF;
    arp_type[_inst]  = ARP_TYPE_FULL;
    arp_arp[_inst]   = ARP_ARP_UP;
    arp_step[_inst]  = 0;
    arp_oct[_inst]   = 1;

    rythm_mode [_inst] = RYTHM_MODE_POLY;
    scale[_inst] = ppqn >> 2; // x/4 == x >> 2
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
        serial_mode = (serial_mode + 1) & 0xffff; // x&1 == x%2
        Serial.begin(pgm_read_dword(&(SERIAL_MODE_VALUES[serial_mode])));
        return;
      }
    }

    if(bitRead(buttons, BUTTON_RED)) {
      if(bitRead(state, MIDI_MASTER)) {
        state ^= 1 << PLAY_PATTERN;
        SR.Led_Pin_Write(BUTTON_RED, bitRead(state, PLAY_PATTERN));
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
      for(register byte _button = BUTTON_BLACK_01; _button < BUTTON_BLACK_03; _button++) {
        if(bitRead(buttons, _button)) {
          bitSet(lcd_state, LCD_PLAY_CHANGE);
          // leave mode bitmap
          bitClear(lcd_state, LCD_MODE_BITMAP);
          SR.Led_Pin_Write(BUTTON_BLACK, bitRead(lcd_state, LCD_MODE_BITMAP));

          if(_button == lcd_mode_play) {
            SR.Led_Pin_Write(lcd_mode_play, 0); // BUTTON = LED
            lcd_mode_play = LCD_MODE_PLAY_00;
          } 
          else {
            SR.Led_Pin_Write(lcd_mode_play, 0); // BUTTON = LED
            lcd_mode_play =  _button;
            SR.Led_Pin_Write(lcd_mode_play, 1); // BUTTON = LED
          }
          return;
        }   
      }

      if(bitRead(buttons, BUTTON_BLACK) && (!bitRead(buttons, BUTTON_WHITE_LEFT) || !bitRead(buttons, BUTTON_WHITE_RIGHT))) {
        lcd_state ^= 1 << LCD_MODE_BITMAP;
        bitSet(lcd_state, LCD_PLAY_CHANGE);
        SR.Led_Pin_Write(BUTTON_BLACK, bitRead(lcd_state, LCD_MODE_BITMAP));

        if(bitRead(lcd_state, LCD_MODE_BITMAP)) {
          SR.Led_Pin_Write(lcd_mode_play, 0); // BUTTON = LED
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
  SR.Led_Pin_Write(BUTTON_BLACK, 0);
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
        }
      }
      else if(_pot == 1) {
        checkValuePotChange(&swing[inst], _pot_value, scale[inst] / 2);
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
        checkValuePotChange(&midi_chnl[inst], _pot_value, 12);
      } 
    }
    
    // Return on first pot change
    if(bitRead(state, POT_CHANGE)) {
      return;
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
          lcd.setCursor(_step & 15, _step >> 4); // x%16 == x&15 i>>4 == i / 16

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
        lcd.setCursor(count_step[inst] & 15, count_step[inst] >> 4);
        lcd.write(char(219));

        // redraw last step
        byte _last_step = (stps[inst] + count_step[inst] - 1) % stps[inst];
        byte _last_puls = (puls[inst] + count_puls[inst] - 1) % puls[inst];
        if(BIT_CHK(beat_holder[inst], count_step[inst])) {
          _last_puls = (puls[inst] + _last_puls - 1) % puls[inst];
        }
        byte _active_char;
        lcd.setCursor(_last_step & 15, _last_step >> 4);
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
        sprintf_P(_midi_note, MIDI_NOTE_LABELS [midi_note[inst] % 12], midi_note[inst] / 12);

        sprintf_P(_line1, LCD_LINES_01[LCD_MODE_PLAY_00], 
          inst + 1, stps[inst], puls[inst], offs[inst], _midi_note);
        
        // LINE 02 
        sprintf_P(_line2, LCD_LINES_02[LCD_MODE_PLAY_00], 
        roll[inst], roff[inst], acnt[inst], aoff[inst]);
      }
      // INST SCL TYPE ARP 
      //      STP  OCT RND
      else if(lcd_mode_play == LCD_MODE_PLAY_01) {
        // LINE 01
        char _arp_scale[4], _arp_type[4], _arp_arp[4];
        strcpy_P(_arp_scale, ARP_SCALE_LABELS[arp_scale[inst]]);
        strcpy_P(_arp_type,  ARP_TYPE_LABELS[arp_type[inst]]);
        strcpy_P(_arp_arp,   ARP_ARP_LABELS[arp_arp[inst]]);

        sprintf_P(_line1, LCD_LINES_01[LCD_MODE_PLAY_01],
          inst + 1, _arp_scale, _arp_type, _arp_arp);

        // LINE 02
        char _yesno[4];
        byte _arp_rnd = BIT_CHK(arp_rnd, inst);
        strcpy_P(_yesno,  YESNO_LABELS[_arp_rnd]);
        sprintf_P(_line2, LCD_LINES_02[LCD_MODE_PLAY_01],
          arp_step[inst], arp_oct[inst], _yesno);
      }
      // INST SWG RYT SRND 
      //      CHN
      else if(lcd_mode_play == LCD_MODE_PLAY_02) {
        // LINE 01
        char _rythm_mode[3], _yesno[4];
        byte _swing_rnd = BIT_CHK(swg_rnd, inst);
        strcpy_P(_rythm_mode, RYTHM_MODE_LABELS[rythm_mode[inst]]);
        strcpy_P(_yesno,      YESNO_LABELS[_swing_rnd]); 

        sprintf_P(_line1, LCD_LINES_01[LCD_MODE_PLAY_02],
          inst + 1, _rythm_mode, swing[inst], _yesno);

        // LINE 02
        sprintf_P(_line2, LCD_LINES_02[LCD_MODE_PLAY_02],
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
      char _step[3];
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
      char _line_buf[18];  

      strcpy_P(_line_buf, LCD_CONFIG_LINES[0]);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(_line_buf);

      char _ser[4], _yesno[4];
      byte _master = bitRead(state, MIDI_MASTER);
      strcpy_P(_yesno, YESNO_LABELS[_master]);
      strcpy_P(_ser,   SERIAL_MODE_LABELS[serial_mode]);
      
      sprintf_P(_line_buf, LCD_CONFIG_LINES[1], _yesno, bpm, _ser);

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
    if((_inst & 7) == 0) {
      step_chng[(_inst >> 3)] = B00000000;
    }
  }
}

void updateMidi() {
  if(bitRead(state, BUTTON_PRESS) && bitRead(buttons, BUTTON_RED)) {
    if(bitRead(state, PLAY_PATTERN)) {
      Serial.write(MIDI_START);
      for(register byte  _bank = 0; _bank < (NB_INSTS >> 3); _bank++) {
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
  Serial.write(144 + midi_chnl[_inst] - 1);//Note ON on selected channel
  Serial.write(next_midi_note[_inst]);
  Serial.write(next_midi_velo[_inst]);

  last_midi_note[_inst] = next_midi_note[_inst];
  
  // Turn on inst led
  if(_inst == inst) {
    SR.Led_Pin_Write((count_puls[_inst] & 1) + 6, 1);
  }

  // Note On
  BIT_SET(noteOn, _inst);
}

inline void Send_NoteOFF(const byte _inst) {
  Serial.write(128 + midi_chnl[_inst] - 1);//Note OFF on selected channel
  Serial.write(last_midi_note[_inst]);
  Serial.write(0x01);//velocite 0

  // Turn off inst led
  if(_inst == inst) {
    SR.Led_Pin_Write((count_puls[_inst] & 1) + 6, 0);
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
        && (((_current_step + 1) & 3) == 0)) {
        _current_step = (count_ppqn[_inst] - swing_final[_inst]) / scale[_inst];
      }

      // New step
      if(count_step[_inst] != _current_step) {
        BIT_SET(step_chng, _inst);
        bitSet(lcd_state, LCD_DRAW_BITMAP);

        (swing[_inst] != 0 && BIT_CHK(swg_rnd, _inst))
          ? swing_final[inst] = swing[_inst] - random(0, (swing[_inst] >> 1))
          : swing_final[_inst] = swing[_inst];
        
        count_step[_inst] = _current_step % stps[_inst];
        if(BIT_CHK(beat_holder[_inst], count_step[_inst])) {
          computeNote(_inst);
        }
      } 
      else {
        BIT_CLR(step_chng, _inst);

        // Half step
        if((count_ppqn[_inst] % scale[_inst]) == (scale[_inst] >> 1)) {
          // Apply roll on old active step
          if(roll_holder[_inst] != NULL && BIT_CHK(noteOn, _inst)) {
            byte _last_puls = (puls[_inst] + count_puls[_inst] - 1) % puls[_inst];
            if(BIT_CHK(roll_holder[_inst], _last_puls)) {
              // Replay step  
              BIT_SET(step_chng, _inst);
              count_puls[_inst] = _last_puls;
            }
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
      memcpy_P(_scale_mods, ARP_SCALES[arp_scale[_inst]], arp_step[_inst]);

      int8_t _arp_puls, _arp_oct;
      // RANDOM
      if(BIT_CHK(arp_rnd, _inst)) {
        _arp_puls = random(0, 7) % arp_step[_inst];
      } 
      // FULL STRAIGHT
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
        else if (arp_arp[_inst] == ARP_ARP_DWN) {
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
          _arp_puls = 2 * (_arp_puls & 3);
          _scale_mod = _scale_mods[_arp_puls] + _arp_oct;
        } 
        else if (arp_arp[_inst] == ARP_ARP_DWN) {
          // 0, 6, 4, 2, 0, 6, 4...
          _arp_puls   = (16 - (2 * (_arp_puls & 3))) & 7;
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
    *_tab = (byte*) malloc(_step >> 3);
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
