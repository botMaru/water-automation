#include "common.h"

int esp_now_peer_setup(const uint8_t MAC_addr[6]) {
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

void printMacAddress(const uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        if (mac[i] < 0x10) Serial.print("0"); // Přidá nulu pro hezčí formát (např. 0A místo A)
        Serial.print(mac[i], HEX);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
}