# WirelessWiegand

- :white_check_mark: Two ESP32 boards: Master and Slave
- :white_check_mark: Boards to be configured to use Wi-fi AP through Web Interface
- :white_check_mark: Boards addresses to be configured through Web Interface
- :white_check_mark: Wi-Fi Confirguration could be changed any time by pressing a button
- :ballot_box_with_check: Boards use ESP-NOW for communication
- :ballot_box_with_check: Slave reads data from Wiegand reader and sends to Master over Wi-Fi
- :ballot_box_with_check: Master gets data packet and send received data to Third Party Device (TPD) over wire (D1,D0) (Wiegand 26/40)
- :ballot_box_with_check: Master gets response from TPD over wire (Beep, LED)
- :ballot_box_with_check: Master sends response over Wi-Fi to slave
- :ballot_box_with_check: Slave gets response and activate Beeper or/and LED
- :ballot_box_with_check: Master's LED and Beeper contacts could be used for Access Grant/Denied
- &#9744; API for POST request could be provided when Wi-Fi to be configured.
- :white_check_mark: Activation time of GRANT and DENY activity could be configured in Web
- :ballot_box_with_check: OTA firmware update
</br>
:white_check_mark: - Works,  :ballot_box_with_check: - Needs test, &#9744; - Not implemented yet
