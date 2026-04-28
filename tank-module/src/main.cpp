#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "common.h"


uint8_t GARAGE_MAC_addr[6] = MAC_GARAGE;

//structure for sending data
struct_message myData;

// Callback to check if data was sent successfully to Garage
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Status odeslání: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Úspěch" : "Chyba!");
}

void setup() {
    Serial.begin(115200);
    
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
    strcpy(myData.sender, "Tank");
    myData.value = 125.5;          
    myData.msg_id++;             

    esp_err_t result = esp_now_send(GARAGE_MAC_addr, (uint8_t *) &myData, sizeof(myData));
    
    if (result == ESP_OK) {
        Serial.println("Message sent to Garage");
    } else {
        Serial.println("Error sending message");
    }

    delay(5000);
}