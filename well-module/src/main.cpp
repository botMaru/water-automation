#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // Required for Wi-Fi Long Range and TX power configuration
#include "common.h"

// Float switch pins
#define PIN_LEVEL_TOP 25
#define PIN_LEVEL_BOT 26

// Peer MAC addresses for redundant routing
uint8_t GARAGE_MAC_addr[6] = MAC_GARAGE;
uint8_t TANK_MAC_addr[6] = MAC_TANK; 

// Structure for sending data
struct_message myData;

// State tracking variables
bool last_level_top = false;
bool last_level_bot = false;
unsigned long last_heartbeat_time = 0;
const unsigned long heartbeat_interval = 5000; // Periodic transmission interval in ms

// Callback to check transmission status per MAC address
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Status odeslání pro MAC [");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X%s", mac_addr[i], (i < 5) ? ":" : "");
    }
    Serial.print("]: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Úspěch" : "Chyba!");
}

void setup() {
    Serial.begin(115200);
    
    /*----------- ESP-NOW -----------*/
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Enforce Wi-Fi Long Range (LR) mode for maximum range and reliability
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
    
    // Set max TX power to 19.5 dBm
    esp_wifi_set_max_tx_power(78);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Error");
    }
    esp_now_register_send_cb(OnDataSent);

    // Register Peer 1: Garage (Direct path)
    if (esp_now_peer_setup(GARAGE_MAC_addr) != SYS_ERR_OK) {
        Serial.println("Failed to set up peer device with MAC address (Garage):");
        printMacAddress(GARAGE_MAC_addr);
    }

    // Register Peer 2: Tank (Backup path via repeater)
    if (esp_now_peer_setup(TANK_MAC_addr) != SYS_ERR_OK) {
        Serial.println("Failed to set up peer device with MAC address (Tank):");
        printMacAddress(TANK_MAC_addr);
    }

    pinMode(PIN_LEVEL_TOP, INPUT_PULLUP);
    pinMode(PIN_LEVEL_BOT, INPUT_PULLUP);

    // Read initial states to prevent false change triggers on startup
    last_level_top = digitalRead(PIN_LEVEL_TOP);
    last_level_bot = digitalRead(PIN_LEVEL_BOT);
    
    last_heartbeat_time = millis();
}

void loop() {
    // Read current physical sensor states
    bool current_level_top = digitalRead(PIN_LEVEL_TOP);
    bool current_level_bot = digitalRead(PIN_LEVEL_BOT);
    
    // Evaluate transmission triggers
    bool state_changed = (current_level_top != last_level_top) || (current_level_bot != last_level_bot);
    bool heartbeat_timeout = (millis() - last_heartbeat_time >= heartbeat_interval);

    // Trigger transmission on state change or heartbeat timeout
    if (state_changed || heartbeat_timeout) {
        
        last_level_top = current_level_top;
        last_level_bot = current_level_bot;
        last_heartbeat_time = millis();

        strcpy(myData.sender, "Well");
        myData.level_top = current_level_top;
        myData.level_bot = current_level_bot;
        myData.value = 0;
        myData.msg_id++;             

        if (state_changed) {
            Serial.println("\n--- Změna stavu! Odesílám data na obě trasy ---");
        } else {
            Serial.println("\n--- Heartbeat! Odesílám data na obě trasy ---");
        }

        // 1. Send data directly to Garage
        esp_err_t res_garage = esp_now_send(GARAGE_MAC_addr, (uint8_t *) &myData, sizeof(myData));
        if (res_garage != ESP_OK) {
            Serial.println("Chyba inicializace odesílání do Garáže");
        }

        // 2. Send data to Tank (acting as a repeater)
        esp_err_t res_tank = esp_now_send(TANK_MAC_addr, (uint8_t *) &myData, sizeof(myData));
        if (res_tank != ESP_OK) {
            Serial.println("Chyba inicializace odesílání do Nádrže");
        }

        Serial.printf("Sent - Top: %d, Bottom: %d\n", myData.level_top, myData.level_bot);
    }

    delay(10);
}