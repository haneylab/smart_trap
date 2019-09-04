#include <RTClib.h>
#include <Wire.h>


#define OVER_SAMPLING 128
#define FS 4.0 // in Hz
#define RISE_TIME 50 // in microseconds
#define POWER_PIN 3
#define PHOTO_TRANSISTOR_PIN 1
#define LEARNING_RATE 0.01 // alpha
#define  Z_SCORE_THRESHOLD 4.753424 //R> qnorm(1 - 10^(-6),0,1)

const float time_to_sleep_us = 1e6 / (FS * OVER_SAMPLING) - RISE_TIME;


RTC_DS1307 RTC;

float rolling_mean = 0;
float rolling_sd =0;
float former_accum = 0;

void setup(void) {
    Serial.begin(57600);
    RTC.begin();
    
    Wire.begin();
    
    if (! RTC.isrunning()) {
      Serial.println("RTC is NOT running!");
      // This will reflect the time that your sketch was compiled
      RTC.adjust(DateTime(__DATE__, __TIME__));
    }
    pinMode(POWER_PIN, OUTPUT);

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

    DateTime now = RTC.now(); 
    

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
    

}
