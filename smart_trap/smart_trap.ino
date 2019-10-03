#include <RTClib.h>
#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>
//#include <sdios.h>

#define BAUD_RATE 57600
#define OVER_SAMPLING 64
#define FS 8 // in Hz
#define RISE_TIME 100 // in microseconds
#define POWER_PIN 3
#define CLOCK_ADJUST_LEARN_RATE 0.005 // > (1 - 0.005)^150 = 0.47 
#define PHOTO_TRANSISTOR_N 4
#define PHOTO_TRANSISTOR_PINS {0,1,2,3}
#define LEARNING_RATE 0.01 // alpha
#define Z_SCORE_THRESHOLD 4.753424 //R> qnorm(1 - 10^(-6),0,1)
#define ERROR_LED 9
#define METADATA_FILENAME "meta.txt"

#define error(msg) sd.errorHalt(F(msg))

// Log file base name.  Must be 2 characters.
#define FILE_BASE_NAME "st"

const float time_to_sleep_us = 1e6 / (FS * OVER_SAMPLING) - RISE_TIME;

RTC_DS1307 RTC;
RTC_Millis soft_rtc;

// SD chip select pin.  Be sure to disable any other SPI devices such as Enet.
const uint8_t chipSelect = SS;
SdFat sd;
SdFile file;
SdFile metadata_file;


int phototransistor_array[PHOTO_TRANSISTOR_N]= {0,1,2,3};
/*float rolling_mean[PHOTO_TRANSISTOR_N];
float rolling_sd[PHOTO_TRANSISTOR_N];
float former_accum[PHOTO_TRANSISTOR_N];*/

unsigned long real_time_ms=0; //time since boot in ms
float drift_avg_s=0;// the drift between the arduino milli clock and the rtc
//
uint32_t realTimeMs(){
  uint32_t soft_now = soft_rtc.now().unixtime();
  uint32_t hard_now = RTC.now().unixtime();

  int drift = 0;
  if(soft_now < hard_now)
      drift = soft_now - hard_now;
  else
      drift = - (int) (hard_now - soft_now );

  drift_avg_s = drift_avg_s * (1-CLOCK_ADJUST_LEARN_RATE) + CLOCK_ADJUST_LEARN_RATE * (float) drift;

  return millis() - drift_avg_s* 1000;
  }


void info(){
  //1. print time
  //2. print current file and line
  //3. print serial help
  }
void adjustClock(){
  //1. print current time
  //2. parse serial data
  //3. adjust from serial data
  
  }

String generateHeader(RTC_DS1307 rtc, String device_id){
    String header = "#";
    DateTime now = rtc.now();
    char datetime[17];
    sprintf (datetime, "%04d%02d%02d %02d:%02d:%02d",now.year(),now.month(),now.day(), now.hour(), now.minute(), now.second());
    header += datetime;
    header += ",";
    header += device_id;
    return header;
}

// Log a data record.
String generateLogString(uint32_t t, float x[]){
  String out = "";
  out += t;
  out += ',';
  
  for(int j =0; j < PHOTO_TRANSISTOR_N ; j++){
    out += x[j];
    out += ',';    
   }
   return out;
  }

//Write a string to both to file and Serial
void log(String string, SdFile f){
    logSerial(string);
    logSD(string, file);
}


void logSD(String string, SdFile f){
  file.println(string);
  // Force data to SD and update the directory entry to avoid data loss.

  if (file.isOpen() && (!file.sync() || file.getWriteError())) {
    error("write error");
  }
}


void logSerial(String string){
  Serial.println(string);
}

void fatalError(String message, char status, SdFile file){
    log(message, file);
    unsigned int i = 0;
    while(true){
        if(i %10 < status){
           digitalWrite(ERROR_LED, HIGH);
        }
        delay(10);
        digitalWrite(ERROR_LED, LOW);
        delay(490);
        i++;
    }
}
void setup(void) {
    Serial.begin(BAUD_RATE);
    pinMode(POWER_PIN, OUTPUT);
    pinMode(ERROR_LED, OUTPUT);

  // Initialize at the highest speed supported by the board that is
  // not over 50 MHz. Try a lower speed if SPI errors occur.
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
       fatalError("Cannot connect to sd card reader", 1, file);
  }

   if(!metadata_file.open(METADATA_FILENAME, O_READ)){
      fatalError("No metadata file", 2, file);
   }
  String device_id = "---";
  for(int i; i != 3; ++i){
    device_id[i] = metadata_file.read();
  }
  metadata_file.close();

  const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
  char fileName[13] = FILE_BASE_NAME "0000.csv";

  while (sd.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 3] != '9') {
      fileName[BASE_NAME_SIZE + 3]++;
    } else if (fileName[BASE_NAME_SIZE + 2] != '9') {
      fileName[BASE_NAME_SIZE + 3] = '0';
      fileName[BASE_NAME_SIZE + 2]++;
    } else if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 2] = '0';
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
      fatalError("Cannot create filename", 3, file);
    }
  }
  if (!file.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
     fatalError("Cannot open file", 3, file);
  }

    RTC.begin();
    Wire.begin();

    if (! RTC.isrunning()) {
      Serial.println("RTC is NOT running!");
       //fixme use external env variables so we set to UTC?
      RTC.adjust(DateTime(__DATE__, __TIME__));
      fatalError("No clock", 3, file);
    }

    soft_rtc.begin(RTC.now());

  log(generateHeader(RTC, device_id), file);
}


void loop(void) {
  //todo check for instructions e.g. set rtc with serial
    float accum[PHOTO_TRANSISTOR_N];
    for(int j =0; j < PHOTO_TRANSISTOR_N ; j++){
      accum[j] = 0;
    }
    for(int i =0; i < OVER_SAMPLING; i++){
        digitalWrite(POWER_PIN, HIGH);
        delayMicroseconds(RISE_TIME);
        for(int j =0; j < PHOTO_TRANSISTOR_N ; j++){
          //Serial.println(phototransistor_array[j]);
          accum[j] +=  analogRead(phototransistor_array[j]);
        }
        digitalWrite(POWER_PIN, LOW);
        if(time_to_sleep_us > 16383)
            delay(time_to_sleep_us / 1e3);
        else
            delayMicroseconds(time_to_sleep_us);
    }
    for(int j =0; j < PHOTO_TRANSISTOR_N ; j++){
      accum[j] /= OVER_SAMPLING;
     }
    uint32_t now = realTimeMs();
    log(generateLogString(now, accum), file);

/*
    float delt_accum = accum -former_accum;
    rolling_mean = rolling_mean * (1.0 -LEARNING_RATE) + LEARNING_RATE *delt_accum;
    float abs_diff = abs(rolling_mean -delt_accum );
    rolling_sd = rolling_sd * (1.0 - LEARNING_RATE) + LEARNING_RATE * abs_diff;
    former_accum=accum;
    float z_score = abs_diff / rolling_sd;
    bool crossing = false;
    if(z_score > Z_SCORE_THRESHOLD){
      crossing = true;
      }*/
    

}


// arduino-cli  compile --fqbn  arduino:avr:uno  smart_trap.ino
