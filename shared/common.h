#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
#include <cstring>
#include <esp_now.h>
#include <WiFi.h>

//Error codes for the system
enum SystemError {
    SYS_ERR_OK = 0,
    SYS_ERR_PEER_ADD = 101,
    SYS_ERR_SEND_FAILED = 102,
    SYS_ERR_SENSOR_TIMEOUT = 200,
};

// MAC addresses of devices 
#define MAC_GARAGE {0xB0, 0xCB, 0xD8, 0xEE, 0xBC, 0x8C}
#define MAC_TANK   {0x08, 0xA6, 0xF7, 0x9C, 0x78, 0x38}
#define MAC_WELL   {0x00, 0x4B, 0x12, 0xE5, 0x68, 0x2C}

typedef struct struct_message {
    char sender[12];
    float value;    
    bool status;    
    uint32_t msg_id;
} struct_message;

static inline int esp_now_peer_setup(const uint8_t *MAC_addr) {
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, MAC_addr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        return SYS_ERR_PEER_ADD;
    }
    return SYS_ERR_OK;
}

static inline void printMacAddress(const uint8_t *mac) {
    for (int i = 0; i < 6; i++) {
        if (mac[i] < 0x10) Serial.print("0");
        Serial.print(mac[i], HEX);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
}

#endif