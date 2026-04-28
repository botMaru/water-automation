# Water automation
Controls a water pump that fills up a water tank from a well. I used three ESP32 WeMos LOLIN32 Lite boards, which communicate via ESP-NOW.

## The system comprises of 3 stations:
* **Garage control module**
    * TFT LCD display
    * 360 degree encoder
    * Button
    * 2-position switch (AUTO/OFF)
* **Water tank**
    * A02YYUW ultrasonic sensor
* **Well**
    * Two float switches
