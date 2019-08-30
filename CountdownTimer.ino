#include "tone.h"

// pin position definition
#define SPEAKER_OUT 2
#define DSP_A_OUT 3
#define DSP_B_OUT 4
#define DSP_C_OUT 5
#define DSP_D_OUT 6
#define DSP_E_OUT 7
#define DSP_F_OUT 8
#define DSP_G_OUT 9
#define DSP_DSP0_OUT 13
#define DSP_DSP1_OUT 12
#define DSP_DSP2_OUT 11
#define DSP_DSP3_OUT 10
#define SW_RESET_IN 14
#define SW_SETUP_IN 15
#define SW_PLAYPAUSE_IN 16

// basic propertis
#define LED_REFRESH_NORMAL_TIME 500
#define LED_REFRESH_QUICK_TIME  10
#define ITER_PER_SECOND 40
#define TICK_PER_10MS 2500 // Using 1/64 prescaler
const long MAX_MILLISECOND = 5999000L;

// States
#define STANDBY  0
#define SB_RESETTRIGGER 10
#define SB_SETUPTRIGGER 20
#define SB_PLAY_TRIGGER 30
#define COUNTING 100
#define CT_PLAY_TRIGGER 130
#define TIMESUP  200

// Constant Array Definitions
const byte numberTo7Seg[] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66,   // 0, 1, 2, 3, 4,
  0x6D, 0x7D, 0x07, 0x7F, 0x67    // 5, 6, 7, 8, 9
};

const byte symbolTo7Seg[] = {
  0x00, 0x01, 0x02, 0x04, 0x08,       //  space, -a, -b, -c, -d,
  0x10, 0x20, 0x40, 0x79, 0x54, 0x5E  // -e, -f, -g,'E','n','d'
};

const int dspToPin[] = {
  DSP_DSP0_OUT,
  DSP_DSP1_OUT,
  DSP_DSP2_OUT,
  DSP_DSP3_OUT
};

// Timer stuff
volatile int ledRefreshCounter = 0;
volatile long countMs = 0;

// LED stuff
volatile int ms_L = 0;
volatile int ms_H = 0;
volatile int second_L = 0;
volatile int second_H = 0;
volatile int minute_L = 0;
volatile int minute_H = 0;

// Speaker stuff
const int melodyLength = 13;
const int melody[] = {
  NOTE_,
  NOTE_E5,  NOTE_D5,  NOTE_FS4, NOTE_GS4,
  NOTE_CS5, NOTE_B4,  NOTE_D4,  NOTE_E4,
  NOTE_B4,  NOTE_A4,  NOTE_CS4, NOTE_E4,  NOTE_A4
};
const int duration[] = {
  0,  // millisecond to start play
  125, 125, 250, 250,
  125, 125, 250, 250,
  125, 125, 250, 250, 500
};
int timeThrough = 0;
int playPosition = 1;
int playTimes = 0;

// Switch states
int prevSwResetState = LOW;
int prevSwSetupState = LOW;
int prevSwPlaypauseState = LOW;
int currSwResetState = LOW;
int currSwSetupState = LOW;
int currSwPlaypauseState = LOW;
volatile int state = STANDBY;
int swSetupTimes = 0;
int timesUpTimes = 0;
bool tuDispFlag = false;
bool longstay = false;

void setup() {
  // pin mode setup
  pinMode(SPEAKER_OUT,OUTPUT);
  pinMode(DSP_A_OUT,  OUTPUT);
  pinMode(DSP_B_OUT,  OUTPUT);
  pinMode(DSP_C_OUT,  OUTPUT);
  pinMode(DSP_D_OUT,  OUTPUT);
  pinMode(DSP_E_OUT,  OUTPUT);
  pinMode(DSP_F_OUT,  OUTPUT);
  pinMode(DSP_G_OUT,  OUTPUT);
  pinMode(DSP_DSP0_OUT, OUTPUT);
  pinMode(DSP_DSP1_OUT, OUTPUT);
  pinMode(DSP_DSP2_OUT, OUTPUT);
  pinMode(DSP_DSP3_OUT, OUTPUT);
  pinMode(SW_RESET_IN,    INPUT);
  pinMode(SW_SETUP_IN,    INPUT);
  pinMode(SW_PLAYPAUSE_IN,INPUT);

  //initialize
  initTimer1();
  displayTime();
}

void loop() {
  // read switches' states
  currSwResetState = digitalRead(SW_RESET_IN);
  currSwSetupState = digitalRead(SW_SETUP_IN);
  currSwPlaypauseState = digitalRead(SW_PLAYPAUSE_IN);

  // working
  switch(state){
    case STANDBY:
    if( swResetRise() ){
      state = SB_RESETTRIGGER;
      countMs = 0;
      refreshClock();
      displayTime();
    }else if( swSetupRise() ){
      state = SB_SETUPTRIGGER;
      swSetupTimes++;
    }else if( swPlaypauseRise() ){
      state = CT_PLAY_TRIGGER;
      resumeTimer1();
    }
      break;
    case SB_RESETTRIGGER:
    if( !swResetHold() ){
      state = STANDBY;
    }
      break;
    case SB_SETUPTRIGGER:
    if( swSetupHold() ){
      if ( swSetupTimes >= ITER_PER_SECOND ){
        longstay = true;
        countMs += 600000;
        refreshClock();
        displayTime();
        swSetupTimes = 0;
      }else{
        swSetupTimes++;
      }
    }else if( longstay ){
      state = STANDBY;
      swSetupTimes = 0;
      longstay = false;
      if( swSetupTimes >= ITER_PER_SECOND ){
        countMs += 600000;
        refreshClock();
        displayTime();
      }
    }else{
      state = STANDBY;
      countMs += 60000;
      refreshClock();
      displayTime();
    }
      break;
    case SB_PLAY_TRIGGER:
    if( !swPlaypauseHold() ){
      state = STANDBY;
    }
      break;
    case COUNTING:
    if( swPlaypauseRise() ){
      state = SB_PLAY_TRIGGER;
      stopTimer1();
    }
      break;
    case CT_PLAY_TRIGGER:
    if( !swPlaypauseHold() ){
      state = COUNTING;
    }
      break;
    case TIMESUP:
    if( swResetRise() || swSetupRise() || swPlaypauseRise() ){
      state = STANDBY;
      resetSpeaker();
    }else{
      // Display control
      if( timesUpTimes >= ITER_PER_SECOND ){
        timesUpTimes = 0;
        if( tuDispFlag ){ displayTime();}
        else{             displayEnd(); }
        tuDispFlag = !tuDispFlag;
      }
      timesUpTimes++;
      
      // Speaker control
      song();
      if( playTimes >= 3 ){
        state = STANDBY;
        resetSpeaker();
      }
    }
      break;
    default:
      state = STANDBY;
  }

  // finally change states
  prevSwResetState = currSwResetState;
  prevSwSetupState = currSwSetupState;
  prevSwPlaypauseState = currSwPlaypauseState;

  // delay between each iteration
  delay(1000/ITER_PER_SECOND);
}

// LED Driver
void displayNumber(int number, int _display){
  byte bNum = numberTo7Seg[number];
  for(int pin=DSP_A_OUT; pin<=DSP_G_OUT; pin++){
    digitalWrite(pin, (bNum & 0x01)?HIGH:LOW);
    bNum >>= 1;
  }
  digitalWrite(dspToPin[_display], HIGH);
  delayMicroseconds(20);
  digitalWrite(dspToPin[_display], LOW);
}

void displaySymbol(int symbol, int _display){
  byte bSmb = symbolTo7Seg[symbol];
  for(int pin=DSP_A_OUT; pin<=DSP_G_OUT; pin++){
    digitalWrite(pin, (bSmb & 0x01)?HIGH:LOW);
    bSmb >>= 1;
  }
  digitalWrite(dspToPin[_display], HIGH);
  delayMicroseconds(20);
  digitalWrite(dspToPin[_display], LOW);
}

void displayTime(){
  if(countMs >= 60000){
    displayNumber(minute_H, 0);
    displayNumber(minute_L, 1);
    displayNumber(second_H, 2);
    displayNumber(second_L, 3);
  }else{
    displayNumber(second_H, 0);
    displayNumber(second_L, 1);
    displayNumber(ms_H, 2);
    displayNumber(ms_L, 3);
  }
}

void displayUpdate(int _display){
  if(countMs >= 60000){
    switch(_display){
      case 0: displayNumber(minute_H, 0); break;
      case 1: displayNumber(minute_L, 1); break;
      case 2: displayNumber(second_H, 2); break;
      case 3: displayNumber(second_L, 3); break;
      default: 0;
    }
  }else{
    switch(_display){
      case 0: displayNumber(second_H, 0); break;
      case 1: displayNumber(second_L, 1); break;
      case 2: displayNumber(ms_H, 2); break;
      case 3: displayNumber(ms_L, 3); break;
      default: 0;
    }
  }
}

void displayEnd(){
  displaySymbol( 0, 0);
  displaySymbol( 8, 1);
  displaySymbol( 9, 2);
  displaySymbol(10, 3);
}

// Switch state master
bool swResetRise(){ return (prevSwResetState == LOW && currSwResetState == HIGH); }
bool swResetHold(){ return (prevSwResetState == HIGH && currSwResetState == HIGH);}

bool swSetupRise(){ return (prevSwSetupState == LOW && currSwSetupState == HIGH); }
bool swSetupHold(){ return (prevSwSetupState == HIGH && currSwSetupState == HIGH);}
bool swSetupFall(){ return (prevSwSetupState == HIGH && currSwSetupState == LOW); }

bool swPlaypauseRise(){ return (prevSwPlaypauseState == LOW && currSwPlaypauseState == HIGH); }
bool swPlaypauseHold(){ return (prevSwPlaypauseState == HIGH && currSwPlaypauseState == HIGH);}

// Speaker functions
void song(){
  // check that previous tone complete playing
  if(timeThrough >= duration[playPosition - 1]){
    if(playPosition <= melodyLength){
      // play next melody
      tone(SPEAKER_OUT, melody[playPosition], duration[playPosition]*0.9);
      playPosition++;
    }else{
      // whole melody played
      playPosition = 1;
      playTimes++;
    }
    timeThrough = 0;
  }
  timeThrough += 1000/ITER_PER_SECOND;
}

void resetSpeaker(){
  timeThrough = 0;
  playPosition = 1;
  playTimes = 0;
}

// Timer functions
void initTimer1(){
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  OCR1A = TICK_PER_10MS;
  TCCR1B |= (1<<WGM12);             // CTC mode; Clear Timer on Compare
  TCCR1B |= (1<<CS10) | (1<<CS11);  // using 1/64 prescaler
  interrupts();
}
void stopTimer1()  { TIMSK1 &= (~(1<<OCIE1A)); }
void resumeTimer1(){ TIMSK1 |= (1 << OCIE1A);  }
void checkCountMs(){ countMs = ( countMs > MAX_MILLISECOND ) ? MAX_MILLISECOND : countMs; }
void refreshClock(){
  checkCountMs();
  ms_L = (int)(( countMs / 10 ) % 10);
  ms_H = (int)(( countMs / 100) % 10);
  second_L = (int)(( countMs / 1000 ) % 10);
  second_H = (int)(( countMs / 10000) % 6);
  minute_L = (int)(( countMs / 60000 ) % 10);
  minute_H = (int)( countMs / 600000 );
}

// Timer routine
ISR(TIMER1_COMPA_vect){
  countMs -= 10;
  ledRefreshCounter += 10;
  ledRefreshCounter %= 1000;
  if( countMs >= 60000 ){
    if( ledRefreshCounter == 0 ){
      refreshClock();
      displayTime();
    }
  }else{
    refreshClock();
    if( ledRefreshCounter == 0 ){
      displayTime();
    }else if( ledRefreshCounter%100 == 0 ){
      displayUpdate(3);
      displayUpdate(2);
    }else{
      displayUpdate(3);
    }
  }

  // check time's up
  if( countMs <= 0 ){
    stopTimer1();
    initTimer1();
    state = TIMESUP;
    countMs = 0;
    refreshClock();
    displayTime();
  }
}
