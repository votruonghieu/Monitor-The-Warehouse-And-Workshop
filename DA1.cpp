#include <WiFi.h>
#include "time.h"
#include "sntp.h"
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFiManager.h>
#include <Button2.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <EEPROM.h>
#include <BlynkSimpleEsp32.h> // Thêm thư vi
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6l-vlcK7W"
#define BLYNK_TEMPLATE_NAME "DoAn2"
#define BLYNK_AUTH_TOKEN "ZYkdM9D7GR6fcplqion
boolean bt1_state = LOW;
boolean bt2_state = LOW;
boolean bt3_state = LOW;
#define DHT_PIN 15
#define DHT_TYPE DHT11
#define bt 4
#define r1 16
#define r2 17
#define MQ_PIN 34
#define FLAME_PIN 35
#define Relay1 33
#define Relay2 32
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);
TaskHandle_t Task1;
TaskHandle_t Task2;
SemaphoreHandle_t lcdSemaphore;
volatile bool switchPage = false;
volatile int currentPage = 1;
unsigned long lastDebounceTime = 0;
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 0;
BlynkTimer timer;
WiFiManager wifiManager;
Button2 resetButton(bt);
byte customChar[] = { B00100, B01010, B00100, B00000, B00000, B00000, B00000,
B00000 };
void setup() {
lcd.init();
lcd.backlight();
dht.begin();
EEPROM.begin(512);
lcd.setCursor(2, 0);
lcd.print("DISCONNECT!!");
Serial.begin(9600);
lcd.createChar(0, customChar);
delay(3000);
resetButton.setLongClickHandler([](Button2& btn) {
wifiManager.resetSettings();
delay(1000);
ESP.restart();
});
wifiManager.autoConnect("Hệ thống nhà kho");
while (WiFi.status() != WL_CONNECTED) {
delay(1000);
}
configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
while (!time(nullptr)) {
delay(1000);
}
lcdSemaphore = xSemaphoreCreateMutex();
xTaskCreatePinnedToCore(task1, "Task1", 10000, NULL, 1, &Task1, 1);
xTaskCreatePinnedToCore(task2, "Task2", 10000, NULL, 1, &Task2, 0);
attachInterrupt(
digitalPinToInterrupt(2),
switchPageButtonInterrupt,
RISING);
Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());
pinMode(Relay1, OUTPUT);
pinMode(Relay2, OUTPUT);
pinMode(r1, INPUT_PULLUP);
pinMode(r2, INPUT_PULLUP);
pinMode(FLAME_PIN, INPUT);
timer.setInterval(100L, sendSensor);
}
BLYNK_WRITE(V0) {
int p = param.asInt();
digitalWrite(Relay1, p);
}
BLYNK_WRITE(V1) {
int p = param.asInt();
digitalWrite(Relay2, p);
}
void sendSensor() {
float temperature = dht.readTemperature();
float humidity = dht.readHumidity();
int mqValue = analogRead(MQ_PIN);
int flameValue = digitalRead(FLAME_PIN);
// You can send any value at any time.
// Please don't send more that 10 values per second.
Blynk.virtualWrite(V3, temperature);
Blynk.virtualWrite(V4, humidity);
Blynk.virtualWrite(V5, mqValue);
if (mqValue > 500) {
Blynk.logEvent("canhbaogas", "Phát hiện rò rỉ khí gas!!!!");
}
if (flameValue == 0) {
Blynk.logEvent("canhbaolua", "Phát hiện có hỏa hoạn!!!!");
}
}
void loop() {
resetButton.loop();
}
void check_button() {
if (digitalRead(r1) == HIGH) {
if (bt1_state == LOW) {
digitalWrite(Relay1, !digitalRead(Relay1));
Blynk.virtualWrite(V0, digitalRead(Relay1));
bt1_state = HIGH;
delay(200);
}
} else {
bt1_state = LOW;
}
if (digitalRead(r2) == HIGH) {
if (bt2_state == LOW) {
digitalWrite(Relay2, !digitalRead(Relay2));
Blynk.virtualWrite(V1, digitalRead(Relay2));
bt2_state = HIGH;
delay(200);
}
} else {
bt2_state = LOW;
}
}void task1(void* parameter) {
for (;;) {
check_button();
if (currentPage == 1) {
display1();
}
delay(1000);
}
}
void task2(void* parameter) {
for (;;) {
if (currentPage == 2) {
display2();
}
Blynk.run();
timer.run();
delay(1000);
}
}
void switchPageButtonInterrupt() {
unsigned long currentMillis = millis();
if (currentMillis - lastDebounceTime > 200) {
switchPage = true;
}
lastDebounceTime = currentMillis;
currentPage = (currentPage == 1) ? 2 : 1;
}
void display1() {
time_t now = time(nullptr);
struct tm* timeinfo;
timeinfo = localtime(&now);
xSemaphoreTake(lcdSemaphore, portMAX_DELAY);
lcd.clear();
lcd.setCursor(0, 0);
lcd.print("Ngay: ");
lcd.print(getFormattedDate(timeinfo));
lcd.setCursor(0, 1);
lcd.print("Gio: ");
lcd.print(getFormattedTime(timeinfo));
xSemaphoreGive(lcdSemaphore);
}
void display2() {
int temperature = dht.readTemperature();
int humidity = dht.readHumidity();
int mqValue = analogRead(MQ_PIN);55
int flameValue = digitalRead(FLAME_PIN);
xSemaphoreTake(lcdSemaphore, portMAX_DELAY);
lcd.clear();
lcd.setCursor(0, 0);
lcd.print("T:");
lcd.print(temperature);
lcd.write(0);
lcd.print("C");
lcd.setCursor(9, 0);
lcd.print("H:");
lcd.print(humidity);
lcd.print("%");
lcd.setCursor(0, 1);
lcd.print("G:");
lcd.print(mqValue);
lcd.setCursor(9, 1);
lcd.print("F:");
lcd.print(flameValue);
xSemaphoreGive(lcdSemaphore);
}
String getFormattedDate(struct tm* timeinfo) {
char buffer[20];
sprintf(buffer, "%02d/%02d/%04d",
timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
return String(buffer);
}
String getFormattedTime(struct tm* timeinfo) {
char buffer[20];
sprintf(buffer, "%02d:%02d:%02d",
timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
return String(buffer);
}