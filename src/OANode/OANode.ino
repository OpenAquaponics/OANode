/*
  OANode
  
  Author: Joshua Eliser

  Filesystem structure:
  / <root>
    /config
      OANode.config // Holds all of the configuration data: IP address, port, NodeID
                    // polling period, sensor configuration, etc
    /data
      raw_<NTP_DATE>.log  // This is one day's worth of data
      raw_<NTP_DATE>.log  // This is one day's worth of data
      raw_<NTP_DATE>.log  // This is one day's worth of data


  Workflow:
  setup -
    if(OANode.config is present):
      load last config
    else
      use compile defaults
    
    setup ethernet
    setup sensors
    
  loop -
    if(current time >= polling time)
      poll sensors
      if SD available
        write to SD
      else
        write to network
    
    if tcp server request
      process request  
        
*/

/* includes */
#include <SD.h>
#include <SPI.h>
#include <Ethernet.h>

#include <avr/sleep.h>
#include <avr/wdt.h>

/* defines */
#define NODE_ID        (1) /* This needs to be unique */
#define CHIP_SELECT    (4) /* On the Ethernet Shield, CS is pin 4. */

#ifndef cbi
# define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
# define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

/* enum */

/* typedefs */
typedef struct {
  int   mNodeId;
  bool  mEthAvail;
  bool  mSdAvail; 
  int   mPollPeriodSecs;
} OANode_Cfg_t;

typedef struct {
  int mWakeup;
  unsigned long mStart;
  char val1[4];
  unsigned long mExecution;
  char val2[4];
  bool  mOverflow;
} Time_t;

/* globals */
/* Default Ethernet configuration */
/* TODO - Should this go in the global structure?? */
byte mMAC[] = {0x90, 0xA2, 0xDA, 0x0D, 0x30, 0x5E};  /* OANode network MAC address */
byte mLocalIP[] = {192, 168, 0, 189}; /* ONANode local IP address */
byte mServerIP[] = { 64, 233, 187, 99 };  /* OAServer IP address */
int  mPortNum = 4567;  /* OAServer port number */

/* Order of analog pins: mTempAirIndoor, mTempAirOutdoor, mTempWater, mHumidityIndoor, mHumidityOutdoor, mWaterLevel, mBattaryVoltage, mSolarPanelVoltage */
char mAnalogPins[]  = {  10,  12,  14,   0,   0,  15,   0,   0 }; /* 0 = not used, else pin number */
float mAnalogConv[] = { 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 }; /* Converts analog to output value (Volts, Temp C, Humidity, etc) */
 
OANode_Cfg_t mCfg;
EthernetClient mClientSock;
Time_t mTime;
  
/***********************************/
void setup() {
/***********************************/
  /* Load the defaults for this OANode */
  mCfg.mNodeId         = NODE_ID;
  mCfg.mEthAvail       = true;
  mCfg.mSdAvail        = true;
  mCfg.mPollPeriodSecs = 10;

  pinMode(10, OUTPUT); /* Must be set to output for SD library software to work */

  /* Open the serial port */
  Serial.begin(112500);


  /* Initialize the SD card */
  Serial.print("INFO: Initializing SD card ... ");
  if(SD.begin(CHIP_SELECT)) {
    Serial.print(" SUCCESS\n");
    /* The SD card is read first so you don't need to compile a unique sketch for each node.
       You can write this config file to a new SD card, clone the OANode hardware, and not
       have to modify the sketch source.  This might not ever really be used, but it's there. */
    if(File mFd = SD.open("/data/OANode.config", FILE_READ)) {
      /* TODO - Load all of the configuration data from the SD card into the global structure */ 
    }  
  }
  else {
    Serial.print(" FAILED\nERR:   Card failed, or not present\n");
    mCfg.mSdAvail = false;
  }

  /* Initialize the Ethernet */
  Serial.print("INFO: Ethernet conncecting to remote server ... ");
  Ethernet.begin(mMAC, mLocalIP);
  if(mClientSock.connect(mServerIP, mPortNum)) {
    Serial.print("SUCCESS\n");
  }
  else {
    Serial.print("FAILED\nERR:   Error connecting to remote host\n");
    mCfg.mEthAvail = false;
  }

  /* Update the node with the latest system information */
  if(mCfg.mEthAvail) {
    /* TODO - Sync with NTP to get the system time (or request it from the server */
    /* TODO - Request the latest node configuration data */
  }
  
  /* Configure all of the sensors */
  for(unsigned int i = 0; i < sizeof(mAnalogPins); i++) {
    /* Determine if the sensor is connected. */
    if(mAnalogPins[i]) {
      pinMode(mAnalogPins[i], INPUT);
    }
  }

  if(mCfg.mSdAvail) {
    /* TODO - Write this data to SD card so next power cycle it will have the latest infomation if eth is unavailable */
  }

  /* Force the main loop to start sampling immediately */
  mTime.mStart = millis();
  mTime.mOverflow = false;
  mTime.mExecution = 0;
}


/***********************************/
void loop() {
/***********************************/

  /* Determine if it's time for the loop to start, handles the overflow at ~50 days */
  unsigned long mCurrTime = millis();
  if(mTime.mOverflow) {
    if(mCurrTime < (unsigned long)100000) {
      mTime.mOverflow = false;
      Serial.println("Here");
    }
    return;
  }
  else if(mCurrTime >= mTime.mStart) { /* It's time to take a sample, compute the next time step */
    mTime.mStart    = mCurrTime + (unsigned long)(mCfg.mPollPeriodSecs * 1000) - (unsigned long)(mCurrTime - mTime.mStart);
    mTime.mOverflow = (mTime.mStart < mCurrTime) ? true : false;
  }
  else { /* Not time for a sample, just return */
    return;
  }

  /* Variable Declaration */
  char mDataStr[128]; /* This could be smaller is necessary */
  float mVal = 0.0;
  
  /* Build the CSV string to be written to the file or ethernet */
  sprintf(&mDataStr[0], "%lu,%lu", mCurrTime, mTime.mExecution);
  for(unsigned int i = 0; i < sizeof(mAnalogPins); i++) {
    mVal = (mAnalogPins[i]) ? (float)analogRead(i) * (float)mAnalogConv[i] : 0.0;
    sprintf(&mDataStr[strlen(mDataStr)], ",");
    dtostrf(mVal, 1, 2, &mDataStr[strlen(mDataStr)]);
  }

  /* If Ethernet is available, send it to the server */
  if(mCfg.mEthAvail) {
    /* TODO - Send Statistics packet to OAServer */
  }
  /* Else if SD is available, save it to the SD card */
  else if(mCfg.mSdAvail) {
    /* Attempt to open the data file.  It will create/append with FILE_WRITE */
    if(File mFd = SD.open("/data/data_123.log", FILE_WRITE)) {
      mFd.println(mDataStr);
      mFd.close();
    }  
  }
  /* Only other option is to write it out over serial. */
  else {
    Serial.println(mDataStr);
  }
  
  /* Service any server requests here */
  if(mCfg.mEthAvail) {
    /* TODO - 
        switch(request type)
          case GetLatestData: return mDataStr;
          case DownloadSD: return AllOfTheSDCardData // This will probably stop the system sampling due to unknown latencies;
          case NTPSync: save the incoming system time as the current seconds counter // This logic will have to be thought out to get the system times synced
    */
  }
  
  if(mCfg.mSdAvail) {
    /* TODO - write the system time to the cfg file so we have the latest timestamp ... helps minimize timestamp problems */
  }

  /* Determine the execution time of the frame */
  mTime.mExecution = millis() - mCurrTime;

  /* TODO - Possbily send processor to low power mode for 'X' seconds to save more power */
  
}









#if 0

  /* This is template code found online to put the AVR into low power mode.  Will put this capability in at a future time */

#if 0
  /* TODO - Put in setup() */
  // CPU Sleep Modes 
  // SM2 SM1 SM0 Sleep Mode
  // 0    0  0 Idle
  // 0    0  1 ADC Noise Reduction
  // 0    1  0 Power-down
  // 0    1  1 Power-save
  // 1    0  0 Reserved
  // 1    0  1 Reserved
  // 1    1  0 Standby(1)

  cbi(SMCR,SE);      // sleep enable, power down mode
  cbi(SMCR,SM0);     // power down mode
  sbi(SMCR,SM1);     // power down mode
  cbi(SMCR,SM2);     // power down mode

  setup_watchdog(7);
#endif

//****************************************************************  
// set system into the sleep state 
// system wakes up when wtchdog is timed out
void system_sleep() {

  cbi(ADCSRA,ADEN);                    // switch ADC OFF
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // sleep mode is set here
  sleep_enable();                      /* Enable the system sleep */
  sleep_mode();                        // System sleeps here
  sleep_disable();                     // System continues execution here when watchdog timed out 
  sbi(ADCSRA,ADEN);                    // switch ADC ON

}

//****************************************************************
// 0=16ms, 1=32ms,2=64ms,3=128ms,4=250ms,5=500ms
// 6=1 sec,7=2 sec, 8=4 sec, 9= 8sec
void setup_watchdog(int ii) {

  byte bb;
  int ww;
  if (ii > 9 ) ii=9;
  bb=ii & 7;
  if (ii > 7) bb|= (1<<5);
  bb|= (1<<WDCE);
  ww=bb;
  Serial.println(ww);


  MCUSR &= ~(1<<WDRF);
  // start timed sequence
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  // set new watchdog timeout value
  WDTCSR = bb;
  WDTCSR |= _BV(WDIE);
}

//****************************************************************  
// Watchdog Interrupt Service / is executed when  watchdog timed out
ISR(WDT_vect) {
  mTime.mWakeup = true;  // set global flag
}

#endif
