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

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(D7, DHT11);

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


unsigned long lastTemperatureSend = millis();
float temperature;
int humidity;
int light;
boolean hall;
float signalstrength;

void setup() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  Serial.begin(9600);
  Serial.println("Starting...");

  // Unique ID must be set!
  byte mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));

  // Connect to wifi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500); // waiting for the connection
  }
  Serial.println();
  Serial.println("Connected to the network");

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

  mqtt.begin(BROKER_ADDR, BROKER_USERNAME, BROKER_PASSWORD);

  while (!mqtt.isConnected()) {
        mqtt.loop();
        Serial.print(".");
        delay(500); // waiting for the connection
  }

  Serial.println();
  Serial.println("Connected to MQTT broker");

  sensorOwner.setValue(STUDENT_NAME);

  sensorLat.setValue(LAT, (uint8_t)15U);
  sensorLong.setValue(LONG, (uint8_t)15U);

  dht.begin();
}

void drawDisplay() {
  display.clearDisplay();

  display.setTextSize(1);             // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setFont(&FreeSans9pt7b);

  display.setCursor(0,15);
  display.println("T");
  display.setCursor(0,37);
  display.println("H");
  display.setCursor(0,59);
  display.println("L");

  display.setCursor(25,15);
  String temperatureStr = String(temperature);
  temperatureStr[strlen(temperatureStr.c_str())-1] = '\0';
  display.println(temperatureStr + " C");
  display.drawCircle(63, 2, 2, WHITE);

  display.setCursor(25,37);
  display.println(String(humidity) + "%");

  display.setCursor(25,59);
  display.println(String(light) + "%");
}


void loop() {
  mqtt.loop();
  signalstrength = WiFi.RSSI();


  float dhtTemperature = dht.readTemperature();
  float dhtHumidity = dht.readHumidity();
  
  if (isnan(dhtTemperature)) {
    temperature = -1;
  }
  else {
    String string = String(dhtTemperature);
    string[strlen(string.c_str())-1] = '\0';
    temperature = dhtTemperature;
  }
  if (isnan(dhtHumidity)) {
    humidity = -1;
  }
  else {
    humidity = (int) dhtHumidity;
  }

  light = (int)((float) analogRead(A0)/1023.0*100.0);

  hall = !digitalRead(D3);

  if ((millis() - lastTemperatureSend) > 10000) {
    sensorTemperature.setValue(temperature);
    sensorHumidity.setValue(humidity);
    sensorSignalstrength.setValue(signalstrength);

    lastTemperatureSend = millis();
  }

  drawDisplay();
  display.invertDisplay(hall);
  display.display();
  delay(500);

}