#include <Arduino.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include <DHT.h>
#include <DHT_U.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(D7, DHT11);


void drawDisplay(String temperature, String humidity, String light) {
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
  display.println(temperature + " C");
  display.drawCircle(63, 2, 2, WHITE);
  display.setCursor(25,37);
  display.println(humidity + "%");
  display.setCursor(25,59);
  display.println(light + "%");
}

void setup() {
  Serial.begin(9600);
  dht.begin();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
}


void loop() {
  String temperature;
  String humidity;
  String light;

  float dhtTemperature = dht.readTemperature();
  float dhtHumidity = dht.readHumidity();
  
  if (isnan(dhtTemperature)) {
    temperature = "E";
  }
  else {
    String string = String(dhtTemperature);
    string[strlen(string.c_str())-1] = '\0';
    temperature = string;
  }
  if (isnan(dhtHumidity)) {
    humidity = "E";
  }
  else {
    humidity = String((int) dhtHumidity);;
  }

  light = String((int)((float) analogRead(A0)/1023.0*100.0));

  boolean hall = !digitalRead(D3);

  drawDisplay(temperature, humidity, light);
  display.invertDisplay(hall);
  display.display();
  delay(500);

}