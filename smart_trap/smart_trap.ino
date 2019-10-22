#include <RTClib.h>
#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>
#include <SerialCommands.h>
//#include <dht_nonblocking.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>


//#include <sdios.h>

//#define SKIP_FATAL 
#define ERROR_MESSAGE_PREFIX F("#!")
#define BAUD_RATE 57600
#define OVER_SAMPLING 32
#define FS 8 // in Hz
#define RISE_TIME 50 // in microseconds
#define POWER_PIN 3
#define CLOCK_ADJUST_LEARN_RATE 0.005 // > (1 - 0.005)^150 = 0.47 
#define PHOTO_TRANSISTOR_N 4
#define PHOTO_TRANSISTOR_PINS {0,1,2,3}
#define LEARNING_RATE 0.01 // alpha
#define Z_SCORE_THRESHOLD 4.753424 //R> qnorm(1 - 10^(-6),0,1)
#define ERROR_LED 9
#define METADATA_FILENAME "meta.txt"
#define COLUMNS F("t,temp_C,rel_hum,trap_1,trap_2,trap_3,trap_4")

#define DHTPIN 2
#define DHT_SAMPLING_TIME 600000 //Temp plus humidity updated every s 
#define DHTTYPE DHT11     // DHT 11


#define error(msg) sd.errorHalt(F(msg))

// Log file base name.  Must be 2 characters.
#define FILE_BASE_NAME "st"

const float time_to_sleep_us = 1e6 / (FS * OVER_SAMPLING) - RISE_TIME;

uint32_t last_temp_read_t = 0;

RTC_DS1307 RTC;
RTC_Millis soft_rtc;

// SD chip select pin.  Be sure to disable any other SPI devices such as Enet.
const uint8_t chipSelect = SS;
SdFat sd;
SdFile file;
SdFile metadata_file;
String header = "#";
String device_id = "NA";
bool started = false;
bool clock_adjusted = false;

int phototransistor_array[PHOTO_TRANSISTOR_N]= {0,1,2,3};
/*float rolling_mean[PHOTO_TRANSISTOR_N];
float rolling_sd[PHOTO_TRANSISTOR_N];
float former_accum[PHOTO_TRANSISTOR_N];*/

unsigned long real_time_ms=0; //time since boot in ms
float drift_avg_s=0;// the drift between the arduino milli clock and the rtc
char serial_command_buffer_[32];


float temperature = 0.0/0.0; // NaN
float humidity = 0.0/0.0;
DHT_Unified dht(DHTPIN, DHTTYPE);


//This is the default handler, and gets called when no other command matches. 
// Note: It does not get called for one_key commands that do not match
void cmd_unrecognized(SerialCommands* sender, const char* cmd){
  sender->GetSerial()->print(F("Unrecognized command ["));
  sender->GetSerial()->print(cmd);
  sender->GetSerial()->println(F("]"));
}

void cmd_set_datetime(SerialCommands* sender){
	char* datetime = sender->Next();
	DateTime now = RTC.now();
    char datetime_old[17];
    sprintf (datetime_old, "%04d%02d%02d %02d:%02d:%02d",now.year(),now.month(),now.day(), now.hour(), now.minute(), now.second());
	sender->GetSerial()->print( F("# Old time: "));
	sender->GetSerial()->println(datetime_old);

	if (datetime == NULL){
		sender->GetSerial()->println(F("#ERROR NO_DATETIME!"));
		return;
	}	
	adjustClock(datetime);
    now = RTC.now();
    char datetime_new[17];
    sprintf (datetime_new, "%04d%02d%02d %02d:%02d:%02d",now.year(),now.month(),now.day(), now.hour(), now.minute(), now.second());

	sender->GetSerial()->print( F("# New time: "));
	sender->GetSerial()->println(datetime_new);
			
}

void cmd_get_info(SerialCommands* sender){
	String message = F("#*");
	message += F("header: ");
	message += header + "|";
	sender->GetSerial()->println(message);
		
}

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
  
void adjustClock(char* datetime){
	//info();
	uint32_t unixtime = 946684800 + (uint32_t) atol(datetime);
	RTC.adjust(DateTime(unixtime));
	clock_adjusted = true;
	delay(500);
	//info();
  }
   
void initOutputFile(SdFat *sd, SdFile *file){
    const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
    char fileName[13] = FILE_BASE_NAME "0000.csv";

    while (sd->exists(fileName)) {
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
          fatalError(F("Cannot create filename"), 3, *file);
        }
      }
  if (!file->open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
     fatalError(F("Cannot open file"), 3, *file);
  }
}

void initTime(RTC_DS1307 *RTC, RTC_Millis *soft_rtc){
    
    Wire.begin();
    if (!RTC->begin()) {
      fatalError(F("No clock"), 4, file);
    }
    
    DateTime compilation_date(__DATE__, __TIME__);    
    DateTime  now = RTC->now();
		    
	uint32_t now_ut = now.unixtime();
    uint32_t compilation_date_ut = compilation_date.unixtime();
    // one day in tyhe past to account for time zones
    if(now_ut < compilation_date_ut && compilation_date_ut -  now_ut > 84200L){
		fatalError(F("Clock set before compilation time"), 4, file);
		}
    
	if(now_ut > compilation_date_ut &&  now_ut - compilation_date_ut > 157680000L){
		fatalError(F("Clock set 5 years past compilation time"), 4, file);
		}
    soft_rtc->begin(RTC->now());
}

String generateHeader(RTC_DS1307 rtc, String device_id){
    header = "#";
    DateTime now = rtc.now();
    char datetime[17];
    sprintf (datetime, "%04d%02d%02d %02d:%02d:%02d",now.year(),now.month(),now.day(), now.hour(), now.minute(), now.second());
    header += datetime;
    header += ",";
    header += device_id;
    header += ",";
    header += clock_adjusted;
    return header;
}

// Log a data record.
String generateLogString(uint32_t t, float temperature, float humidity, float x[]){
  String out = "";
  out += t;
  out += ',';
  out += temperature;
  out += ',';
  out += humidity;
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
    error("#Write error");
  }
}

void logSerial(String string){
  Serial.println(string);
}

void fatalError(String message, char status, SdFile file){
	String m = ERROR_MESSAGE_PREFIX;
	m += message;
    log(m, file);
    unsigned int i = 0;
    while(true){
        if(i %10 < status){
           digitalWrite(ERROR_LED, HIGH);
        }
        delay(10);
        digitalWrite(ERROR_LED, LOW);
        delay(490);
        i++;
#ifdef SKIP_FATAL 
		return ;
#endif
    }
}



bool updateTemperatureHumidity(uint32_t time, bool force=false){
	if(time - last_temp_read_t > DHT_SAMPLING_TIME || force){
		sensors_event_t event;
		dht.temperature().getEvent(&event);
		
		if (isnan(event.temperature)){
			return false;
		}
		temperature = event.temperature;
		
		dht.humidity().getEvent(&event);
		if (isnan(event.relative_humidity)){
			return false;
		}
			
		humidity = event.relative_humidity;
		last_temp_read_t = time;
	}
	return true;
}

SerialCommands serial_commands_(&Serial, serial_command_buffer_, sizeof(serial_command_buffer_), "\r\n", " ");
SerialCommand cmd_set_datetime_("sdt", cmd_set_datetime);
SerialCommand cmd_get_info_("gi", cmd_get_info);


void setup(void) {
    Serial.begin(BAUD_RATE);
    pinMode(POWER_PIN, OUTPUT);
    pinMode(ERROR_LED, OUTPUT);
    digitalWrite(ERROR_LED, HIGH);
    delay(1000);
    digitalWrite(ERROR_LED, LOW);


	serial_commands_.SetDefaultHandler(cmd_unrecognized);
	serial_commands_.AddCommand(&cmd_set_datetime_);
	serial_commands_.AddCommand(&cmd_get_info_);

  // Initialize at the highest speed supported by the board that is
  // not over 50 MHz. Try a lower speed if SPI errors occur.
	bool sd_ok = sd.begin(chipSelect, SD_SCK_MHZ(50));
	if (!sd_ok) {
       fatalError(F("Cannot connect to sd card reader"), 1, file);
	}
	else if(!metadata_file.open(METADATA_FILENAME, O_READ)){
		fatalError(F("No metadata file"), 2, file);
	}	
	else{
		device_id = "";
		for(int i=0; i != 3; ++i){
			device_id += (char) metadata_file.read();
		}
		metadata_file.close();
	}
	if(sd_ok){
		initOutputFile(&sd, &file);
	}
    initTime(&RTC, &soft_rtc);
 
    // todo check if can probe temp. otherwise, error 5
	dht.begin();	
	sensor_t sensor;
	dht.temperature().getSensor(&sensor);
	dht.humidity().getSensor(&sensor);
	
	if(!updateTemperatureHumidity(realTimeMs(), true)){
		fatalError(F("Fail to read DHT11"), 5, file);
	}
	while(!clock_adjusted && realTimeMs() < 10000L ){
		delay(50);
		serial_commands_.ReadSerial();
	}	
	
    log(generateHeader(RTC, device_id),file);
    log(COLUMNS,file);
}

	
void loop(void) {
	serial_commands_.ReadSerial();
    float accum[PHOTO_TRANSISTOR_N] = {0};
    
    
    for(int i =0; i < OVER_SAMPLING; i++){
        digitalWrite(POWER_PIN, HIGH);
        delayMicroseconds(RISE_TIME);
        for(int j =0; j < PHOTO_TRANSISTOR_N ; j++){;
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
    
	updateTemperatureHumidity(now);
    log(generateLogString(now, temperature, humidity, accum), file);
	//Serial.println(millis());
	//delay(500);
    
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
