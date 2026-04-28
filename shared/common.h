#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>

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

//function prototypes
int esp_now_peer_setup(const uint8_t MAC_addr[6]);
void printMacAddress(const uint8_t mac[6]);

#endif