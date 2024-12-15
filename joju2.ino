#include <Arduino.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ArduinoOTA.h>
#include "time.h"
#include <ADS1115_WE.h>
#include <Adafruit_INA219.h>
#include "Adafruit_SHT31.h"
Adafruit_SHT31 sht31 = Adafruit_SHT31();
Adafruit_INA219 ina219;
#define I2C_ADDRESS 0x48

ADS1115_WE adc = ADS1115_WE(I2C_ADDRESS);

const char* ssid = "mikesnet";
const char* password = "springchicken";

#define CAMERA_PIN     0
#define SLEEP_MINS     5       * 60 //5 minutes in seconds
#define TIMEOUT_MINS   5       * 60 * 1000 //30 minutes in milliseconds


const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;  //Replace with your GMT offset (secs)
const int daylightOffset_sec = 0;   //Replace with your daylight offset (secs)
int hours, mins, secs;
bool camerapower = false;
char auth[] = "fEZlZOio7CS1nBXNm3A8HR5DysrzIoYW";

float tempSHT, humSHT;
int16_t adc0, adc1, adc2, adc3, soil;
float volts0, volts1, volts2, volts3;
float wifi;
unsigned long boottime;


float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
float power_mW = 0;

bool buttonstart = false;
bool cameraison = false;

WidgetTerminal terminal(V10);

#define every(interval) \
  static uint32_t __every__##interval = millis(); \
  if (millis() - __every__##interval >= interval && (__every__##interval = millis()))

BLYNK_WRITE(V10) {
  if (String("help") == param.asStr()) {
    terminal.println("==List of available commands:==");
    terminal.println("wifi");
    terminal.println("sleep");
    terminal.println("print");
    terminal.println("camera");
    terminal.println("==End of list.==");
  }
  if (String("wifi") == param.asStr()) {
    terminal.print("Connected to: ");
    terminal.println(ssid);
    terminal.print("IP address:");
    terminal.println(WiFi.localIP());
    terminal.print("Signal strength: ");
    terminal.println(WiFi.RSSI());
    printLocalTime();
  }
  if (String("sleep") == param.asStr()) {
    terminal.println("");
    printLocalTime();
    terminal.println("Going to sleep...");
    digitalWrite(CAMERA_PIN, HIGH);
    delay(500);
    digitalWrite(CAMERA_PIN, LOW);
    delay(100);
    terminal.flush();
    Blynk.run();
    delay(100);
    gotosleep(SLEEP_MINS);
  }
  if (String("print") == param.asStr()) {
    tempSHT = sht31.readTemperature();
    volts3 = 2.0 * readChannel(ADS1115_COMP_3_GND);
    current_mA = ina219.getCurrent_mA();
    terminal.print("TempSHT: ");
    terminal.println(tempSHT);
    terminal.print("Volts3: ");
    terminal.println(volts3);
    terminal.print("INA mA: ");
    terminal.println(current_mA);
  }
  if (String("camera") == param.asStr()) {
    digitalWrite(CAMERA_PIN, HIGH);
    delay(500);
    digitalWrite(CAMERA_PIN, LOW);
    delay(100);
    cameraison = !cameraison;
    terminal.print("Camera status is now: ");
    terminal.println(cameraison);
  }
  terminal.flush();
}


void gotosleep(int sleeptimeSecs) {
  WiFi.disconnect();
  ina219.powerSave(1);
  SPI.end();
  Wire.end();
  pinMode(SS, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
  //  pinMode(I2C_PIN, INPUT );
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);

  pinMode(5, INPUT_PULLUP);
  pinMode(CAMERA_PIN, INPUT);

  esp_sleep_enable_timer_wakeup(sleeptimeSecs * 1000000ULL);
  delay(1);
  esp_deep_sleep_start();
  //esp_light_sleep_start();
  delay(1000);
}

BLYNK_WRITE(V11)
{
  if (param.asInt() == 1) {buttonstart = true;}
  if (param.asInt() == 0) {buttonstart = false;}
}


BLYNK_WRITE(V12) {
  if (param.asInt() == 1) { camerapower = true; }
  if (param.asInt() == 0) { camerapower = false; }
}

BLYNK_CONNECTED() {
  Blynk.syncVirtual(V11);
  Blynk.syncVirtual(V12);
}


void printLocalTime() {
  time_t rawtime;
  struct tm* timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  terminal.print(asctime(timeinfo));
}

float readChannel(ADS1115_MUX channel) {
  float voltage = 0.0;
  adc.setCompareChannels(channel);
  adc.startSingleMeasurement();
  while (adc.isBusy()) {}
  voltage = adc.getResult_V();  // alternative: getResult_mV for Millivolt
  return voltage;
}

void setup(void) {
  Serial.begin(115200);
  //pinMode(I2C_PIN, OUTPUT);
  //digitalWrite(I2C_PIN, HIGH);
  //delay(100);
  Serial.println("ADC init");
  Wire.begin();
  adc.init();
  adc.setVoltageRange_mV(ADS1115_RANGE_4096);
  volts3 = 2.0 * readChannel(ADS1115_COMP_3_GND);
  volts2 = 2.0 * readChannel(ADS1115_COMP_2_GND);
  Serial.println("SHT init");
  sht31.begin(0x44);
  Serial.println("INA init");
  ina219.powerSave(0);
  ina219.begin();
  ina219.powerSave(0);
  ina219.setCalibration_16V_400mA();
  tempSHT = sht31.readTemperature();
  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  power_mW = ina219.getPower_mW();
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  
  pinMode(CAMERA_PIN, OUTPUT);
  digitalWrite(CAMERA_PIN, LOW);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // Wait for connection
  while ((WiFi.status() != WL_CONNECTED) && (millis() < 20000)) {
    if (millis() > 10000) {
      WiFi.setTxPower(WIFI_POWER_8_5dBm);
      Serial.print("!");
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

    Blynk.config(auth, IPAddress(192, 168, 50, 197), 8080);
  Blynk.connect();
  while ((!Blynk.connected()) && (millis() < 20000)){delay(250);}


  Blynk.virtualWrite(V1, tempSHT);
  if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
  Blynk.virtualWrite(V3, volts2);
  if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
  Blynk.virtualWrite(V4, volts3);
  if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
  Blynk.virtualWrite(V5, WiFi.RSSI());
  if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
  Blynk.virtualWrite(V21, shuntvoltage);
  if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
  Blynk.virtualWrite(V22, busvoltage);
  if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
  Blynk.virtualWrite(V23, current_mA);
  if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
  Blynk.virtualWrite(V24, power_mW);
  if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
  Blynk.virtualWrite(V25, loadvoltage);
  if (WiFi.status() == WL_CONNECTED) {Blynk.run();}

  Blynk.virtualWrite(V25, loadvoltage);
  if (WiFi.status() == WL_CONNECTED) {Blynk.run();}
  boottime = millis();
  if (buttonstart) {

    ArduinoOTA.setHostname("Joju2");
    ArduinoOTA.begin();
    Serial.println("HTTP server started");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    hours = timeinfo.tm_hour;
    mins = timeinfo.tm_min;
    secs = timeinfo.tm_sec;
    terminal.println("***JOJU 2.1 STARTED***");
    terminal.print("Connected to ");
    terminal.println(ssid);
    terminal.print("IP address: ");
    terminal.println(WiFi.localIP());
    printLocalTime();
    terminal.print("Boot time: ");
    terminal.println(boottime);
    terminal.flush();
    if (camerapower) {
      cameraison = true;
      digitalWrite(CAMERA_PIN, HIGH);
      delay(500);
      digitalWrite(CAMERA_PIN, LOW);
      terminal.println("Turning camera on...");
    }
    
    if (WiFi.status() == WL_CONNECTED) {Blynk.run();} 
  }
  else {
    gotosleep(SLEEP_MINS);
  }
}

void loop() {
  Blynk.run();
  

  if (!buttonstart) {
    terminal.println("");
    printLocalTime();
    terminal.println("Going to sleep...");
    if (cameraison) {
      digitalWrite(CAMERA_PIN, HIGH);
      delay(500);
      digitalWrite(CAMERA_PIN, LOW);
      delay(100);
    }
    terminal.flush();
    Blynk.run();
    delay(100);
    gotosleep(SLEEP_MINS);    
  }
  ArduinoOTA.handle();
  every(10000) {
    tempSHT = sht31.readTemperature();
    volts3 = 2.0 * readChannel(ADS1115_COMP_3_GND);
    volts2 = 2.0 * readChannel(ADS1115_COMP_2_GND);
    shuntvoltage = ina219.getShuntVoltage_mV();
    busvoltage = ina219.getBusVoltage_V();
    current_mA = ina219.getCurrent_mA();
    power_mW = ina219.getPower_mW();
    loadvoltage = busvoltage + (shuntvoltage / 1000);
    Blynk.virtualWrite(V1, tempSHT);
    Blynk.virtualWrite(V3, volts2);
    Blynk.virtualWrite(V4, volts3);
    Blynk.virtualWrite(V5, WiFi.RSSI());
    Blynk.virtualWrite(V21, shuntvoltage);
    Blynk.virtualWrite(V22, busvoltage);
    Blynk.virtualWrite(V23, current_mA);
    Blynk.virtualWrite(V24, power_mW);
    Blynk.virtualWrite(V25, loadvoltage);
  }

  if (millis() > TIMEOUT_MINS) {gotosleep(SLEEP_MINS);}
}