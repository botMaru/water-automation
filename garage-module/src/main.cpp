#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>    
#include <Adafruit_ST7735.h> 
#include <SPI.h>
#include "common.h"


// TFT display pins
#define TFT_CS     5
#define TFT_RST    4 
#define TFT_DC     2
#define TFT_LED    19 

// Display definition
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup() {
    Serial.begin(115200);
    Serial.println("Initializing TFT display...");

    // Display initialization
    tft.initR(INITR_BLACKTAB); 
    tft.setRotation(0); // 0 = Portrait
    tft.fillScreen(ST77XX_BLACK);

    // Turn on the TFT backlight
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, LOW); 

}

void loop() {
    // Tady bude později menu a ovládání čerpadla
}