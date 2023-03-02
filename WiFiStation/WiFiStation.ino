#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <WiFi.h>
#include <esp_now.h>

#ifdef ESP32
#include <SPIFFS.h>
#endif

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

//Captive Portal Routine begin


//define your default values here, if there are different values in config.json, they are overwritten.
//char mqtt_server[40];
char reader[8] = "FFFF";
char api_token[34] = "YOUR_API_TOKEN";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//Captive portal routine end

//weigand routine start


uint64_t incomingVAL = 8589934592LL;  //bits correction for card code with 64 bits inside. New Decoded card code.;
//weigand routine end

//ESP-NOW START

// REPLACE WITH THE MAC Address of your receiver
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Define variables to store incoming readings
float incomingGRANT;
float incomingDENY;

// Variable to store if sending data was successful
String success;

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
  float GRANT;
  float DENY;
} struct_message;

typedef struct test_struct {
  uint64_t x;
} test_struct;
test_struct test;

// Create a struct_message to hold incoming sensor readings
test_struct incomingReadings;

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
  incomingVAL = incomingReadings.x;
  //wiegandOut.send(incomingVAL, 34, true); // Send 34 bits with facility code
  print_uint64_t (incomingVAL);
  //delay(1000);
}
//ESP-NOW END




void setup() {


  // put your setup code here, to run once:
  Serial.begin(115200);

  // CAPTIVE

  Serial.println();
  bool res;

  WiFiManager wm;
  char buf1[15];
  String BoardId = WiFi.macAddress();
  Serial.println(BoardId);
  BoardId = BoardId.substring(12);
  BoardId.remove(2, 1);

  //res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap


  BoardId = "Reader_" + BoardId;

  BoardId.toCharArray(buf1, BoardId.length() + 1);
  Serial.println(buf1);
  //res = wm.autoConnect(buf1 , "password"); // password protected ap
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
          //strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(reader, json["reader"]);
          strcpy(api_token, json["api_token"]);
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
  //WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_reader("reader", "readerID", reader, 4);
  WiFiManagerParameter custom_api_token("apikey", "API token", api_token, 32);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10, 0, 1, 99), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));

  //add all your parameters here
  //wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_reader);
  wifiManager.addParameter(&custom_api_token);

  //reset settings - for testing
  wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  wifiManager.setMinimumSignalQuality(50);

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

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
  //  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(reader, custom_reader.getValue());
  strcpy(api_token, custom_api_token.getValue());
  Serial.println("The values in the file are: ");
  //Serial.println("\tmqtt_server : " + String(mqtt_server));
  Serial.println("\treader : " + String(reader));
  Serial.println("\tapi_token : " + String(api_token));

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
    DynamicJsonDocument json(1024);
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
#endif
    //json["mqtt_server"] = mqtt_server;
    json["reader"] = reader;
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


  //CAPTIVE END

  //ESP-NOW START
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
  //ESP-NOW END
}

void loop() {
  // put your main code here, to run repeatedly:

  //ESP-NOW SEND STRING
  //esp_err_t result = esp_now_send (0, (uint8_t *) &struct_message, sizeof(struct_message));

  //  if (result == ESP_OK) {
  //  Serial.println("Sent with success");
  //}
  //else {
  //Serial.println("Error sending the data");
  //}
  //ESP-NOW End


}
//weigand






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

    // Now lets play with some LED's for fun:
    //digitalWrite(LED_RED, LOW); // Red
    // if(val == 12345){
    // If this one "bad" card, turn off green
    // so it's just red. Otherwise you get orange-ish
    //digitalWrite(LED_GREEN, HIGH);
  }
}
