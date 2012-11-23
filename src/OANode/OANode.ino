/*
  OANode
  
  Author: Joshua Eliser

  Filesystem structure:
  / <root>
    /config
      OANode.cfg   // Holds all of the configuration data: IP address, port, NodeID
                   // polling period, sensor configuration, etc
    /data
      raw_<NTP_DATE>.log  // This is one day's worth of data
      raw_<NTP_DATE>.log  // This is one day's worth of data
      raw_<NTP_DATE>.log  // This is one day's worth of data


  Workflow:
  setup -
    if(OANode.cfg is present):
      load last config
    else
      use compile defaults
    
    setup ethernet
    setup sensors
    
  loop -
    if(current time > polling time)
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

/* defines */
#define NODE_ID        (1) /* This needs to be unique */
#define CHIP_SELECT    (4) /* On the Ethernet Shield, CS is pin 4. */

/* typedefs */
typedef struct {
  int   mNodeId;
  bool  mEthAvail;
  bool  mSdAvail; 
  int   mPollPeriodSecs;
} OANode_Cfg_t;


/* globals */
/* Default Ethernet configuration */
/* TODO - Should this go in the global structure?? */
byte mMAC[] = {0x90, 0xA2, 0xDA, 0x0D, 0x30, 0x5E};  /* OANode network MAC address */
byte mLocalIP[] = {192, 168, 0, 189}; /* ONANode local IP address */
byte mServerIP[] = { 64, 233, 187, 99 };  /* OAServer IP address */
int  mPortNum = 4567;  /* OAServer port number */


OANode_Cfg_t mCfg;
EthernetClient mClientSock;
  
void setup()
{
  /* Load the defaults for this OANode */
  mCfg.mNodeId         = NODE_ID;
  mCfg.mEthAvail       = true;
  mCfg.mSdAvail        = true;
  mCfg.mPollPeriodSecs = 10;

  pinMode(10, OUTPUT); /* Must be set to output for SD library software to work */

  /* Open serial comm */
  Serial.begin(112500);

  /* Initialize the SD card */
  Serial.print("INFO: Initializing SD card ... ");
  if(SD.begin(CHIP_SELECT)) {
    Serial.print(" SUCCESS\n");
    /* TODO - Test the file structure and create as necessary */
    /* TODO - Load all of the configuration data from the SD card into the global structure */ 
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
    /* TODO - Write this data to SD card so next power cycle it will have the latest infomation if eth is unavailable */
  }
  
  
  /* Configure all of the sensors */
  /* TODO - Set the analog pin dir/type, setup te digital pins, etc */
}

void loop()
{
  /* Build the CSV string to be written to the file */
  String mDataStr = "";
  for (int analogPin = 0; analogPin < 3; analogPin++) {
    int sensor = analogRead(analogPin);
    mDataStr += String(sensor);
    if (analogPin < 2) {
      mDataStr += ",";
    }
  }

  /* Attempt to open the data file.  It will create/append with FILE_WRITE */
  File mFd = SD.open("/data/data_123.log", FILE_WRITE);
  if(mFd) {
    mFd.println(mDataStr);
    mFd.close();
  }  
  
  /* Service any server requests here */
  if(mCfg.mEthAvail) {
    /* TODO - 
        switch(request type)
          case GetLatestData: return mDataStr;
          case DownloadSD: return AllOfTheSDCardData // This will probably stop the system sampling due to unknown latencies;
          case NTPSync: save the incoming system time as the current seconds counter
    */
  }
  
  delay(mCfg.mPollPeriodSecs*1000);
}
