#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <algorithm> // For std::sort
#include "common.h"

// UART sensor pins
#define RXD2 16
#define TXD2 17

uint8_t GARAGE_MAC_addr[6] = MAC_GARAGE;

// Structure for sending data
struct_message myData;

// Get stable measurement using a median filter from 5 samples
int get_filtered_distance() {
    int measurements[5];
    int count = 0;
    unsigned long start_attempt = millis();

    // Try to collect 5 valid measurements within 1 second timeout
    while (count < 5 && millis() - start_attempt < 1000) {
        if (Serial2.available() >= 4) {
            if (Serial2.read() == 0xFF) { // Start byte
                uint8_t h = Serial2.read();
                uint8_t l = Serial2.read();
                uint8_t sum = Serial2.read();
                
                // Check checksum
                if (((0xFF + h + l) & 0xFF) == sum) {
                    measurements[count] = (h << 8) + l;
                    count++;
                }
            }
        }
    }

    if (count < 5) return -1; // Sensor timeout or invalid data

    // Sort measurements by value
    std::sort(measurements, measurements + 5);

    // Return the average of the 3 middle values (discard extremes)
    return (measurements[1] + measurements[2] + measurements[3]) / 3;
}

// Callback to check if data was sent successfully to Garage
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Status odeslání: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Úspěch" : "Chyba!");
}

void setup() {
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    
    /*----------- ESP-NOW -----------*/
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Error");
    }
    esp_now_register_send_cb(OnDataSent);

    if (esp_now_peer_setup(GARAGE_MAC_addr) != SYS_ERR_OK) {
        Serial.println("Failed to set up peer device with MAC address:");
        printMacAddress(GARAGE_MAC_addr);
    }
}

void loop() {
    static unsigned long last_send_time = 0;
    const unsigned long send_interval = 5000; // Send interval in milliseconds

    if (millis() - last_send_time >= send_interval) {
        last_send_time = millis();
        
        // Clear old buffered data before measuring
        while(Serial2.available()) { Serial2.read(); }

        int distance = get_filtered_distance();

        if (distance > 0) {
            strcpy(myData.sender, "Tank");
            myData.value = (float)distance; 
            myData.msg_id++;

            esp_err_t result = esp_now_send(GARAGE_MAC_addr, (uint8_t *) &myData, sizeof(myData));
            
            Serial.printf("Vzdálenost: %d mm | Status: %s\n", 
                          distance, (result == ESP_OK ? "Odesláno" : "Chyba"));
        } else {
            Serial.println("Chyba měření: Senzor nedostupný nebo mimo rozsah.");
        }
    }
}