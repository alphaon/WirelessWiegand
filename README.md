# WirelessWiegand

-Two ESP32 boards: Master and Slave
-Boards to be configured to use Wi-fi AP through Web Interface
-Wi-Fi Confirguration could be changed any time
-Boards use ESP-NOW for communication
-Slave reads data from HID reader and sends to Master over Wi-Fi
-Master gets data packet and send received data to Third Party Device (TPD) over wire (D1,D0)
-Master gets response from TPD over wire (Beep, LED)
-Master sends response over Wi-Fi to slave
-Slave gets response and activate Beeper or/and LED
-Master's LED and Beeper contacts could be used for Access Grant/Denied
-API for POST request could be provided when Wi-Fi to be configured.
