#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // Přidáno pro pokročilé nastavení Wi-Fi (Long Range a TX výkon)
#include <algorithm> // Pro funkci std::sort
#include "common.h"

// Definice pinů pro UART senzor
#define RXD2 16
#define TXD2 17

uint8_t GARAGE_MAC_addr[6] = MAC_GARAGE;

//structure for sending data
struct_message myData;

// Funkce pro získání jedné stabilní hodnoty (Median Filter ze 5 vzorků)
int get_filtered_distance() {
    int measurements[5];
    int count = 0;
    unsigned long start_attempt = millis();

    // Pokusíme se nasbírat 5 platných měření během max 1 sekundy
    while (count < 5 && millis() - start_attempt < 1000) {
        if (Serial2.available() >= 4) {
            if (Serial2.read() == 0xFF) {
                uint8_t h = Serial2.read();
                uint8_t l = Serial2.read();
                uint8_t sum = Serial2.read();
                
                if (((0xFF + h + l) & 0xFF) == sum) {
                    measurements[count] = (h << 8) + l;
                    count++;
                }
            }
        }
    }

    if (count < 5) return -1; // Pokud senzor neodpovídá nebo jsou data špatná

    // Seřadíme naměřené hodnoty podle velikosti
    std::sort(measurements, measurements + 5);

    // Vrátíme průměr ze 3 prostředních hodnot (odstřihneme extrémy)
    return (measurements[1] + measurements[2] + measurements[3]) / 3;
}

// Callback to check if data was sent successfully to Garage
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Status odeslání z Nádrže: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Úspěch" : "Chyba!");
}

// NOVÉ: Callback pro příjem dat (Repeater funkce)
// Jakmile Nádrž zachytí zprávu ze Studny, okamžitě ji přepošle do Garáže
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    Serial.println("Repeater: Přijata data ze Studny, přeposílám do Garáže...");
    
    // Přeposlání surového balíčku dat přímo na MAC adresu Garáže
    esp_err_t result = esp_now_send(GARAGE_MAC_addr, incomingData, len);
    
    if (result == ESP_OK) {
        Serial.println("Repeater: Data úspěšně přeposlána.");
    } else {
        Serial.printf("Repeater: Chyba při přeposílání: %d\n", result);
    }
}

void setup() {
    Serial.begin(115200);

    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // [cite: 50, 174]
    
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
    
    // NOVÉ: Registrace přijímacího callbacku pro funkci repeateru
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

    if (esp_now_peer_setup(GARAGE_MAC_addr) != SYS_ERR_OK) {
        Serial.println("Failed to set up peer device with MAC address:");
        printMacAddress(GARAGE_MAC_addr);
    }
}

void loop() {
    static unsigned long last_send_time = 0;
    const unsigned long send_interval = 5000; // Odesílání každých 5 sekund

    if (millis() - last_send_time >= send_interval) {
        last_send_time = millis();
        
        while(Serial2.available()) { Serial2.read(); }

        int distance = get_filtered_distance();

        if (distance > 0) {
            strcpy(myData.sender, "Tank");
            myData.value = (float)distance; // Posíláme milimetry jako float
            myData.msg_id++;

            esp_err_t result = esp_now_send(GARAGE_MAC_addr, (uint8_t *) &myData, sizeof(myData));
            
            Serial.printf("Vzdálenost: %d mm | Status: %s\n", 
                          distance, (result == ESP_OK ? "Odesláno" : "Chyba"));
        } else {
            Serial.println("Chyba měření: Senzor nedostupný nebo mimo rozsah.");
        }
    }
}