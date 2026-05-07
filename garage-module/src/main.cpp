#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>    
#include <Adafruit_ST7735.h> 
#include <Fonts/FreeSansBold9pt7b.h>
#include <SPI.h>
#include <AiEsp32RotaryEncoder.h>

#include "common.h"


// TFT display pins
#define TFT_CS     5
#define TFT_RST    4 
#define TFT_DC     22
#define TFT_LED    19 

#define ROTARY_ENCODER_A_PIN 25
#define ROTARY_ENCODER_B_PIN 33
#define ROTARY_ENCODER_BUTTON_PIN 32
#define ROTARY_ENCODER_VCC_PIN -1 // Pokud máš napájeno z 3.3V pinu, dej -1
#define ROTARY_ENCODER_STEPS 4    // Většinou 4 kroky na jedno cvaknutí

#define WATER_COLOR    0x6c1f
#define MAX_WATER_LEVEL 100.0

// Display definition
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(
    ROTARY_ENCODER_A_PIN, 
    ROTARY_ENCODER_B_PIN, 
    ROTARY_ENCODER_BUTTON_PIN, 
    ROTARY_ENCODER_VCC_PIN, 
    ROTARY_ENCODER_STEPS
);

char scene[2][10] = {"Dashboard", "Settings"};
int current_scene = 0; // Variable to track the current scene (dashboard, settings, etc.)
int last_scene = -1; // Variable to track the last scene for detecting changes

float current_water_level = 100; // Variable to store the current water level
float last_water_level = -1; // Variable to store the last water level for change detection
float contact_timeout = 10000; // Timeout in milliseconds to consider a device as offline
float last_contact_tank = -(contact_timeout+1); // Variable to store the last contact time with the tank
float last_contact_well = -(contact_timeout+1); // Variable to store the last contact time with the well

float last_interaction_time = 0; // Variable to store the last interaction time for display sleep mode
float display_sleep_timeout = 60000; // Timeout in milliseconds to turn off the display after inactivity

bool top_well_sensor = false;
bool bottom_well_sensor = false;

//function prototypes
void draw_water_level();
void draw_dashboard_static();
void draw_dashboard_dynamic();
void draw_centered_text(int x_center, int y, const char* text);
void draw_well();

// Function to handle rotary encoder input
void rotary_loop() {
    int32_t delta = rotaryEncoder.encoderChanged();

    if (delta != 0) {
        current_water_level += delta;

        // Kontrola limitů
        if (current_water_level > MAX_WATER_LEVEL) current_water_level = MAX_WATER_LEVEL;
        if (current_water_level < 0) current_water_level = 0;

        Serial.printf("Zmena: %d | Hladina: %.1f%%\n", delta, current_water_level);
    }
    
    if (rotaryEncoder.isEncoderButtonClicked()) {
        Serial.println("Tlacitko!");
        last_interaction_time = millis(); // Update the last interaction time
    }
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    struct_message packet;
    memcpy(&packet, incomingData, sizeof(packet));

    Serial.print("Data prijata od: ");
    Serial.println(packet.sender);
    Serial.print("Hodnota: ");
    Serial.println(packet.value);
    
    if (strcmp(packet.sender, "Tank") == 0) {
        current_water_level = packet.value;
        last_contact_tank = millis();
    } else if (strcmp(packet.sender, "Well") == 0) {
        top_well_sensor = packet.level_top;
        bottom_well_sensor = packet.level_bot;
        last_contact_well = millis();
    }

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

    rotaryEncoder.begin();
    rotaryEncoder.setup(
        [] { rotaryEncoder.readEncoder_ISR(); }, 
        [] { rotaryEncoder.readEncoder_ISR(); }
    );
    rotaryEncoder.setBoundaries(0, 100, false); 
    rotaryEncoder.setEncoderValue(current_water_level); // Synchronizace!
    rotaryEncoder.setAcceleration(250);
    
}

void loop() {
    if (current_scene != last_scene) {
        switch (current_scene) {
            case 0: // Dashboard
                draw_dashboard_static();
                break;
            case 1: // Settings
                //TODO: Implement settings screen
                break;
        }
        last_scene = current_scene;
    }
    switch (current_scene) {
        case 0: // Dashboard
            draw_dashboard_dynamic();
            break;
        case 1: // Settings
            //TODO: Implement settings screen
            break;
    }

    rotary_loop(); // Encoder handling

    static bool sleep_state = false;
    if (millis()-last_interaction_time > display_sleep_timeout && !sleep_state) {
        digitalWrite(TFT_LED, HIGH); // Turn off the backlight after inactivity
        sleep_state = true;
    } else if (millis()-last_interaction_time <= display_sleep_timeout && sleep_state) {
        digitalWrite(TFT_LED, LOW); // Turn on the backlight if there was recent interaction
        sleep_state = false;
    }
    delay(100); 
}

void draw_dashboard_static(){
    int container_size = 80; //size of the part that gets filled (no borders)
    int border_thickness = 3;
    tft.fillRect(0, 0, 128, 100, ST77XX_BLACK);
    tft.fillRect(10-border_thickness, 10-border_thickness, container_size+(2*border_thickness), container_size+(2*border_thickness), ST77XX_WHITE); // Draw the border
    tft.fillRect(0, 100, 128, 60, ST77XX_BLACK); // Clear the bottom area for text
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);

    int well_h = 80;
    int well_w = 20;
    tft.fillRect(100-border_thickness, 10-border_thickness, well_w+(2*border_thickness), well_h+(2*border_thickness), ST77XX_WHITE); // Draw the well

    tft.setCursor(5, 100);
    tft.print("Tank: ");
    tft.setCursor(5, 100+10);
    tft.print("Well: ");
}

void draw_dashboard_dynamic(){
    if (current_water_level != last_water_level) {
        draw_water_level();
        last_water_level = current_water_level;
    }

    tft.setTextSize(1);
    tft.setFont();

    static int last_tank_status = -1;
    int current_tank_status = ((millis() - last_contact_tank) < contact_timeout);

    if (current_tank_status != last_tank_status) {
        tft.fillRect(35, 100, 41, 8, ST77XX_BLACK); // Clear the area for text
        tft.setTextColor(current_tank_status ? ST77XX_GREEN : ST77XX_RED);
        tft.setCursor(35, 100);
        tft.print(current_tank_status ? "Online " : "Offline");
        last_tank_status = current_tank_status;
    }

    static int last_well_status = -1;
    int current_well_status = ((millis() - last_contact_well) < contact_timeout);
    if (current_well_status != last_well_status) {
        tft.fillRect(35, 100+10, 41, 8, ST77XX_BLACK); // Clear the area for text
        tft.setTextColor(current_well_status ? ST77XX_GREEN : ST77XX_RED);
        tft.setCursor(35, 100+10);
        tft.print(current_well_status ? "Online " : "Offline");
        last_well_status = current_well_status;
    }

    static int last_top_sensor = -1;
    static int last_bottom_sensor = -1;

    if (top_well_sensor != last_top_sensor || bottom_well_sensor != last_bottom_sensor) {
        draw_well();
        last_top_sensor = top_well_sensor;
        last_bottom_sensor = bottom_well_sensor;
    }

}

void draw_water_level() {
    int container_size = 80; //size of the part that gets filled (no borders)
    int water_level_percentage = (int)(current_water_level / MAX_WATER_LEVEL * 100);
    if (water_level_percentage > 100) water_level_percentage = 100;
    if (water_level_percentage < 0) water_level_percentage = 0;
    int filled_height = (int)(water_level_percentage * 0.01 * container_size); // Calculate the filled pixels
    
    tft.fillRect(10, 10, container_size, (container_size-filled_height), ST7735_WHITE); // Draw the empty part
    tft.fillRect(10, 10+(container_size-filled_height), container_size, filled_height, WATER_COLOR); // Draw the filled part
    tft.setTextColor(ST77XX_BLACK);
    tft.setTextSize(3);
    //tft.setFont(&FreeSansBold9pt7b);

    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%d%%", water_level_percentage);
    draw_centered_text(50, 50, buffer);
    tft.setFont();
}

void draw_well() {
    int well_h = 80;
    int well_w = 20;
    int well_bottom = 90;
    int well_water;
    
    if (top_well_sensor) {
        well_water = 20;
    } else if (bottom_well_sensor) {
        well_bottom = 50;
    } else {
        well_water = 80;
    }

    tft.fillRect(100, well_water, well_w, well_bottom-well_water, WATER_COLOR); // Water level in the well
    tft.fillRect(100, 10, well_w, well_water-10, ST77XX_WHITE); // Clear the well area

    tft.fillRect(100, 30, well_w, 10, top_well_sensor ? ST77XX_GREEN : ST77XX_RED); // Upper sensor
    tft.drawRect(100, 30, well_w, 10, ST77XX_BLACK); // Upper sensor border

    tft.fillRect(100, 60, well_w, 10, bottom_well_sensor ? ST77XX_GREEN : ST77XX_RED); // Lower sensor
    tft.drawRect(100, 60, well_w, 10, ST77XX_BLACK); // Lower sensor border
}

void draw_centered_text(int x_center, int y_center, const char* text) {
    int16_t x1, y1;
    uint16_t w, h;

    // Funkce změří text a uloží šířku do 'w' a výšku do 'h'
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    // Výpočet: začátek = střed - (šířka / 2)
    int x_pos = x_center - (w / 2);
    int y_pos = y_center - (h / 2);

    tft.setCursor(x_pos, y_pos);
    tft.print(text);
}