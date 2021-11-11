#include <Arduino.h>

#include <SPI.h>
#include "Secret.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ESP8266WiFi.h>
#include <ArduinoHA.h>
#include <cmath>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(D6, DHT11);

WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);

//Define the sensors and/or devices
//The string must not contain any spaces!!! Otherwise the sensor will not show up in Home Assistant
HASensor sensorOwner("Owner");
HASensor sensorLong("Long");
HASensor sensorLat("Lat");
HASensor sensorTemperature("Temperature");
HASensor sensorHumidity("Humidity");
HASensor sensorSignalstrength("Signal_strength");
HASensor sensorLight("Illuminance");
HASensor sensorWind("Wind");

const int windMeasurementsAmount = 50;
const int lightMeasurementsAmount = 10;

unsigned long lastTemperatureSend = millis();
float temperature;
int humidity;
int light;
int currentLightMeasurementIndex = 0;
int lightMeasurements[lightMeasurementsAmount] = {};
int wind;
float signalstrength;

volatile unsigned long lastWindRotation = millis();
volatile int currentRotationIndex = 0;
volatile int timeBetweenRotations[windMeasurementsAmount] = {};
volatile int lastTimeBetweenRotations;

void IRAM_ATTR processWindRotation()
{
  int time = millis() - lastWindRotation;
  timeBetweenRotations[currentRotationIndex] = millis() - lastWindRotation;
  lastTimeBetweenRotations = timeBetweenRotations[currentRotationIndex];
  currentRotationIndex++;
  if (currentRotationIndex > windMeasurementsAmount)
  {
    currentRotationIndex = 0;
  }
  lastWindRotation = millis();
}

void startWindMonitor()
{
  int start = millis();
  while (digitalRead(D5) && millis() - start < 1000)
  {
    // wait until low, max 1 sec
  }

  lastWindRotation = millis();

  start = millis();
  while (!digitalRead(D5) && millis() - start < 1000)
  {
    // wait until high again, max 1 sec
  }

  attachInterrupt(digitalPinToInterrupt(D5), processWindRotation, FALLING);
}

void stopWindMonitor()
{
  detachInterrupt(D5);
}

void setLightBrightness(int brightness) {
  WiFiClient yeelightClient;
  if(!yeelightClient.connect("192.168.135.41", 55443)){
    Serial.println("Failed connecting to yeelight");
    return;
  }
  yeelightClient.println("{\"id\":1, \"method\": \"set_bright\", \"params\":[" + String(brightness) + ", \"smooth\", 1000]}");
  yeelightClient.stop();
}

void setupMqtt()
{
  // Set sensor and/or device names
  // String conversion for incoming data from Secret.h
  String student_id = STUDENT_ID;
  String student_name = STUDENT_NAME;

  //Add student ID number with sensor name
  String stationName = student_name + "'s Home Station";
  String ownerName = student_id + " Station owner";
  String longName = student_id + " Long";
  String latName = student_id + " Lat";
  String temperatureName = student_id + " Temperature";
  String humidityName = student_id + " Humidity";
  String lightName = student_id + " Illuminance";
  String windName = student_id + " Wind";
  String signalstrengthName = student_id + " Signal Strength";

  //Set main device name
  device.setName(stationName.c_str());
  device.setSoftwareVersion(SOFTWARE_VERSION);
  device.setManufacturer(STUDENT_NAME);
  device.setModel(MODEL_TYPE);

  sensorOwner.setName(ownerName.c_str());
  sensorOwner.setIcon("mdi:account");

  sensorLong.setName(longName.c_str());
  sensorLong.setIcon("mdi:crosshairs-gps");
  sensorLat.setName(latName.c_str());
  sensorLat.setIcon("mdi:crosshairs-gps");

  sensorTemperature.setName(temperatureName.c_str());
  sensorTemperature.setDeviceClass("temperature");
  sensorTemperature.setUnitOfMeasurement("°C");

  sensorHumidity.setName(humidityName.c_str());
  sensorHumidity.setDeviceClass("humidity");
  sensorHumidity.setUnitOfMeasurement("%");

  sensorSignalstrength.setName(signalstrengthName.c_str());
  sensorSignalstrength.setDeviceClass("signal_strength");
  sensorSignalstrength.setUnitOfMeasurement("dBm");

  sensorLight.setName(lightName.c_str());
  sensorLight.setDeviceClass("illuminance");
  sensorLight.setUnitOfMeasurement("%");

  sensorWind.setName(windName.c_str());
  sensorWind.setUnitOfMeasurement("km/h");

  mqtt.begin(BROKER_ADDR, BROKER_USERNAME, BROKER_PASSWORD);

  while (!mqtt.isConnected())
  {
    mqtt.loop();
    Serial.print(".");
    delay(500); // waiting for the connection
  }

  Serial.println();
  Serial.println("Connected to MQTT broker");

  sensorOwner.setValue(STUDENT_NAME);

  sensorLat.setValue(LAT, (uint8_t)15U);
  sensorLong.setValue(LONG, (uint8_t)15U);
}

void setup()
{
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setRotation(2);
  Serial.begin(9600);
  Serial.println("Starting...");

  // Unique ID must be set!
  byte mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));

  // Connect to wifi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500); // waiting for the connection
  }
  Serial.println();
  Serial.println("Connected to the network");

  setupMqtt();
  dht.begin();
  pinMode(D5, INPUT_PULLUP);
  startWindMonitor();
}

void drawDisplay()
{
  display.clearDisplay();

  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setFont(&FreeSans9pt7b);

  display.setCursor(0, 15);
  display.println("T");
  display.setCursor(0, 37);
  display.println("H");
  display.setCursor(64, 37);
  display.println("L");
  display.setCursor(0, 59);
  display.println("W");

  display.setCursor(20, 15);
  String temperatureStr = String(temperature);
  temperatureStr[strlen(temperatureStr.c_str()) - 1] = '\0';
  display.println(temperatureStr + " C");
  display.drawCircle(58, 2, 2, WHITE);

  display.setCursor(20, 37);
  display.println(String(humidity) + "%");

  display.setCursor(79, 37);
  display.println(String(light) + "%");

  display.setCursor(20, 59);
  display.println(String(wind) + "km/h");
}

bool wifiEnabled = true;

void loop()
{
  signalstrength = WiFi.RSSI();

  float dhtTemperature = dht.readTemperature();
  float dhtHumidity = dht.readHumidity();

  if (isnan(dhtTemperature))
  {
    temperature = -1;
  }
  else
  {
    String string = String(dhtTemperature);
    string[strlen(string.c_str()) - 1] = '\0';
    temperature = dhtTemperature;
  }
  if (isnan(dhtHumidity))
  {
    humidity = -1;
  }
  else
  {
    humidity = (int)dhtHumidity;
  }

  int lightMeasurement = ((float)analogRead(A0)) / 1023.0 * 100.0;
  lightMeasurements[currentLightMeasurementIndex] = lightMeasurement;
  if (currentLightMeasurementIndex < lightMeasurementsAmount)
  {
    currentLightMeasurementIndex++;
  }
  else
  {
    currentLightMeasurementIndex = 0;
  }

  int totalLight = 0;
  for (int i = 0; i < lightMeasurementsAmount; i++)
  {
    totalLight += lightMeasurements[i];
  }
  light = totalLight / lightMeasurementsAmount;

  int totalMs = 0;
  for (int i = 0; i < windMeasurementsAmount; i++)
  {
    totalMs += timeBetweenRotations[i];
  }
  int averageMs = totalMs / windMeasurementsAmount;
  int rpm = 1.0 / (float)averageMs * 1000 * 60;
  wind = 863536.6 + (0.000242878 - 863536.6) / (1.0 + pow(rpm / 76999530.0, 0.8378296));
  if (millis() - lastWindRotation > 2000)
  {
    wind = 0;
  }

  if ((millis() - lastTemperatureSend) > 25000)
  {
    stopWindMonitor();
    WiFi.forceSleepWake();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500); // waiting for the connection
    }
    setLightBrightness(100-light);

    setupMqtt();
    sensorLight.setValue(light);
    sensorTemperature.setValue(temperature);
    sensorHumidity.setValue(humidity);
    sensorSignalstrength.setValue(signalstrength);
    sensorWind.setValue(wind);
    mqtt.loop();

    lastTemperatureSend = millis();
    mqtt.disconnect();
    WiFi.disconnect();
    WiFi.forceSleepBegin();
    startWindMonitor();
  }

  drawDisplay();
  display.display();
  delay(500);
}