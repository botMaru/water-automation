#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // Přidáno pro pokročilé nastavení Wi-Fi (Long Range a TX výkon)
#include "common.h"


#define PIN_LEVEL_TOP 25
#define PIN_LEVEL_BOT 26

// Definice obou přijímačů pro redundantní trasu
uint8_t GARAGE_MAC_addr[6] = MAC_GARAGE;
uint8_t TANK_MAC_addr[6] = MAC_TANK; // Přidána MAC adresa Nádrže

//structure for sending data
struct_message myData;

// Proměnné pro sledování předchozího stavu senzorů a hlídání času
bool last_level_top = false;
bool last_level_bot = false;
unsigned long last_heartbeat_time = 0;
const unsigned long heartbeat_interval = 5000; // Interval pro pravidelný heartbeat (5 sekund)

// Upravený callback, který ti v sériovém monitoru ukáže, pro kterou MAC adresu status platí
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

    // Vynucení POUZE Long Range (LR) módu na Wi-Fi rozhraní pro maximální dosah a spolehlivost
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
    
    // Nastavení maximálního vysílacího výkonu antény (78 = 19.5 dBm)
    esp_wifi_set_max_tx_power(78);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Error");
    }
    esp_now_register_send_cb(OnDataSent);

    // Registrace PEERA 1: Garáž (přímá trasa)
    if (esp_now_peer_setup(GARAGE_MAC_addr) != SYS_ERR_OK) {
        Serial.println("Failed to set up peer device with MAC address (Garage):");
        printMacAddress(GARAGE_MAC_addr);
    }

    // Registrace PEERA 2: Nádrž (záložní trasa přes opakovač)
    if (esp_now_peer_setup(TANK_MAC_addr) != SYS_ERR_OK) {
        Serial.println("Failed to set up peer device with MAC address (Tank):");
        printMacAddress(TANK_MAC_addr);
    }

    pinMode(PIN_LEVEL_TOP, INPUT_PULLUP);
    pinMode(PIN_LEVEL_BOT, INPUT_PULLUP);

    // Načtení výchozího stavu při startu, aby se hned po zapnutí neodeslala falešná změna
    last_level_top = digitalRead(PIN_LEVEL_TOP);
    last_level_bot = digitalRead(PIN_LEVEL_BOT);
    
    // Nastavení výchozího času, aby první zpráva odešla ihned po zapnutí
    last_heartbeat_time = millis();
}

void loop() {
    // Přečtení aktuálního fyzického stavu spínačů
    bool current_level_top = digitalRead(PIN_LEVEL_TOP);
    bool current_level_bot = digitalRead(PIN_LEVEL_BOT);
    
    // Vyhodnocení podmínek pro odeslání
    bool state_changed = (current_level_top != last_level_top) || (current_level_bot != last_level_bot);
    bool heartbeat_timeout = (millis() - last_heartbeat_time >= heartbeat_interval);

    // Spuštění vysílání: pokud se buď změnil stav spínačů, nebo uteklo 5 sekund od minulé zprávy
    if (state_changed || heartbeat_timeout) {
        
        // Uložení aktuálních stavů a času pro příští porovnání
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

        // 1. Odeslání přímo do Garáže
        esp_err_t res_garage = esp_now_send(GARAGE_MAC_addr, (uint8_t *) &myData, sizeof(myData));
        if (res_garage != ESP_OK) {
            Serial.println("Chyba inicializace odesílání do Garáže");
        }

        // 2. Odeslání do Nádrže (která zprávu automaticky přepošle dál)
        esp_err_t res_tank = esp_now_send(TANK_MAC_addr, (uint8_t *) &myData, sizeof(myData));
        if (res_tank != ESP_OK) {
            Serial.println("Chyba inicializace odesílání do Nádrže");
        }

        Serial.printf("Sent - Top: %d, Bottom: %d\n", myData.level_top, myData.level_bot);
    }

    // Krátká pauza (10 ms) zabraňuje stoprocentnímu vytížení procesoru naprázdno,
    // ale zároveň zaručuje bleskovou odezvu na reakci plováků (zpoždění max 0.01 sekundy).
    delay(10);
}