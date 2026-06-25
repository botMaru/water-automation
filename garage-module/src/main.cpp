#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // Required for Wi-Fi Long Range and TX power configuration
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

// Rotary encoder configuration
#define ROTARY_ENCODER_A_PIN 25
#define ROTARY_ENCODER_B_PIN 33
#define ROTARY_ENCODER_BUTTON_PIN 32
#define ROTARY_ENCODER_VCC_PIN -1 
#define ROTARY_ENCODER_STEPS 4    

// Button pins
#define MENU_BUTTON 35
#define ON_OFF_BUTTON 34

#define WATER_COLOR    0x6c1f

// Display definition
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(
    ROTARY_ENCODER_A_PIN, 
    ROTARY_ENCODER_B_PIN, 
    ROTARY_ENCODER_BUTTON_PIN, 
    ROTARY_ENCODER_VCC_PIN, 
    ROTARY_ENCODER_STEPS
);

volatile int current_scene = 0; // Track the current active scene
volatile int last_scene = -1;    // Track scene changes

// Tank calibration constants (in mm)
const float TANK_EMPTY_MM = 900.0; 
const float TANK_FULL_MM  = 57.0;

float current_water_level = 100; 
int current_water_level_percentage = 0; 
int last_water_level_percentage = -1; 

float contact_timeout = 13000; 
unsigned long last_contact_tank = -(contact_timeout+1); 
unsigned long last_contact_well = -(contact_timeout+1); 

volatile unsigned long last_interaction_time = 0; // For display sleep timeout tracking
float display_sleep_timeout = 60000; 

bool top_well_sensor = false;
bool bottom_well_sensor = false;

// Function prototypes
void draw_water_level();
void draw_dashboard_static();
void draw_dashboard_dynamic(bool force_update=false);
void draw_centered_text(int x_center, int y, const char* text);
void draw_well();
void draw_settings_static();
void draw_settings_dymamic(bool force_update=false);

// Handle rotary encoder inputs
void rotary_loop() {
    int32_t delta = rotaryEncoder.encoderChanged();

    if (delta != 0) {
        last_interaction_time = millis(); 
    }
    
    if (rotaryEncoder.isEncoderButtonClicked()) {
        Serial.println("Button pressed");
        last_interaction_time = millis(); 
    }
}

// ESP-NOW Data receive callback
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    struct_message packet;
    memcpy(&packet, incomingData, sizeof(packet));

    Serial.print("Data prijata od: ");
    Serial.println(packet.sender);
    
    if (strcmp(packet.sender, "Tank") == 0) {
        Serial.print("Hodnota (vzdalenost): ");
        Serial.println(packet.value);

        float distance_mm = packet.value;
        current_water_level = (TANK_EMPTY_MM - distance_mm);
        current_water_level_percentage = (int)((current_water_level / (TANK_EMPTY_MM - TANK_FULL_MM)) * 100);
        last_contact_tank = millis();

    } else if (strcmp(packet.sender, "Well") == 0) {
        Serial.print("Plovaky -> Top (Horni): ");
        Serial.print(packet.level_top);
        Serial.print(" | Bot (Spodni): ");
        Serial.println(packet.level_bot);

        top_well_sensor = packet.level_top;
        bottom_well_sensor = packet.level_bot;
        last_contact_well = millis();
    }
}

// Interrupt handler for menu button
void IRAM_ATTR menu_button_press(){
    unsigned long interrupt_menu = millis();
    if (interrupt_menu - last_interrupt_menu > 200) {
        current_scene = (current_scene + 1) % 2;
        last_interrupt_menu = interrupt_menu; 
    }
    last_interaction_time = millis();
}

// Interrupt handler for industrial ON/OFF toggle switch
volatile bool on_off_state = false;
void IRAM_ATTR on_off_switch_change(){
    static unsigned long last_interrupt_time = 0;
    unsigned long interrupt_time = millis();

    if (interrupt_time - last_interrupt_time > 100) { 
        on_off_state = (digitalRead(ON_OFF_BUTTON) == LOW);
    }
    last_interrupt_time = interrupt_time;
    last_interaction_time = millis();
}

void setup() {
    Serial.begin(115200);

    // Initialize buttons
    pinMode(MENU_BUTTON, INPUT);
    attachInterrupt(MENU_BUTTON, menu_button_press, FALLING);
    pinMode(ON_OFF_BUTTON, INPUT);
    attachInterrupt(ON_OFF_BUTTON, on_off_switch_change, CHANGE);

    // Initialize TFT display
    Serial.println("Initializing TFT display...");
    tft.initR(INITR_BLACKTAB); 
    tft.setRotation(0); 

    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, LOW); 
    tft.fillScreen(ST77XX_BLACK);

    // Initialize Wi-Fi and ESP-NOW
    Serial.println("Initializing ESP-NOW...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Enable Wi-Fi Long Range mode
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
    
    // Set max TX power to 19.5 dBm
    esp_wifi_set_max_tx_power(78);

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

    // Initialize rotary encoder
    rotaryEncoder.begin();
    rotaryEncoder.setup(
        [] { rotaryEncoder.readEncoder_ISR(); }, 
        [] { rotaryEncoder.readEncoder_ISR(); }
    );
    rotaryEncoder.setBoundaries(0, 100, false); 
    rotaryEncoder.setEncoderValue(current_water_level); 
    rotaryEncoder.setAcceleration(250);
}

void loop() {
    // Handle scene switching
    if (current_scene != last_scene) {
        switch (current_scene) {
            case 0: 
                draw_dashboard_static();
                break;
            case 1: 
                draw_settings_static();
                break;
        }
        last_scene = current_scene;
    }

    // Update dynamic elements
    switch (current_scene) {
        case 0: 
            draw_dashboard_dynamic();
            break;
        case 1: 
            break;
    }

    rotary_loop(); 

    // Handle display sleep state
    static bool sleep_state = false;
    if (millis() - last_interaction_time > display_sleep_timeout && !sleep_state) {
        digitalWrite(TFT_LED, HIGH); // Backlight OFF
        sleep_state = true;
    } else if (millis() - last_interaction_time <= display_sleep_timeout && sleep_state) {
        digitalWrite(TFT_LED, LOW);  // Backlight ON
        sleep_state = false;
    }
    delay(100); 
}

void draw_dashboard_static(){
    int container_size = 80; 
    int border_thickness = 3;
    tft.fillRect(0, 0, 128, 100, ST77XX_BLACK);
    tft.fillRect(10-border_thickness, 10-border_thickness, container_size+(2*border_thickness), container_size+(2*border_thickness), ST77XX_WHITE); 
    tft.fillRect(0, 100, 128, 60, ST77XX_BLACK); 
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);

    int well_h = 80;
    int well_w = 20;
    tft.fillRect(100-border_thickness, 10-border_thickness, well_w+(2*border_thickness), well_h+(2*border_thickness), ST77XX_WHITE); 

    tft.setCursor(5, 100);
    tft.print("Nadrz: ");
    tft.setCursor(5, 100+10);
    tft.print("Studna: ");
    
    draw_dashboard_dynamic(true);
}

void draw_dashboard_dynamic(bool force_update){
    if (current_water_level_percentage != last_water_level_percentage || force_update) {
        draw_water_level();
        last_water_level_percentage = current_water_level_percentage;
    }

    tft.setTextSize(1);
    tft.setFont();

    static int last_tank_status = -1;
    int current_tank_status = ((millis() - last_contact_tank) < contact_timeout);

    if (current_tank_status != last_tank_status || force_update) {
        tft.fillRect(48, 100, 45, 8, ST77XX_BLACK); 
        tft.setTextColor(current_tank_status ? ST77XX_GREEN : ST77XX_RED);
        tft.setCursor(48, 100);
        tft.print(current_tank_status ? "Online " : "Offline");
        last_tank_status = current_tank_status;
    }

    static int last_well_status = -1;
    int current_well_status = ((millis() - last_contact_well) < contact_timeout);
    if (current_well_status != last_well_status || force_update) {
        tft.fillRect(54, 100+10, 45, 8, ST77XX_BLACK); 
        tft.setTextColor(current_well_status ? ST77XX_GREEN : ST77XX_RED);
        tft.setCursor(54, 100+10);
        tft.print(current_well_status ? "Online " : "Offline");
        last_well_status = current_well_status;
    }

    static int last_top_sensor = -1;
    static int last_bottom_sensor = -1;

    if (top_well_sensor != last_top_sensor || bottom_well_sensor != last_bottom_sensor || force_update) {
        draw_well();
        last_top_sensor = top_well_sensor;
        last_bottom_sensor = bottom_well_sensor;
    }
}

void draw_water_level() {
    int container_size = 80; 
    int capped_percentage;
    if (current_water_level_percentage > 100){
        capped_percentage = 100;
    } else if (current_water_level_percentage < 0){
        capped_percentage = 0;
    } else {
        capped_percentage = current_water_level_percentage;
    }
    int filled_height = (int)(capped_percentage * 0.01 * container_size); 

    tft.fillRect(10, 10, container_size, (container_size-filled_height), ST7735_WHITE); 
    tft.fillRect(10, 10+(container_size-filled_height), container_size, filled_height, WATER_COLOR); 
    tft.setTextColor(ST77XX_BLACK);
    tft.setTextSize(3);

    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%d%%", current_water_level_percentage);
    draw_centered_text(50, 50, buffer);
    tft.setFont();
}

void draw_well() {
    int well_h = 80;
    int well_w = 20;
    int well_top = 10;
    int well_bottom = 90;
    int well_water_y;
    
    if (top_well_sensor) {
        well_water_y = 20;
    } else if (bottom_well_sensor) {
        well_water_y = 50;
    } else {
        well_water_y = 80;
    }

    tft.fillRect(100, well_water_y, well_w, well_bottom-well_water_y, WATER_COLOR); 
    tft.fillRect(100, 10, well_w, well_water_y-well_top, ST77XX_WHITE); 

    tft.fillRect(100, 30, well_w, 10, top_well_sensor ? ST77XX_GREEN : ST77XX_RED); 
    tft.drawRect(100, 30, well_w, 10, ST77XX_BLACK); 

    tft.fillRect(100, 60, well_w, 10, bottom_well_sensor ? ST77XX_GREEN : ST77XX_RED); 
    tft.drawRect(100, 60, well_w, 10, ST77XX_BLACK); 
}

void draw_centered_text(int x_center, int y_center, const char* text) {
    int16_t x1, y1;
    uint16_t w, h;

    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    int x_pos = x_center - (w / 2);
    int y_pos = y_center - (h / 2);

    tft.setCursor(x_pos, y_pos);
    tft.print(text);
}

void draw_settings_static(){
    tft.setCursor(0,0);
    tft.fillRect(0,0,128,160, ST77XX_BLACK);
}

void draw_settings_dymamic(bool force_update){
}