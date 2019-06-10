#include <RTClib.h>

/*
  Datalogger for induction loop traffic counters
  Texas Instruments LDC1614EVM quad inductance sensor

  Switchable LED signal to display bypassing car

  Based on SD logging example by Tom Igoe, 2010-2012
  Also based on the SFRRangerReader example by Nicholas Zambetti, 2006
  and the calibration example by David A Mellis, 2011

  Using Adafruit Datalogging Shield Rev3

  Author: Timo Ruohomaki timo.ruohomaki@forumvirium.fi
  
  This example code is in the public domain.
  www.github.com/VekotinVerstas/TrafficCounter

 */

#include <Wire.h>
#include <RTClib.h>
#include <SD.h>

RTC_PCF8523 rtc;

const int loopAPin = A0;
const int loopBPin = A1;
const int loopCPin = A2;
const int loopDPin = A3;

const int chipSelect = 10;    // 10 for Datalogger Shield, 4 for Ethernet Shield

int reading = 0;

int ledPin = 4; // car sensing led pin
int btnPin = 2; // calibration button pin
int btnVal = 0; // calibration button value

int loopAValue = 0;
int loopAMin = 1023;
int loopAMax = 0;

int loopBValue = 0;
int loopBMin = 1023;
int loopBMax = 0;

bool calibMode = false;

Sd2Card card;
SdVolume volume;
SdFile root;

// ****** CUSTOM FUNCTIONS START HERE ******

void calibrate() {

  // when button pressed,
  // set loop sensor min and max values when no cars are passing
  // values within this range will not be logged

calibMode = true;

 while (millis() <5000) {
    loopAValue = digitalRead(loopAPin);
    
    if (loopAValue > loopAMax) {
      loopAMax = loopAValue;
    }

    if (loopAValue < loopAMin) {
      loopAMin = loopAValue;
    }
  
 }

  logger("Calibration mode finished!");

calibMode = false;
  
}

void logger(String message) {

  Serial.println("[" + sqlDateTime() + "] " + message);
  
}

String sqlDateTime() {

  // 2011-09-03T08:13:32

  DateTime now = rtc.now();

  String s = "";

  s = String(now.year()) + "-" + format00(now.month()) + "-" + format00(now.day());
  s = s + "T";
  s = s + format00(now.hour()) + ":" + format00(now.minute()) + ":" + format00(now.second());

  return s;
  
 }

String format00(int number) {
  String st = "";
  if (number >= 0 && number < 10) {
    st = ('0');
  }
  return st += String(number);
}

// ***** DEFAULT FUNCTIONS START HERE ******

void setup() {
  
  logger("Starting Traffic Counter...");
 
  // Open serial communications and wait for port to open:
  Serial.begin(57600);
  
  while (!Serial) {
    // Connection for programmer software
    // TODO maybe needs a timeout
    ; // wait for serial port to connect. Needed for native USB port only
  }

 Serial.begin(57600);
 if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

 if (! rtc.initialized()) {
   Serial.println("RTC is not running, setting time by host.");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

 // Open I2C using Wire
  Wire.begin();

  pinMode(ledPin, OUTPUT);
  pinMode(btnPin, INPUT);

  logger("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!card.init(SPI_HALF_SPEED,chipSelect)) {
    logger("Card failed, or not present.");
    // don't do anything more:
    return;
  }
  logger("SD card initialized.");
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      logger ("Card type: SD1");
      break;
    case SD_CARD_TYPE_SD2:
      logger ("Card type: SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      logger ("Card type: SDHC");
      break;
    default:
      logger ("Uknown card type.");
  }

  logger("Checking volumes...");

  if (!volume.init(card)) {
    logger ("Could not find FAT16/FAT32 partition.");
    return;
  }

  uint32_t volumesize;
  volumesize = volume.blocksPerCluster();
  volumesize *= volume.clusterCount();
  volumesize /= 2;

  logger ("Volume size: " + String((float)volumesize / 1024.0));
  
}

// ****** MAIN LOOP ******

void loop() {

if ( calibMode == false) {

  // get button status

  btnVal = digitalRead(btnPin);

// btnVal = HIGH;
  
  if (btnVal == LOW) {
    digitalWrite(ledPin, LOW); // led OFF
  } else {
    digitalWrite (ledPin, HIGH); // led ON
    //logger("Entering calibration mode...");
    //calibrate();
  }
  
  // call I2C

  Wire.beginTransmission(112); // transmit to device #112 (0x70)
  // in this case the address specified in the datasheet is 224 (0xE0)
  // but i2c addressing uses the high 7 bits so it's 112 instead
  Wire.write(byte(0x00)); // sets register pointer to command register
  
  // make a string for assembling the data to log:
  String dataFileName = "";
  String dataTimeSeries = "";
  String dataDelimiter = ";";

  // read I2C sensor values (max 4 channels) and append to the string:
  
  for (int analogPin = 0; analogPin < 3; analogPin++) {
    int sensor = analogRead(analogPin);
    dataTimeSeries += String(sensor);
    if (analogPin < 2) {
      dataTimeSeries += dataDelimiter;
    }
  }

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  
  File dataFile = SD.open(dataFileName, FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataTimeSeries);
    dataFile.close();
    // print to the serial port too:
    logger(dataTimeSeries);
  }
  // if the file isn't open, pop up an error:
  else {
   // logger("error opening traffic.csv");
  }

  } // end of calibMode

delay(1000); // set poll rate
  
}
