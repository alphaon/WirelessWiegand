#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <esp_now.h>
#include <WiFi.h>
#ifdef ESP32
#include <SPIFFS.h>
#endif

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

//weigand reader
#define MAX_BITS 100                 // max number of bits 
#define WEIGAND_WAIT_TIME  3000      // time to wait for another weigand pulse.  

unsigned char databits[MAX_BITS];    // stores all of the data bits
unsigned char bitCount;              // number of bits currently captured
unsigned char flagDone;              // goes low when data is currently being captured
unsigned int weigand_counter;        // countdown until we assume there are no more bits

void ISR_INT0() {
  // Serial.print("0");   // uncomment this line to display raw binary
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME;

}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1() {
  //Serial.print("1");   // uncomment this line to display raw binary
  databits[bitCount] = 1;
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME;
}
//wegand reader
//define your default values here, if there are different values in config.json, they are overwritten.

char station_id[6] = "8524";
char api_token[34] = "YOUR_API_TOKEN";
char grant_time[2] = "1";
char deny_time[2] = "5";

//flag for saving data
bool shouldSaveConfig = false;

//Wiegand Bits to send
uint64_t UID = 8589934592LL;

// Define variables to store incoming readings
int incomingGRANT;
int incomingDENY;

// Variable to store if sending data was successful
String success;

//Settings Reset Trigger
int Settings;

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
  int GRANT;
  int DENY;
} struct_message;

typedef struct test_struct {
  uint64_t x = 8589934592LL;;
} test_struct;
test_struct test;

// Create a struct_message to hold incoming sensor readings
struct_message incomingReadings;

esp_now_peer_info_t peerInfo;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status == 0) {
    success = "Delivery Success :)";
  }
  else {
    success = "Delivery Fail :(";
  }
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  Serial.print("Bytes received: ");
  Serial.println(len);
  incomingGRANT = incomingReadings.GRANT;
  incomingDENY = incomingReadings.DENY;
  
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  pinMode(34, INPUT);     // DATA0 (INT0)
  pinMode(35, INPUT);     // DATA1 (INT1)
  pinMode(39,INPUT); // Settings Reset
//LEDS
  pinMode(18, OUTPUT); //GREEN
  pinMode(17, OUTPUT); //RED
  pinMode(16, OUTPUT); // YELLOW
  pinMode(21, OUTPUT); // BEEP
  digitalWrite(18, HIGH); // turn the LED on
  digitalWrite(17, HIGH); // turn the LED on
  digitalWrite(16, HIGH); // turn the LED on
  UID = 0;
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if ( ! deserializeError ) {
#else
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
#endif
          Serial.println("\nparsed json");

          strcpy(station_id, json["station_id"]);
          strcpy(api_token, json["api_token"]);
          strcpy(api_token, json["grant_time"]);
          strcpy(api_token, json["deny_time"]);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length

  WiFiManagerParameter custom_station_id("station_id", "Station ID", station_id, 6);
  WiFiManagerParameter custom_api_token("apikey", "API token", api_token, 32);
  WiFiManagerParameter custom_grant_time("grant_time", "Access Granted (sec)", grant_time, 2);
  WiFiManagerParameter custom_deny_time("deny_time", "Access Denied (sec)", deny_time, 2);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  wifiManager.setSTAStaticIPConfig(IPAddress(10, 0, 1, 99), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));

  //add all your parameters here
  wifiManager.addParameter(&custom_station_id);
  wifiManager.addParameter(&custom_api_token);
  wifiManager.addParameter(&custom_grant_time);
  wifiManager.addParameter(&custom_deny_time);

  //reset settings - for testing
  Settings=digitalRead(39);
  if (Settings==HIGH)
  {
  wifiManager.resetSettings();
  }

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality(50);

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);
  char buf1[15];
  String BoardId = WiFi.macAddress();
  Serial.println(BoardId);
  BoardId = BoardId.substring(12);
  BoardId.remove(2, 1);
  BoardId = "Reader ID" + BoardId;

  BoardId.toCharArray(buf1, BoardId.length() + 1);
  Serial.println(buf1);
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(buf1, "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(station_id, custom_station_id.getValue());
  strcpy(api_token, custom_api_token.getValue());
  strcpy(grant_time, custom_grant_time.getValue());
  strcpy(deny_time, custom_deny_time.getValue());
  Serial.println("The values in the file are: ");
  Serial.println("\tstation_id : " + String(station_id));
  Serial.println("\tapi_token : " + String(api_token));
  Serial.println("\tgrant_time : " + String(grant_time));
  Serial.println("\tdeny_time : " + String(deny_time));

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
    DynamicJsonDocument json(1024);
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
#endif
    json["station_id"] = station_id;
    json["api_token"] = api_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
    serializeJson(json, Serial);
    serializeJson(json, configFile);
#else
    json.printTo(Serial);
    json.printTo(configFile);
#endif
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  digitalWrite(18, LOW); // turn the LED off


  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  //Broadcast address
  String id(station_id);
  String onepair = id.substring(2);
  id.remove(2, 2);
  char oo[3];
  char bb[3];
  onepair.toCharArray(oo, onepair.length() + 1);
  id.toCharArray(bb, id.length() + 1);
  uint8_t opair = StrToHex(oo);
  uint8_t spair = StrToHex (bb);
  uint8_t broadcastAddress[] = {0xE8, 0x68, 0xE7, 0x2E, spair, opair};
  Serial.println("Pair MAC 1 DEC: ");
  Serial.println(spair);
  Serial.println("Pair MAC 2 DEC: ");
  Serial.println(opair);

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  digitalWrite(17, LOW); // turn the LED off
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);

  // binds the ISR functions to the falling edge of INTO and INT1
  attachInterrupt(0, ISR_INT0, FALLING);
  attachInterrupt(1, ISR_INT1, FALLING);


  weigand_counter = WEIGAND_WAIT_TIME;


}


void loop() {
  digitalWrite(16, LOW); // turn the LED off
if (incomingGRANT==HIGH)
{
  digitalWrite(18, HIGH); // turn the LED on
  digitalWrite(21, HIGH); //turn the BEEPER
  delay(atoi(grant_time)*1000);
   digitalWrite(18, LOW); // turn the LED on
  digitalWrite(21, LOW); //turn the BEEPER 
  incomingGRANT=LOW;
}else if (incomingDENY==HIGH)
{
   digitalWrite(16, HIGH); // turn the LED on
  digitalWrite(21, HIGH); //turn the BEEPER
  delay(atoi(deny_time)*1000);
   digitalWrite(16, LOW); // turn the LED on
  digitalWrite(21, LOW); //turn the BEEPER 
  incomingDENY=LOW; 
}
  // This waits to make sure that there have been no more data pulses before processing data
  if (!flagDone) {
    if (--weigand_counter == 0)
      flagDone = 1;
  }
  // Read Weigand Data

  if (bitCount > 0 && flagDone) {
    unsigned char i;
    if (bitCount > 10) {
      for (i = 0; i < bitCount; i++) {
        UID <<= 1;
        UID |= databits[i];
      }

      print_uint64_t(UID);
    }


    // cleanup and get ready for the next card
    bitCount = 0;
    UID = 0;
    for (i = 0; i < MAX_BITS; i++)
    {
      databits[i] = 0;
    }
  }
}

void print_uint64_t(uint64_t num) {

  char rev[128];
  char *p = rev + 1;

  while (num > 0) {
    *p++ = '0' + ( num % 10);
    num /= 10;
  }
  p--;
  /*Print the number which is now in reverse*/
  while (p > rev) {
    Serial.print(*p--);


  }
}
int StrToHex(char str[])
{
  return (int) strtol(str, 0, 16);
}
