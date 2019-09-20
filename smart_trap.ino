#include <RTClib.h>
#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>
//#include <sdios.h>
//
#define OVER_SAMPLING 64
#define FS 8 // in Hz
#define RISE_TIME 100 // in microseconds
#define POWER_PIN 3
#define PHOTO_TRANSISTOR_PIN 3
#define LEARNING_RATE 0.01 // alpha
#define  Z_SCORE_THRESHOLD 4.753424 //R> qnorm(1 - 10^(-6),0,1)

//#define error(msg) sd.errorHalt(F(msg))

// Log file base name.  Must be six characters or less.
#define FILE_BASE_NAME "data"

const float time_to_sleep_us = 1e6 / (FS * OVER_SAMPLING) - RISE_TIME;


//RTC_DS1307 RTC;


// SD chip select pin.  Be sure to disable any other SPI devices such as Enet.
/*
const uint8_t chipSelect = SS;
SdFat sd;
SdFile file;
*/


float rolling_mean = 0;
float rolling_sd =0;
float former_accum = 0;


// Log a data record.
/*
void logData(float x) {
  unsigned long time = millis();
  file.print(time);
  file.write(',');
  file.print(x); 
  file.println();
}
*/

void setup(void) {
    Serial.begin(57600);
    /*RTC.begin();
    
    Wire.begin();
    
    if (! RTC.isrunning()) {
      Serial.println("RTC is NOT running!");
      // This will reflect the time that your sketch was compiled
      RTC.adjust(DateTime(__DATE__, __TIME__));
    }
    */
    pinMode(POWER_PIN, OUTPUT);
/*
  const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
  char fileName[13] = FILE_BASE_NAME "00.csv";


  // Initialize at the highest speed supported by the board that is
  // not over 50 MHz. Try a lower speed if SPI errors occur.
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }

  // Find an unused file name.
  if (BASE_NAME_SIZE > 6) {
    error("FILE_BASE_NAME too long");
  }
 
  
  while (sd.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
      error("Can't create file name");
    }
  }
  if (!file.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
    error("file.open");
  }
*/
}

void loop(void) {
    float accum = 0;
    for(int i =0; i < OVER_SAMPLING; i++){
        digitalWrite(POWER_PIN, HIGH);
        delayMicroseconds(RISE_TIME);
        accum +=  analogRead(PHOTO_TRANSISTOR_PIN);
        digitalWrite(POWER_PIN, LOW);
        if(time_to_sleep_us > 16383)
            delay(time_to_sleep_us / 1e3);
        else
            delayMicroseconds(time_to_sleep_us);
    }
    accum /= OVER_SAMPLING;

    //DateTime now = RTC.now(); 
    

    float delt_accum = accum -former_accum;
    rolling_mean = rolling_mean * (1.0 -LEARNING_RATE) + LEARNING_RATE *delt_accum;
    float abs_diff = abs(rolling_mean -delt_accum );
    rolling_sd = rolling_sd * (1.0 - LEARNING_RATE) + LEARNING_RATE * abs_diff;
    former_accum=accum;
    float z_score = abs_diff / rolling_sd;
    bool crossing = false;
    if(z_score > Z_SCORE_THRESHOLD){
      crossing = true;
      }
    //logData(accum);
    /*
    Serial.print(now.year(), DEC);
    Serial.print('-');
    Serial.print(now.month(), DEC);
    Serial.print('-');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.print('|');
    Serial.print(accum);
    Serial.print('|');
    Serial.print(delt_accum);
    Serial.print('|');
    Serial.print(rolling_mean);
    Serial.print('|');
    Serial.print(rolling_sd);
    Serial.print('|');
    Serial.println(crossing);
    
*/
  Serial.println(accum);
}
