///////////////////////////////////////////////////////////////////
// main code - don't change if you don't know what you are doing //
///////////////////////////////////////////////////////////////////
const char FW_version[] PROGMEM = "2.1.0 igor";

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
ESP8266WebServer server(80);

#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>


#include <Wire.h>
#include "src/Adafruit_MCP23017/Adafruit_MCP23017.h"
Adafruit_MCP23017 mcp;
// Assign ports names
// Here is the naming convention:
// A0=0 .. A7=7
// B0=8 .. B7=15
// That means if you have A0 == 0 B0 == 8 (this is how it's on the board B0/A0 == 8/0)
// one more example B3/A3 == 11/3
#define A0 0
#define A1 1
#define A2 2
#define A3 3
#define A4 4
#define A5 5
#define A6 6
#define A7 7
#define B0 8
#define B1 9
#define B2 10
#define B3 11
#define B4 12
#define B5 13
#define B6 14
#define B7 15

#include "src/LiquidCrystal_I2C/LiquidCrystal_I2C.h"
LiquidCrystal_I2C lcd(0x27,16,2); // Check I2C address of LCD, normally 0x27 or 0x3F

#include "src/HX711/src/HX711.h"
const int LOADCELL_DOUT_PIN = D5;
const int LOADCELL_SCK_PIN = D6;
HX711 scale;

class Kalman { // https://github.com/denyssene/SimpleKalmanFilter
  private:
  float err_measure, err_estimate, q, last_estimate;  

  public:
  Kalman(float _err_measure, float _err_estimate, float _q) {
      err_measure = _err_measure;
      err_estimate = _err_estimate;
      q = _q;
  }

  float updateEstimation(float measurement) {
    float gain = err_estimate / (err_estimate + err_measure);
    float current_estimate = last_estimate + gain * (measurement - last_estimate);
    err_estimate =  (1.0 - gain) * err_estimate + fabs(last_estimate - current_estimate) * q;
    last_estimate = current_estimate;
    return last_estimate;
  }
  float getEstimation() {
    return last_estimate;
  }
};

Kalman displayFilter = Kalman(1400, 80, 0.15); // плавный
Kalman filter = Kalman(1000, 80, 0.4);         // резкий

#define PUMPS_NO 8
const char* names[PUMPS_NO]       = {pump1n, pump2n, pump3n, pump4n, pump5n, pump6n, pump7n, pump8n};
const byte pinForward[PUMPS_NO]   = {pump1,  pump2,  pump3,  pump4,  pump5,  pump6,  pump7,  pump8};
const byte pinReverse[PUMPS_NO]   = {pump1r, pump2r, pump3r, pump4r, pump5r, pump6r, pump7r, pump8r};
const int staticPreload[PUMPS_NO] = {pump1p, pump2p, pump3p, pump4p, pump5p, pump6p, pump7p, pump8p};
const char* stateStr[]            = {"Ready", "Working"};
enum State {READY, WORKING};

State state;
float goal[PUMPS_NO];
float curvol[PUMPS_NO];
float sumA,sumB;
int pumpWorking;
unsigned long sTime, eTime;

void setup() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {delay(500);}
  MDNS.begin("mixer");
  MDNS.addService("http", "tcp", 80);
  server.on("/api/meta", metaApi);
  server.on("/api/status", statusApi);
  server.on("/api/scales", scalesApi);
  server.on("/api/start", startApi);
  server.on("/api/tare", tareApi);
  server.on("/api/test", testApi);
  server.on("/", mainPage);
  server.on("/scales", scalesPage);
  server.on("/calibration", calibrationPage);
  server.on("/style.css", cssPage);
  server.begin();
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onEnd([]() {});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});
  ArduinoOTA.onError([](ota_error_t error) {});
  ArduinoOTA.begin();

  Wire.begin(D1, D2);
  //Wire.setClock(400000L);   // 400 kHz I2C clock speed
  lcd.begin(D1,D2);      // SDA=D1, SCL=D2               
  lcd.backlight();

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(scale_calibration_A); //A side
  lcd.setCursor(0, 0);
  lcd.print(F("Start FW: "));
  lcd.print(FPSTR(FW_version));
  lcd.setCursor(0, 1); 
  lcd.print(WiFi.localIP()); 
  scale.power_up();

  state = READY;

  server.handleClient();

  delay (3000);
  tareScalesWithCheck(255);
  lcd.clear();
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();
  scale.set_scale(scale_calibration_A);
  readScales(16);
  lcd.setCursor(0, 1);
  lcd.print(toString(rawToUnits(displayFilter.getEstimation()), 2));
  lcd.print(F("         "));
  lcd.setCursor(10, 0);
  lcd.print(stateStr[state]); 
}

String toString(float x, byte precision) {
  if (x < 0 && -x < pow(0.1, precision)) {
    x = 0;
  }
  return String(x, precision);
}

void metaApi() {
  String message((char*)0);
  message.reserve(255);
  message += F("{\n");
  append(message,    F("version"),     FPSTR(FW_version),         true,  false);
  append(message,    F("scalePointA"), scale_calibration_A,       false, false);
  append(message,    F("scalePointB"), scale_calibration_B,       false, false);  
  appendArr(message, F("names"),       names,                     true,  true);  
  message += '}'; 
  server.send(200, "application/json", message);
}

void scalesApi() {
  if (state != READY) {
    busyPage(); 
    return; 
  }
  
  float rawValue = displayFilter.getEstimation();
  String message((char*)0);
  message.reserve(255);
  message += F("{\n");
  append(message, F("value"),    rawToUnits(rawValue),  false, false);
  append(message, F("rawValue"), rawValue,              false, false);
  append(message, F("rawZero"),  scale.get_offset(),    false, true);
  message += '}';
  server.send(200, "application/json", message);
} 

void statusApi() {
  unsigned long ms = sTime == 0 ? 0 : (eTime == 0 ? millis() - sTime : eTime - sTime);  
  String message((char*)0);
  message.reserve(512);
  message += F("{\n");
  append(message,    F("state"),  stateStr[state],   true,  false);
  append(message,    F("timer"),  ms / 1000,         false, false);
  append(message,    F("sumA"),   sumA,              false, false);
  append(message,    F("sumB"),   sumB,              false, false);
  append(message,    F("pumpWorking"), pumpWorking,  false, false); 
  appendArr(message, F("goal"),   goal,              false, false);
  appendArr(message, F("result"), curvol,            false, true);
  message += '}';
  server.send(200, "application/json", message);  
}

void tareApi(){
  if (state == READY) {
    tareScalesWithCheck(255);
    okPage();
  } else {
    busyPage(); 
  }
}

void testApi(){
  if (state != READY) { 
    busyPage(); 
    return; 
  }  
  
  okPage();
  for (int i = 0; i < PUMPS_NO; i++) {
    lcd.home();lcd.print(F("Start "));lcd.print(names[i]);
    PumpStart(i);delay(3000);
    lcd.home();lcd.print(F("Revers       "));
    PumpReverse(i);delay(3000);
    lcd.home();lcd.print(F("Stop       "));
    PumpStop(i);delay(1000);
  }
}

void startApi() {
  if (state != READY) { 
    busyPage(); 
    return; 
  }
    
  sTime = millis();
  eTime = 0;
  sumA = 0;
  sumB = 0;
 
  for (byte i = 0; i < PUMPS_NO; i ++) {
    goal[i] = server.arg("p" + (i + 1)).toFloat();
    curvol[i] = 0;
  }
  
  okPage();

  state = WORKING;
  float offsetBeforePump = scale.get_offset();
  scale.set_scale(scale_calibration_A); //A side
  float rawStart = readScalesWithCheck(255);
  pumping(0);
  pumping(1);
  pumping(2);
  float rawMid = readScalesWithCheck(255);   
  sumA = (rawMid - rawStart) / scale_calibration_A;
  
  scale.set_scale(scale_calibration_B); //B side 
  pumping(3);
  pumping(4);
  pumping(5);
  pumping(6);
  pumping(7); 
  float rawEnd = readScalesWithCheck(255); 
  sumB = (rawEnd - rawMid) / scale_calibration_B;

  scale.set_offset(offsetBeforePump);
  eTime = millis();
  pumpWorking = 0;
  state = READY;

  reportToWega();

  delay(1000);
  lcd.clear();
}

void reportToWega() {
  String httpstr((char*)0);
  httpstr.reserve(512);
  httpstr += F(WegaApiUrl);
  httpstr += '?';
  for(byte i = 0; i < PUMPS_NO; i++) {
    httpstr += F("&p");
    httpstr += (i + 1);
    httpstr += '=';
    httpstr += String(curvol[i],3);
    httpstr += F("&v");
    httpstr += (i + 1);
    httpstr += '=';
    httpstr += String(goal[i],3);
  } 
  WiFiClient client;
  HTTPClient http;
  http.begin(client, httpstr);
  http.GET();
  http.end();
}

// Функции помп
float PumpStart(int n) {
  mcp.begin();
  mcp.pinMode(pinForward[n], OUTPUT); mcp.pinMode(pinReverse[n], OUTPUT);
  mcp.digitalWrite(pinForward[n], HIGH);mcp.digitalWrite(pinReverse[n], LOW);
}
float PumpStop(int n) {
  mcp.begin();
  mcp.pinMode(pinForward[n], OUTPUT); mcp.pinMode(pinReverse[n], OUTPUT);
  mcp.digitalWrite(pinForward[n], LOW);mcp.digitalWrite(pinReverse[n], LOW);
}
float PumpReverse(int n) {
  mcp.begin();
  mcp.pinMode(pinForward[n], OUTPUT); mcp.pinMode(pinReverse[n], OUTPUT);
  mcp.digitalWrite(pinForward[n], LOW);mcp.digitalWrite(pinReverse[n], HIGH);
}

void printValueAndPercent(float value, float targetValue) {
    lcd.setCursor(0, 1); 
    lcd.print(toString(value, 2)); 
    if (!isnan(targetValue)) {
      lcd.print(F(" (")); lcd.print(String(value/targetValue*100,1)); lcd.print(F("%)"));
    }
    lcd.print(F("    "));
    yield();
}

void pumpToValue(float capValue, float capMillis, float targetValue, int n, float allowedOscillation) {
  float value = rawToUnits(filter.getEstimation());
  if (value >= capValue) {
    return;
  }
  long endMillis = millis() + capMillis;
  float maxValue = value;
  PumpStart(n);
  char exitCode;
  long i = 0;
  while (true) { 
    if (value >= capValue) {
      exitCode = 'V'; // вес достиг заданный
      break;
    } else if (millis() >= endMillis) {
      exitCode = 'T'; // истекло время
      break;
    } else if (value < maxValue - allowedOscillation) {
      exitCode = 'E'; // аномальные показания весов
      break;
    }
    server.handleClient();
    readScales(1);
    value = rawToUnits(filter.getEstimation());
    maxValue = max(value, maxValue);
    if (i % 10 == 0) printValueAndPercent(value, targetValue);
    i++;
  }
  PumpStop(n);
  printValueAndPercent(value, targetValue);
  lcd.setCursor(15, 1);lcd.print(exitCode);
}

// Функция налива
float pumping(int n) {
  pumpWorking = n;
  float wt = goal[n];
  server.handleClient();

  if (wt <= 0) {
    lcd.setCursor(0, 0); lcd.print(names[n]);lcd.print(F(":"));lcd.print(wt);
    lcd.setCursor(10, 0);
    lcd.print(F("SKIP..   "));
    server.handleClient();
    delay (1000);
    return 0;
  }

  lcd.clear(); lcd.setCursor(0, 0); lcd.print(names[n]);lcd.print(F(" Tare..."));
  float value = 0;
  tareScalesWithCheck(255);
  server.handleClient();
  delay(10);

  int preload;
  if (wt < 0.5) { // статический прелоад
    preload = staticPreload[n];
    lcd.clear(); lcd.setCursor(0, 0); lcd.print(names[n]);lcd.print(F(" Reverse..."));
    PumpReverse(n);
    delay(preload);
    PumpStop(n);
    server.handleClient();
    delay(10);
  
    lcd.clear(); lcd.setCursor(0, 0); lcd.print(names[n]);lcd.print(F(" Preload..."));
    lcd.setCursor(0, 1);lcd.print(F(" Preload="));lcd.print(preload);lcd.print(F("ms"));
    PumpStart(n);
    delay(preload);
    PumpStop(n);
    server.handleClient();
    delay(10);

    value = rawToUnits(readScalesWithCheck(128));
    server.handleClient();
    delay(10);
  } else { // прелоад до первой капли
    lcd.clear(); lcd.setCursor(0, 0); lcd.print(names[n]);lcd.print(F(" Preload..."));
    preload = 0;
    while (value < 0.02) {
      long startTime = millis();
      pumpToValue(0.03, staticPreload[n] * 2, NAN, n, 0.1);
      preload += millis() - startTime;
      value = rawToUnits(readScalesWithCheck(128));
      curvol[n]=value;
      server.handleClient();
      delay(10);
    }
    lcd.setCursor(0, 1);lcd.print(F(" Preload="));lcd.print(preload);lcd.print(F("ms"));
    delay(1000);
  }
  
  // до конечного веса минус 0.2 - 0.5 грамм по половине от остатка 
  lcd.clear(); lcd.setCursor(0, 0); lcd.print(names[n]);lcd.print(F(":"));lcd.print(wt);lcd.print(F(" Fast..."));
  float performance = 0.0007;
  float dropTreshold = wt - value > 1.0 ? 0.2 : 0.5; // определяет сколько оставить на капельный налив
  float valueToPump = wt - dropTreshold - value;
  float allowedOscillation = valueToPump < 1.0 ? 1.5 : 3.0;  // допустимое раскачивание, чтобы остановитmся при помехах/раскачивании, важно на малых объемах  
  if (valueToPump > 0.3) { // если быстро качать не много то не начинать даже
    while ((valueToPump = wt - dropTreshold - value) > 0) {
      if (valueToPump > 0.2) valueToPump = valueToPump / 2;  // качать по половине от остатка
      long timeToPump = valueToPump / performance;           // ограничение по времени
      long startTime = millis();
      pumpToValue(curvol[n] + valueToPump, timeToPump, wt, n, allowedOscillation);
      long endTime = millis();
      value = rawToUnits(readScalesWithCheck(128));
      if (endTime - startTime > 200 && value-curvol[n] > 0.15) performance = max(performance, (value - curvol[n]) / (endTime - startTime));
      curvol[n]=value;
      server.handleClient();
      delay(10);
    }
  }
  
  // капельный налив
  lcd.clear(); lcd.setCursor(0, 0); lcd.print(names[n]);lcd.print(F(":"));lcd.print(wt);lcd.print(F(" Drop..."));
  float prevValue = value;
  int sk = 25;
  while (value < wt - 0.01) {
    lcd.setCursor(0, 1);
    lcd.print(value, 2);
    lcd.print(F(" ("));
    lcd.print(value / wt * 100, 0);
    lcd.print(F("%) "));
    lcd.print(sk);
    lcd.print(F("ms     "));
      
    if (value - prevValue < 0.01) {sk = min(80, sk+2);}
    if (value - prevValue > 0.01) {sk = max(2, sk-2);}
    if (value - prevValue > 0.1 ) {sk = 0;}

    prevValue = value;
    PumpStart(n);
    delay(sk);
    PumpStop(n);

    server.handleClient();
    delay(100);
    value = rawToUnits(readScalesWithCheck(128));
    curvol[n] = value;
  }

  lcd.setCursor(0, 1);
  lcd.print(String(value, 2));
  lcd.print(F(" ("));
  lcd.print(value/wt*100, 2);
  lcd.print(F("%)      "));

  // реверс, высушить трубки
  PumpReverse(n);
  long endPreloadTime = millis() + max(preload, staticPreload[n]) * 1.5; 
  while (millis() < endPreloadTime) {
    server.handleClient();
    delay(10);
  }
  PumpStop(n);
    
  return value;
}

// Функции для работы с весами
float readScales(int times) {
  float sum = 0;
  for (int i = 0; i < times; i++) {
    float value = scale.read();
    sum += value;
    displayFilter.updateEstimation(value);
    filter.updateEstimation(value);
  }
  return sum / times;
}

float readScalesWithCheck(int times) {
  float value1 = readScales(times / 2);
  while (true) {
    delay(20);
    float value2 = readScales(times / 2);
    if (fabs(value1 - value2) < fabs(0.01 * scale_calibration_A)) {
      return (value1 + value2) / 2;
    }
    value1 = value2;
  }
}

void tareScalesWithCheck(int times) {
  scale.set_offset(readScalesWithCheck(times));
}

float rawToUnits(float raw) {
  return (raw - scale.get_offset()) / scale.get_scale();
}

float rawToUnits(float raw, float calibrationPoint) {
  return (raw - scale.get_offset()) / calibrationPoint;
}

// функции для генерации json
template<typename T>
void append(String& src, const __FlashStringHelper* name, const T& value, bool quote, boolean last) {
  src += '"'; src += name; src += F("\":");
  if (quote) src += '"';
  src += value;
  if (quote) src += '"';
  if (!last) src += ',';  
  src += '\n';
}

template<typename T>
void appendArr(String& src, const __FlashStringHelper* name, T value[PUMPS_NO], bool quote, bool last) {
  src += '"'; src += name; src += F("\":");
  src += '[';
  for (int i = 0; i < PUMPS_NO; i++) {
    if (quote) src += '"';
    src += value[i];
    if (quote) src += '"';
    if (i < (PUMPS_NO - 1)) src += ',';
  }
  src += ']';
  if (!last) src += ',';  
  src += '\n';
}
