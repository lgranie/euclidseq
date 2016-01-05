/*
 * Author:       Laurent Granié
 * Copyright:    (C) 2015 Laurent Granié
 * Licence:      GNU General Public Licence version 3
 */
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

#include <TimerOne.h>
#include <SmoozPotMUX.h>
#include <SRIO.h>
#include <LiquidCrystal.h>

/* a=target variable, b=bit number to act upon 0-n */
#define _SET(a,b) (a[(b>>3)] |= 1 << (b&7))  // b/8 == i>>3, b%8 == b&7
#define _CLR(a,b) (a[(b>>3)] &= ~(1 << (b&7)))
#define _FLP(a,b) (a[(b>>3)] ^= 1 << (b&7))
#define _CHK(a,b) (a[(b>>3)] & (1 << (b&7)))
#define _BYT(a,b) !!(a[(b>>3)] & (1 << (b&7)))

#define NB_PARM  8
#define NB_INSTS 16 // NB_BANK x NB_INST
#define MAX_STEP 32

#define INST_CHANGE     0
#define PLAY_PATTERN    1
#define MIDI_MASTER     2
#define UPDATED_PPQN    3
#define BTN_PRESS       4
#define POT_CHANGE      5
uint8_t state = B00000101;

//Mode define
#define MODE_CONFIG_1 0
#define MODE_PLAY     1
#define NB_MODE       2
uint8_t mode = MODE_PLAY;

// Serial Mode
const char SERIAL_MODE_LABELS[2][4] PROGMEM = { 
 { "Mid" }, { "Usb" }
};
const uint32_t SERIAL_MODE[2] PROGMEM = {
  31250, 115200
};
#define SERIAL_MODE_MIDI   0
#define SERIAL_MODE_USB    1
uint8_t serial_mode = SERIAL_MODE_USB;

// Rythm Mode
const char RYTHM_MODE_LABELS[2][4] PROGMEM = {
  { "Pol" }, { "Sol" }
};
#define RYTHM_MODE_POLY 0
#define RYTHM_MODE_SOLO 1

// Lcd
LiquidCrystal lcd(2,3,4,5,6,7);

#define LCD_MODE_BITMAP 0
#define LCD_PLAY_CHANGE 1
#define LCD_DRAW_BITMAP 2
#define LCD_MODE_CHANGE 3
uint8_t lcd_state = B00000010;

const uint8_t BITMAP_CHARS[6][8] PROGMEM = {
  {B00000, B00000, B00100, B00100, B00100, B00100, B11111, B00000}, 
  {B00100, B00100, B00100, B00100, B00100, B00100, B11111, B00000},
  {B00000, B00000, B01010, B01010, B01010, B01010, B11111, B00000}, 
  {B01010, B01010, B01010, B01010, B01010, B01010, B11111, B00000},
  {B00000, B00000, B00000, B00000, B00100, B01000, B11111, B01000}, 
  {B00000, B00000, B00000, B00000, B00100, B00010, B11111, B00010}
};

//-----MIDI-----//            
uint8_t bpm = 120; //Default BPM

#define MIDI_MASTER_PPQN 96
#define MIDI_SLAVE_PPQN  24
uint8_t ppqn = MIDI_MASTER_PPQN; // 24, 96, (348?)

//Time in microsecond of the callback fuction
uint16_t old_clock_time = 0;
uint16_t timer_time = 5000;
uint32_t total_timer_time = 0;

//Midi message define
#define MIDI_START 0xfa
#define MIDI_STOP  0xfc
#define MIDI_CLOCK 0xf8

#define DEFAULT_MIDI_VELO 100

typedef struct {
  uint8_t offs = 0;
  uint8_t roll = 0;
  uint8_t roff = 0;
  uint8_t acnt = 0;
  uint8_t aoff = 0;

  uint8_t arp_mod = 0;
  uint8_t arp_alt = 0;
  uint8_t arp_aof = 0;
} PTRACK;

// Current inst
uint8_t  inst_index = 0;
PTRACK  pinst;

// Used data
#define ARP_RND 0
#define STP_CHG 1
#define NOTE_ON 2
typedef struct {
  uint8_t state = B00000000;
  
  uint8_t stps = 0;
  uint8_t puls = 0;
  
  uint8_t rythm_mode = RYTHM_MODE_POLY;
  uint8_t swing = 0;
  
  uint8_t note = 33;
  uint8_t chnl = 3;
  
  uint8_t arp_ton = 0;
  uint8_t arp_oct = 0;
  uint8_t arp_typ = 0;
  
} PMTRACK;
PMTRACK pminsts[NB_INSTS];

typedef struct {
  int8_t   count_step;
  int8_t   count_puls;
  uint16_t count_ppqn;
  
  uint8_t scale = 24;
  
  uint8_t *beat_holder;
  uint8_t *roll_holder;
  uint8_t *acnt_holder;
  uint8_t *arp_holder;
  
  uint8_t arp_line[13] = {32};
  
  uint8_t last_note;
  uint8_t next_note;
  uint8_t next_velo;
} MTRACK;
MTRACK minsts[NB_INSTS];

#define LCD_00 0 // DEFAULT SCREEN
#define LCD_01 1 // BTN_BLACK_01
#define LCD_02 2 // BTN_BLACK_02
#define NB_LCD 3

const char LCD_CONFIG_LINES[2][18] PROGMEM = {
  {"MST BPM BUS      "}, 
  {"%3s %3u %3s  "}
};

const char LCD_LINES_01[3][22] PROGMEM = {
// INST ST  PL  OF  NT
  {"%2u %2u %2u %2u %-4s"},
// INST RYT SWG   CHN
  {"%2u %3s %2u   %2u  "}, 
// INST TON MOD ALT AOF
  {"%2u %2u %2u %2u %2u  "}
};

const char LCD_LINES_02[3][18] PROGMEM = {
//  RL  OF  AT  OF
  {"%2u %2u %2u %2u  "}, 
// 
  {"             "},
//  OCT TYP
  {"%2u %3s       "}
};

const char MIDI_NOTE[12][5] PROGMEM = {
  {"C%d"},  {"C%d#"}, {"D%d"},  {"D%d#"}, 
  {"G%d#"}, {"A%d"},  {"A%d#"}, {"B%d"}, 
  {"E%d"},  {"F%d"},  {"F%d#"}, {"G%d"}
};

const char LABELS[2][4] PROGMEM = {
  {" No"}, {"Yes"}, 
};
const char ARP_TYPES[4][4] PROGMEM = {
  {" Up"}, {"Dwn"}, {"UDw"}, {"DwU"} 
};

uint8_t lcd_mode      = MODE_PLAY;
uint8_t lcd_mode_play = LCD_00;

// Buttons et LED
#define BTN_BLACK_00    0
#define BTN_BLACK_01    1
#define BTN_BLACK_02    2
#define BTN_BLACK_03    3
#define BTN_RED         4
#define BTN_BLACK       5
#define BTN_WHITE_LEFT  6
#define BTN_WHITE_RIGHT 7

uint8_t buttons;

//----------------SETUP----------------//
void setup() {
  // Attach callback function to Timer1
  Timer1.attachInterrupt(Count_PPQN);

  //initialize special character
  lcd.begin(16,2);
  uint8_t *_buffer = (uint8_t*) malloc(8);
  for(uint8_t _i = 0; _i < 6; _i++) {
    memcpy_P(_buffer, BITMAP_CHARS[_i], 8);
    lcd.createChar(_i, _buffer);
  }
  free(_buffer);
  lcd.clear();
  
  // Serial init
  Serial.begin((uint32_t) pgm_read_dword(&(SERIAL_MODE[serial_mode])));
  
  // Init Pot
  SmoozPot.Initialize();

  // Init Buttons
  SR.Initialize();
  SR.Led_All_On();
  
  // Init minsts & pminsts
  for(uint8_t _i = 0; _i < NB_INSTS; _i++) {
    minsts[_i].count_step = 0;
    minsts[_i].count_puls = 0;
    minsts[_i].count_ppqn = 0;
  
    minsts[_i].scale = ppqn / 4;
  
    minsts[_i].beat_holder = NULL;
    minsts[_i].roll_holder = NULL;
    minsts[_i].acnt_holder = NULL;
    minsts[_i].arp_holder  = NULL;
  
    minsts[_i].last_note = 0;
    minsts[_i].next_note = 0;
    minsts[_i].next_velo = 0;
    
    pminsts[_i].state = B00000000;
  
    pminsts[_i].stps = 0;
    pminsts[_i].puls = 0;
  
    pminsts[_i].rythm_mode = RYTHM_MODE_POLY;
  
    pminsts[_i].swing = 0;
  
    pminsts[_i].note = 33;
    pminsts[_i].chnl = 3;
    pminsts[_i].arp_ton = 0;
  }
  
  // Init all eeprom to default
//  for(inst_index = 0; inst_index < NB_INSTS; inst_index++) {
//    save_instrument();
//  }
  
  inst_index = 0;
  load_instrument();

  delay(100);
  SR.Led_All_Off();
}
//--------------SETUP------------------------//

void load_instrument() {
  PMTRACK *_pminst = &(pminsts[inst_index]);
  MTRACK  *_minst  = &(minsts[inst_index]);
  
  // Load PTRACK in pinst
  eeprom_busy_wait();
  eeprom_read_block(
    (void*) &pinst, 
    (const void*) (inst_index * sizeof(PTRACK)), 
    sizeof(PTRACK));
    
  // Load PMTRACK in pinst.pminst
  eeprom_busy_wait();
  eeprom_read_block(
    (void*) _pminst, 
    (const void*) (NB_INSTS * sizeof(PTRACK) + inst_index * sizeof(PMTRACK)),
    sizeof(PMTRACK));
  
  euclid(_pminst->stps, _pminst->puls, pinst.offs, &(_minst->beat_holder));
  euclid(_pminst->puls, pinst.roll,    pinst.roff, &(_minst->roll_holder));
  euclid(_pminst->puls, pinst.acnt,    pinst.aoff, &(_minst->acnt_holder));
  
  euclid_arp(
    _pminst->arp_ton, pinst.arp_mod, pinst.arp_alt, pinst.arp_aof, 
    &(_minst->arp_holder), _minst->arp_line);
  
  // Update 
  computeNote(inst_index);

  updateScale(inst_index);

  bitClear(_pminst->state, STP_CHG);
  bitClear(_pminst->state, NOTE_ON);
  
  bitSet(state, INST_CHANGE);
}

void save_instrument() {
  // Load PTRACK in pinst
  eeprom_busy_wait();
  eeprom_update_block(
    (const void*) &pinst, 
    (void*) (inst_index * sizeof(PTRACK)),
    sizeof(PTRACK));
  
  // Load PMTRACK in pinst.pminst
  eeprom_busy_wait();
  eeprom_update_block( 
    (const void*) &(pminsts[inst_index]),
    (void*) (NB_INSTS * sizeof(PTRACK) + inst_index * sizeof(PMTRACK)), 
    sizeof(PMTRACK));
}

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
  bitClear(state, BTN_PRESS);
  bitClear(state, INST_CHANGE);
  bitClear(lcd_state, LCD_PLAY_CHANGE);
  bitClear(state, POT_CHANGE);

  uint8_t _current_buttons = SR.Button_SR_Read(0);
  if(buttons != _current_buttons) {
    buttons = _current_buttons;
    bitSet(state, BTN_PRESS);

    if(mode == MODE_CONFIG_1) {
      if(bitRead(buttons, BTN_BLACK_00)) {
        state ^= 1 << MIDI_MASTER;
        (bitRead(state, MIDI_MASTER)) 
          ? ppqn = MIDI_MASTER_PPQN 
          : ppqn = MIDI_SLAVE_PPQN;

        updateTimerTime();
        return;
      }

      if(bitRead(state, MIDI_MASTER) && bitRead(buttons, BTN_BLACK_01)) {
        if(bitRead(buttons, BTN_WHITE_LEFT)) {
          bpm--;
        }

        if(bitRead(buttons, BTN_WHITE_RIGHT)) {
          bpm++;
        }

        updateTimerTime();
        return;
      }

      if(bitRead(buttons, BTN_BLACK_02)) {
        serial_mode = ((serial_mode + 1) & 1); // x&1 == x%2
        Serial.begin((uint32_t) pgm_read_dword(&(SERIAL_MODE[serial_mode])));
        return;
      }
    }

    if(bitRead(buttons, BTN_RED)) {
      if(bitRead(state, MIDI_MASTER)) {
        state ^= 1 << PLAY_PATTERN;
        SR.Led_Pin_Write(BTN_RED, bitRead(state, PLAY_PATTERN));
      }
      return;
    }

    if(bitRead(buttons, BTN_BLACK)) {
      // Mode Navigation
      if(bitRead(buttons, BTN_WHITE_LEFT)) {
        mode = (mode + NB_MODE - 1) % NB_MODE;
        switchOffLcdModeBitmap();
        return;
      }

      if(bitRead(buttons, BTN_WHITE_RIGHT)) {
        mode = (mode + 1) % NB_MODE;
        switchOffLcdModeBitmap();
        return;
      }
    }

    // Instrument navigation
    if(bitRead(buttons, BTN_WHITE_LEFT)) {
      save_instrument();
      inst_index = (inst_index + NB_INSTS - 1) % NB_INSTS;
      load_instrument();
      return;
    }

    if(bitRead(buttons, BTN_WHITE_RIGHT)) {
      save_instrument();
      inst_index = (inst_index + 1) % NB_INSTS;
      load_instrument();
      return;
    }

    if(mode == MODE_PLAY) {
      for(uint8_t _button = BTN_BLACK_00; _button < BTN_BLACK_03; _button++) {
        if(bitRead(buttons, _button)) {
          bitSet(lcd_state, LCD_PLAY_CHANGE);
          // leave mode bitmap
          bitClear(lcd_state, LCD_MODE_BITMAP);
          SR.Led_Pin_Write(BTN_BLACK, bitRead(lcd_state, LCD_MODE_BITMAP));

          if(_button != lcd_mode_play) {
            SR.Led_Pin_Write(lcd_mode_play, 0); // BUTTON = LED
            lcd_mode_play =  _button;
            SR.Led_Pin_Write(lcd_mode_play, 1); // BUTTON = LED
          }
          return;
        }   
      }

      if(bitRead(buttons, BTN_BLACK) 
         && (!bitRead(buttons, BTN_WHITE_LEFT) 
         || !bitRead(buttons, BTN_WHITE_RIGHT))) {
        lcd_state ^= 1 << LCD_MODE_BITMAP;
        bitSet(lcd_state, LCD_PLAY_CHANGE);
        SR.Led_Pin_Write(BTN_BLACK, bitRead(lcd_state, LCD_MODE_BITMAP));
        return;
      }
    }
  }
}

inline void switchOffLcdModeBitmap() {
  bitClear(lcd_state, LCD_MODE_BITMAP);
  bitSet(lcd_state, LCD_PLAY_CHANGE);
  SR.Led_Pin_Write(BTN_BLACK, 0);
}

void checkPotValues() {
  uint8_t _pot_value;
  PMTRACK *_pminst = &(pminsts[inst_index]);
  MTRACK  *_minst  = &(minsts[inst_index]);
  
  if(bitRead(lcd_state, LCD_PLAY_CHANGE)) {
    SmoozPot.Reset();
    return;
  } else {
    for(uint8_t _pot = 0; _pot < NB_PARM; _pot++) {
      _pot_value = SmoozPot.Read(_pot);
      
      if(lcd_mode_play == LCD_00) {
        if(_pot == 0) {
          uint8_t _old_stps = _pminst->stps;
          if(checkValuePotChange(&(_pminst->stps), _pot_value, MAX_STEP)) {
            // Change scale
            updateScale(inst_index);
            
            checkValuePotChange(&(pinst.offs), (pinst.offs * 255 / _old_stps), _pminst->stps);
            
            uint8_t _old_puls = _pminst->puls;
            if(checkValuePotChange(&(_pminst->puls), ((_pminst->puls) * 255 / _old_stps), _pminst->stps)) {
               checkValuePotChange(&(pinst.roll), (pinst.roll * 255 / _old_puls), _pminst->puls);
               checkValuePotChange(&(pinst.roff), (pinst.roff * 255 / _old_puls), _pminst->puls);
               checkValuePotChange(&(pinst.acnt), (pinst.acnt * 255 / _old_puls), _pminst->puls);
               checkValuePotChange(&(pinst.aoff), (pinst.aoff * 255 / _old_puls), _pminst->puls);
            }
          }
        }
        else if(_pot == 1) {
          uint8_t _old_puls = _pminst->puls;
          if(checkValuePotChange(&(_pminst->puls), _pot_value, _pminst->stps)) {
             checkValuePotChange(&(pinst.roll), (pinst.roll * 255 / _old_puls), _pminst->puls);
             checkValuePotChange(&(pinst.roff), (pinst.roff * 255 / _old_puls), _pminst->puls);
             checkValuePotChange(&(pinst.acnt), (pinst.acnt * 255 / _old_puls), _pminst->puls);
             checkValuePotChange(&(pinst.aoff), (pinst.aoff * 255 / _old_puls), _pminst->puls);
          }
        }
        else if(_pot == 2) {
          checkValuePotChange(&(pinst.offs), _pot_value, _pminst->stps);
        } 
        else if(_pot == 3) {
          checkValuePotChange(&(_pminst->note), _pot_value, 127);
        }
        else if(_pot == 4) {
          checkValuePotChange(&(pinst.roll), _pot_value, _pminst->puls);
        } 
        else if(_pot == 5) {
          checkValuePotChange(&(pinst.roff), _pot_value, _pminst->puls);
        } 
        else if(_pot == 6) {
          checkValuePotChange(&(pinst.acnt), _pot_value, _pminst->puls);
        }
        else if(_pot == 7) {
          checkValuePotChange(&(pinst.aoff), _pot_value, _pminst->puls);
        }
  
        if(bitRead(state, POT_CHANGE)) {
          euclid(_pminst->stps, _pminst->puls, pinst.offs, &(_minst->beat_holder));
          euclid(_pminst->puls, pinst.roll, pinst.roff, &(_minst->roll_holder));
          euclid(_pminst->puls, pinst.acnt, pinst.aoff, &(_minst->acnt_holder));
          computeNote(inst_index);
          continue;
        }
      }
      else if(lcd_mode_play == LCD_01) {
        if(_pot == 0) {
          if(checkValuePotChange(&(_pminst->rythm_mode), _pot_value, 2)) {
            updateScale(inst_index);
          }
        }
        else if(_pot == 1) {
          checkValuePotChange(&(_pminst->swing), _pot_value, _minst->scale / 2);
        }
        else if(_pot == 3) {
          checkValuePotChange(&(_pminst->chnl), _pot_value, 12);
        } 
      }
      // Ist Tn Of Al Af
      //     |_|_||_|_|_|
      else if(lcd_mode_play == LCD_02) { 
        if(_pot == 0) {
          uint8_t _old_ton = _pminst->arp_ton;
          if(checkValuePotChange(&(_pminst->arp_ton), _pot_value, 12)) { // 12 semitons per octave
            checkValuePotChange(&(pinst.arp_mod), (pinst.arp_mod * 255 / _old_ton), _pminst->arp_ton);
          }
        }
        else if(_pot == 1) {
          checkValuePotChange(&(pinst.arp_mod), _pot_value, _pminst->arp_ton);
        }
        else if(_pot == 2) {
          checkValuePotChange(&(pinst.arp_alt), _pot_value, _pminst->arp_ton);
        }
        else if((_pot == 3)) {
          checkValuePotChange(&(pinst.arp_aof), _pot_value, 10);
        }
        else if((_pot == 4)) {
          checkValuePotChange(&(_pminst->arp_oct), _pot_value, 4);
        } 
        else if((_pot == 5)) {
          checkValuePotChange(&(_pminst->arp_typ), _pot_value, 3);
        }
  
        // Update arp
        if(bitRead(state, POT_CHANGE)) {
          euclid_arp(
            _pminst->arp_ton, pinst.arp_mod, pinst.arp_alt, pinst.arp_aof, 
            &(_minst->arp_holder), _minst->arp_line);
            
          computeNote(inst_index);
        }
      }
      
      // Return on first pot change
      if(bitRead(state, POT_CHANGE)) {
        return;
      }
    }
  }
}

uint8_t checkValuePotChange(uint8_t *value, const uint8_t _value_pot, const uint8_t _max_value) {
  uint8_t _value = (_value_pot * _max_value) / 255;
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
  PMTRACK *_pminst = &(pminsts[inst_index]);
  MTRACK  *_minst  = &(minsts[inst_index]);

  if(lcd_mode != mode) {
    lcd_mode = mode;
    bitSet(lcd_state, LCD_MODE_CHANGE);    
    bitSet(lcd_state, LCD_PLAY_CHANGE);
  }

  if(lcd_mode == MODE_PLAY) {
    // MODE BITMAP
    if(bitRead(lcd_state, LCD_MODE_BITMAP) ) {
      if(lcd_mode_play == LCD_00) {
        // Draw the full sequence
        if(bitRead(lcd_state, LCD_MODE_CHANGE) 
            || bitRead(lcd_state, LCD_PLAY_CHANGE) 
            || bitRead(state, POT_CHANGE)) {
          lcd.clear();
          // Draw all steps
          uint8_t _pulse_step = 0;
          for(uint8_t _step = 0; _step <  _pminst->stps; _step++) {
            lcd.setCursor(_step & 15, _step >> 4); // x%16 == x&15 i>>4 == i / 16
  
            if(_CHK(_minst->beat_holder, _step)) {
              lcd.write(uint8_t(getActiveStepChar(_pulse_step)));
              _pulse_step++;
            } 
            else {
              lcd.write(char(95));
            }
          }
        }
          
        // only draw active and last step
        if(bitRead(state, PLAY_PATTERN) 
            && bitRead(lcd_state, LCD_DRAW_BITMAP) && bitRead(_pminst->state, STP_CHG)) {
          bitClear(lcd_state, LCD_DRAW_BITMAP);
          // current step
          lcd.setCursor(_minst->count_step & 15, _minst->count_step >> 4);
          lcd.write(char(219));
  
          // redraw last step
          uint8_t _last_step = (_pminst->stps + _minst->count_step - 1) % _pminst->stps;
          uint8_t _last_puls = (_pminst->puls + _minst->count_puls - 1) % _pminst->puls;
          if(_CHK(_minst->beat_holder, _minst->count_step)) {
            _last_puls = (_pminst->puls + _last_puls - 1) % _pminst->puls;
          }
          
          uint8_t _active_char;
          lcd.setCursor(_last_step & 15, _last_step >> 4);
          (_CHK(_minst->beat_holder, _last_step)) ?
            _active_char = uint8_t(getActiveStepChar(_last_puls))
          :
            _active_char = 95;
          
          lcd.write(_active_char);
        }
  
        return;
      }
    }
    
    //MODE PLAY VALUES LINES
    if(bitRead(state, POT_CHANGE) 
        || bitRead(state, INST_CHANGE) 
        || bitRead(lcd_state, LCD_PLAY_CHANGE)) {
          
      lcd.clear();
      char _line1[16] = {' '}, _line2[13] = {' '};

      bitClear(state, INST_CHANGE);

      // INST ST PL OF NT
      //      RL OF AT OF
      if(lcd_mode_play == LCD_00) {
        // LINE 01
        char _note[4];
        sprintf_P(_note, MIDI_NOTE[_pminst->note % 12], _pminst->note / 12);

        sprintf_P(_line1, LCD_LINES_01[LCD_00], 
          inst_index + 1, _pminst->stps, _pminst->puls, pinst.offs, _note);
        
        // LINE 02 
        sprintf_P(_line2, LCD_LINES_02[LCD_00], 
          pinst.roll, pinst.roff, pinst.acnt, pinst.aoff);
      }
      
      // INST RYT SWG    CHN
      else if(lcd_mode_play == LCD_01) {
        // LINE 01
        char _rythm_mode[3];
        strcpy_P(_rythm_mode, RYTHM_MODE_LABELS[_pminst->rythm_mode]);

        sprintf_P(_line1, LCD_LINES_01[lcd_mode_play],
          inst_index + 1, _rythm_mode, _pminst->swing, _pminst->chnl);

        // LINE 02
        strcpy_P(_line2, LCD_LINES_02[lcd_mode_play]);
      }
      
      // INST TON MOD ALT AOF 
      //      OCT TYP      
      else if(lcd_mode_play == LCD_02) {
        // LINE 01
        sprintf_P(_line1, LCD_LINES_01[lcd_mode_play],
          inst_index + 1, _pminst->arp_ton, pinst.arp_mod, pinst.arp_alt, pinst.arp_aof);

         // LINE 02
         if(bitRead(lcd_state, LCD_MODE_BITMAP) ) {
           memcpy(_line2, _minst->arp_line, 13);
         } else {
          char _arp_typ[3];
          strcpy_P(_arp_typ, ARP_TYPES[_pminst->arp_typ]);
          sprintf_P(_line2, LCD_LINES_02[lcd_mode_play],
            _pminst->arp_oct, _arp_typ);
         }
      }
      
      lcd.setCursor(0, 0);
      for(uint8_t _i = 0; _i < 16; _i++) {
        lcd.write(_line1[_i]);
      }

      lcd.setCursor(3, 1);
      for(uint8_t _i = 0; _i < 13; _i++) {
        lcd.write(_line2[_i]);
      }
      
    }
    return;
  }

  if(lcd_mode == MODE_CONFIG_1) {
    if(bitRead(lcd_state, LCD_MODE_CHANGE) || bitRead(state, BTN_PRESS)) {
      char _line_buf[18];  

      strcpy_P(_line_buf, LCD_CONFIG_LINES[0]);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(_line_buf);

      char _ser[4], _yesno[4];
      uint8_t _master = bitRead(state, MIDI_MASTER);
      strcpy_P(_yesno, LABELS[_master]);
      strcpy_P(_ser,   SERIAL_MODE_LABELS[serial_mode]);
      
      sprintf_P(_line_buf, LCD_CONFIG_LINES[1], _yesno, bpm, _ser);

      lcd.setCursor(0,1);
      lcd.print(_line_buf);
    }
    return;
  }
}

uint8_t getActiveStepChar(const uint8_t _puls) {
  MTRACK *_minst = &(minsts[inst_index]);
  
  uint8_t _step_char = 0;
  // Apply accent
  if((_minst->acnt_holder != NULL)  
    && _CHK(_minst->acnt_holder, _puls)) {
    _step_char++;
  }

  // Apply roll
  if((_minst->roll_holder != NULL) 
    && _CHK(_minst->roll_holder, _puls)) {
    _step_char += 2;
  }

  return _step_char;
}

void resetAlltrack() {
  PMTRACK *_pminst;
  MTRACK  *_minst;
  for(uint8_t _inst = 0; _inst < NB_INSTS; _inst++) {
    _pminst = &(pminsts[_inst]);
    _minst  = &(minsts[_inst]);
    
    if(bitRead(_pminst->state, NOTE_ON)) {
      Send_NoteOFF(_inst);
    }
    
    _minst->count_ppqn = 0;
    _minst->count_step = 0;
    _minst->count_puls = 0;
    
    if(_pminst->puls > 0 && _CHK(_minst->beat_holder, 0)) {
      computeNote(_inst);
    }
    bitClear(_pminst->state, STP_CHG);
  }
}

void updateMidi() {
  if(bitRead(state, BTN_PRESS) && bitRead(buttons, BTN_RED)) {
    if(bitRead(state, PLAY_PATTERN)) {
      Serial.write(MIDI_START);
      for(uint8_t _i = 0; _i < NB_INSTS; _i++) {
        bitSet(pminsts[_i].state, STP_CHG);
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
    
    PMTRACK *_pminst;
    MTRACK *_minst;
    for(uint8_t _inst = 0; _inst < NB_INSTS; _inst++) {
      _pminst = &(pminsts[_inst]);
      _minst  = &(minsts[_inst]);

      if(_pminst->puls > 0 && bitRead(_pminst->state, STP_CHG)) {
        if(bitRead(_pminst->state, NOTE_ON)) {
          Send_NoteOFF(_inst);
        }

        // Play step
        if(_CHK(_minst->beat_holder, _minst->count_step)) {
          Send_NoteON(_inst);
          _minst->count_puls = (_minst->count_puls + 1) % _pminst->puls;
        }
      }
    }
    bitClear(state, UPDATED_PPQN);
  }
}

inline void Send_NoteON(const uint8_t _inst) {
  PMTRACK *_pminst = &(pminsts[_inst]);
  MTRACK  *_minst  = &(minsts[_inst]);
  
  Serial.write(144 + _pminst->chnl - 1);//Note ON on selected channel
  Serial.write(_minst->next_note);
  Serial.write(_minst->next_velo);
  
  _minst->last_note = _minst->next_note;
  
  // Turn on inst led
  if(_inst == inst_index) {
    SR.Led_Pin_Write((_minst->count_puls & 1) + 6, 1);
  }

  // Note On
  bitSet(_pminst->state, NOTE_ON);
}

inline void Send_NoteOFF(const uint8_t _inst) {
  PMTRACK *_pminst = &(pminsts[_inst]);
  MTRACK  *_minst  = &(minsts[_inst]);
  
  Serial.write(128 + _pminst->chnl - 1);//Note OFF on selected channel
  Serial.write(_minst->last_note);
  Serial.write(0x01);//velocite 0

  // Turn off inst led
  if(_inst == inst_index) {
    SR.Led_Pin_Write((_minst->count_puls & 1) + 6, 0);
  }

  // Note Off
  bitClear(_pminst->state, NOTE_ON);
}

inline void updateTimerTime() {
  // Timer time = 60  / (BPM * PPQN)
  // 24 ppqn = 2 500 000, 48 = 1 250 000, 96 = 625 000
  timer_time = 60000000 / (bpm * ppqn);
}

inline void updateScale(const uint8_t _inst) {
  PMTRACK *_pminst = &(pminsts[_inst]);
  MTRACK  *_minst  = &(minsts[_inst]);
  ((_pminst->rythm_mode == RYTHM_MODE_SOLO) && (_pminst->stps != 0)) 
    ? _minst->scale = (ppqn * 8) / _pminst->stps // 96 = 24 ppqn x 4 noires = nb ppqm / bar
    : _minst->scale  = ppqn / 4;
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
      uint8_t data = Serial.read();
      if(data == MIDI_START) {
        bitSet(state, PLAY_PATTERN);
      }
      else if(data == MIDI_STOP ) {
        bitClear(state, PLAY_PATTERN);
        resetAlltrack();
      }
      else if(data == MIDI_CLOCK && bitRead(state, PLAY_PATTERN)) {
        int time = millis();
        bpm = (uint8_t) (time - old_clock_time) / MIDI_SLAVE_PPQN * 60000;
        old_clock_time = time;
        update_all_count_ppqn();
      }
    }
  }
}

void update_all_count_ppqn() {
  bitSet(state, UPDATED_PPQN);
  PMTRACK *_pminst;
  MTRACK  *_minst;
  
  for(uint8_t _i = 0; _i < NB_INSTS; _i++) {
    _pminst = &(pminsts[_i]);
    
    if(_pminst->puls > 0) {
      _minst  = &(minsts[_i]);
      
      // Loop to the start of sequence
      if(_minst->count_ppqn >= ((_pminst->stps * _minst->scale) - 1)) {
        _minst->count_ppqn = 0;
        _minst->count_puls = 0;
      } 
      else {
        _minst->count_ppqn++;
      }

      uint8_t _current_step = _minst->count_ppqn / _minst->scale; //straight
      // Apply swing
      // Delay the second 16th note within each 8th note.
      // ie. delay all the even-numbered 16th notes within the beat (2, 4, 6, 8, etc.)
      if(_pminst->swing != 0 
        && (((_current_step + 1) & 3) == 0)) {
        _current_step = (_minst->count_ppqn - _pminst->swing) / _minst->scale;
      }

      // New step
      if(_minst->count_step != _current_step) {
        bitSet(_pminst->state, STP_CHG);
        bitSet(lcd_state, LCD_DRAW_BITMAP);
        
        _minst->count_step = _current_step % _pminst->stps;
        if(_CHK(_minst->beat_holder, _minst->count_step)) {
          computeNote(_i);
        }
      } 
      else {
        bitClear(_pminst->state, STP_CHG);

        // Half step
        if((_minst->count_ppqn % _minst->scale) == (_minst->scale >> 1)) {
          // Apply roll on old active step
          if(_minst->roll_holder != NULL && bitRead(_pminst->state, NOTE_ON)) {
            uint8_t _last_puls = (_pminst->puls + _minst->count_puls - 1) % _pminst->puls;
            if(_CHK(_minst->roll_holder, _last_puls)) {
              // Replay step  
              bitSet(_pminst->state, STP_CHG);
              _minst->count_puls = _last_puls;
            }
          }
        }
      }
    }
  }
}

void computeNote(uint8_t _inst) {
  PMTRACK *_pminst = &(pminsts[_inst]);
  MTRACK  *_minst  = &(minsts[_inst]);
  
  // Compute next note and velo
  if(_pminst->puls > 0) {
    // Compute midi note
    uint8_t _note = _pminst->note;
    
    // Apply arp
    if(_minst->arp_holder != NULL & _pminst->arp_ton != 0 && _minst->count_puls != 0) {
      // Apply oct
      int8_t  _oct    = (_pminst->arp_oct != 0) ? (_minst->count_puls / _pminst->arp_ton) % _pminst->arp_oct : 0;
      boolean _oct_up = false;
      // ARP UP
      if(_pminst->arp_typ == 0) {
        _oct_up = true;
      } 
      // ARP DWN
      else if(_pminst->arp_typ == 1) {
        _oct = -_oct;
      } 
      // ARP UDw
      else if(_pminst->arp_typ == 2) {
        if(_oct >= (_pminst->arp_oct / 2)) {
          _oct = _pminst->arp_oct - _oct;
        } else {
          _oct_up = true;
        }
      } 
      // ARP DwU
      else if(_pminst->arp_typ == 3) {
        if(_oct <= (_pminst->arp_oct / 2)) {
          _oct = -_oct;
        } else {
          _oct = -(_pminst->arp_oct - _oct);
          _oct_up = true;
        }
      }
      _note += (_oct * 12);
      
      // Position of the count_puls th in arp_holder
      int8_t _scale_mod = 0;
      uint8_t _count = (_minst->count_puls % _pminst->arp_ton) + 1;
      while(_count != 0) {
        if(_CHK(_minst->arp_holder, (12 + _scale_mod) % 12)) {
          _count--;
        }
        (_oct_up)
          ? _scale_mod += (_count != 0)
          : _scale_mod -= (_count != 0);
      }
      _note += _scale_mod;
    }
    
    _minst->next_note = min(_note, 127);
    _minst->next_note = max(_note, 0);

    // Compute midi velo
    (_minst->acnt_holder != NULL && _CHK(_minst->acnt_holder, _minst->count_puls)) 
      ? _minst->next_velo = 127
      : _minst->next_velo = DEFAULT_MIDI_VELO;
  }
}

// Euclid calculation function
uint8_t eucl_pos;
int8_t eucl_remainder[MAX_STEP];
int8_t eucl_count[MAX_STEP];
void euclid(const uint8_t _step, const uint8_t _pulse, const uint8_t _offs, uint8_t* *_tab) {
  // Init tab
  free(*_tab);
  (_step > 0)
    ?  *_tab = (uint8_t*) calloc(_step >> 3, 1)
    :  *_tab = NULL;

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

void build(const int8_t _level, const uint8_t _step, const uint8_t _offs, uint8_t* _tab) {
  uint8_t _newPos = eucl_pos++;

  // Apply offset
  _newPos = (uint8_t) (_newPos + _offs + 1)  % _step;  

  if(_level == -1) {
    _CLR(_tab, _newPos);
  } 
  else if(_level == -2) {
    _SET(_tab, _newPos);
  } 
  else {
    eucl_pos--;
    for(uint8_t _p = 0; _p < eucl_count[_level]; _p++) {
      build(_level-1, _step, _offs, _tab); 
    }

    if(eucl_remainder[_level] != 0) {
      build(_level-2, _step, _offs, _tab); 
    }
  }
}

void euclid_arp(const uint8_t _ton, const uint8_t _mod, const uint8_t _altr, const uint8_t  _aoff, 
                uint8_t* *_arp, uint8_t* _arp_line) {
  // Init tab
  if(_ton == 0) {
    *_arp = NULL;
    for(uint8_t _stp = 0; _stp < 13; _stp++) {
      _arp_line[_stp] = 32;
    }
    return;
  }
  
  // First bit is 1
  uint8_t _spos = 0, _count = (_mod + 1);
  while(_count != 0) {
    euclid(12, _ton, _spos, _arp);
    _count -= ((*_arp)[0] & 1);
    _spos++;
  }
  
  uint8_t* _alts = NULL;
  
  // First bit is 0
  uint8_t _apos = 0; _count = (_aoff + _mod + 2);
  while(_count != 0) {
    euclid(12, _altr, _apos, (&_alts));
    _count -= (_alts != NULL) && !(_alts[0] & 1);
    _apos++;
  }
  
  uint8_t _buf[2];         // 16bits we need 12
  _buf[0] = (*_arp)[0];
  _buf[1] = (*_arp)[1];
  
  for(uint8_t _stp = 0; _stp < 12; _stp++) {
    if((_alts != NULL) && _CHK((_alts), _stp)) {
      uint8_t _lstp = (12 + _stp - 1) % 12;
      
      // '01' -> '1<' => down alt
      if(_CHK((_buf), _stp)) {
        if(!_CHK((_buf), _lstp)) {
          _SET((*_arp), _lstp);
          _CLR((*_arp), _stp);
          
          _arp_line[_lstp] = 0;
          _arp_line[_stp] = 4; // "<"
        } else {
          _arp_line[_stp] = 0;
        }
      // '10' -> '>1' => up alt
      } else {
        if(_lstp != 0 && _CHK((_buf), _lstp)) {
          _SET((*_arp), _stp);
          _CLR((*_arp), _lstp);
          
          _arp_line[_lstp] = 5; // ">"
          _arp_line[_stp]  = 0;
        } else {
          _arp_line[_stp] = 95;
        }
      }
    }
    
    // No altr
    else {
      if(_CHK((*_arp), _stp)) {
        _arp_line[_stp] = 0;
      } else {
        _arp_line[_stp] = 95;
      }
    }
  }
  free(_alts);
  _arp_line[12] = 32;
}
