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

float current_water_level = 50; // Variable to store the current water level

//function prototypes
void draw_water_level(float level);

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    struct_message packet;
    memcpy(&packet, incomingData, sizeof(packet));

    Serial.print("Data prijata od: ");
    Serial.println(packet.sender);
    

    tft.fillRect(0, 40, 128, 20, ST77XX_BLACK); //Clear old text
    tft.setCursor(0, 40);
    tft.print("Od: "); tft.println(packet.sender);
    tft.print("Hodnota: "); tft.println(packet.value);
}

void setup() {
    Serial.begin(115200);

    /*--------- TFT display ---------*/
    Serial.println("Initializing TFT display...");

    tft.initR(INITR_BLACKTAB); //Initialize display
    tft.setRotation(0); // 0 = Portrait

    //Turn on the backlight
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, LOW); 

    //Clear the display
    tft.fillScreen(ST77XX_BLACK);
    tft.println("Hello World!");

    /*----------- ESP-NOW -----------*/
    Serial.println("Initializing ESP-NOW...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Error");
    }
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

    uint8_t TANK_MAC_addr[6] = MAC_TANK;
    if (esp_now_peer_setup(TANK_MAC_addr) != SYS_ERR_OK) {
        Serial.println("Failed to set up peer device with MAC address:");
        printMacAddress(TANK_MAC_addr);
    }

    uint8_t WELL_MAC_addr[6] = MAC_WELL;
    if (esp_now_peer_setup(WELL_MAC_addr) != SYS_ERR_OK) {
        Serial.println("Failed to set up peer device with MAC address:");
        printMacAddress(WELL_MAC_addr);
    }

}

void loop() {
    
    delay(1000); // Update every second
}

void draw_dashboard_static(){
    int container_size = 100; //size of the part that gets filled (no borders)
    int border_thickness = 2;
    tft.fillRect(0, 0, 128, 128, ST77XX_BLACK);
    tft.fillRect((128/2)-(container_size/2)-border_thickness, (128/2)-(container_size/2)-border_thickness, 
        container_size + (border_thickness * 2), container_size + (border_thickness * 2), ST7735_WHITE); // Draw the container

}

void draw_dashboard_dynamic(){
    draw_water_level(current_water_level);
}

void draw_water_level(float level) {
    // Clear the area where the water level is displayed
    int container_size = 100; //size of the part that gets filled (no borders)
    int border_thickness = 2;
    int filledHeight = (int)(level * 0.01 * container_size); // Calculate the filled pixels
    
    tft.fillRect((128/2)-(container_size/2), (128/2)+(container_size/2)-filledHeight, 
        container_size, filledHeight, ST7735_BLUE); // Fill the container based on the water level
}