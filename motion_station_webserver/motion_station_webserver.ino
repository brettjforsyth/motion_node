#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "SPIFFS.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#define LED_BUILTIN 2   // Set the GPIO pin where you connected your test LED or comment this line out if your dev board has a built-in LED

// Set these to your desired credentials.
const char *ssid = "StationNode";
const char *password = "1234";
#define CHANNEL 1
/* Wifi Config */
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncWebSocketClient* wsClient;

boolean clientConnected = false;
WiFiClient client;
#define FORMAT_SPIFFS_IF_FAILED true

struct Config {
  uint8_t pollingTimer;
  uint8_t firstNodeAwakeTime;
  uint8_t stationNodeAwakeTime;
};
const char *filename = "/config.json";  // <- SD library uses 8.3 filenames
Config config;  

uint8_t pollingTimer;
uint8_t firstNodeAwakeTime;
uint8_t stationNodeAwakeTime;

// Replaces placeholder with stored values
String processor(const String& var){
  //Serial.println(var);
  if(var == "pollingTimer"){
    return (String)pollingTimer;
  };
  if(var == "firstNodeAwakeTime"){
    return (String)firstNodeAwakeTime;
  };
  if(var == "stationNodeAwakeTime"){
    return (String)stationNodeAwakeTime;
  };
  return String();
}

void onWSEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    //client connected
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    //client disconnected
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    //error was received from the other end
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    //pong message was received (in response to a ping request maybe)
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    //data packet
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
      if(info->opcode == WS_TEXT){
        data[len] = 0;
        Serial.printf("%s\n", (char*)data);
      } else {
        for(size_t i=0; i < info->len; i++){
          Serial.printf("%02x ", data[i]);
        }
        Serial.printf("\n");
      }
      if(info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
      if(info->message_opcode == WS_TEXT){
        data[len] = 0;
        Serial.printf("%s\n", (char*)data);
      } else {
        for(size_t i=0; i < len; i++){
          Serial.printf("%02x ", data[i]);
        }
        Serial.printf("\n");
      }

      if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}
void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}
void setup() {

  Serial.begin(115200);
  while(!Serial){
    ;    
  }
  Serial.println();
  Serial.println("Configuring access point...");
   if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
        Serial.println("SPIFFS Mount Failed");
        return;
    }
  //listDir(SPIFFS, "/", 0);
  loadConfiguration(filename, config);
  //delay(1000);
  startWebInterface();
}

void loop() {
  
}

// Saves the configuration to a file
void saveConfiguration(const char *filename, const Config &config) {
  // Delete existing file, otherwise the configuration is appended to the file
  SPIFFS.remove(filename);
  // Open file for writing
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<256> doc;

  // Set the values in the document
  doc["pollingTimer"] = config.pollingTimer;
  doc["firstNodeAwakeTime"] = config.firstNodeAwakeTime;
  doc["stationNodeAwakeTime"] = config.stationNodeAwakeTime;
  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();
}

// Loads the configuration from a file
void loadConfiguration(const char *filename, Config &config) {

  Serial.println("config loaded");
  Serial.println(filename);
  // Open file for reading
  File file = SPIFFS.open(filename);


char json[] = "{\"pollingTimer\":\"5\",\"firstNodeAwakeTime\":\"5\",\"stationNodeAwakeTime\":\"5\"}";

  if (!file)
  {
    Serial.println("There was an error opening the file");
    return;
  }else{
    //readFile(SPIFFS,filename);
    // // Allocate a temporary JsonDocument
    // // Don't forget to change the capacity to match your requirements.
    // // Use arduinojson.org/v6/assistant to compute the capacity.
    StaticJsonDocument<512> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error){
      Serial.println(F("Failed to read file, using default configuration"));
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }else{
      // Copy values from the JsonDocument to the Config
      config.pollingTimer = pollingTimer = doc["pollingTimer"];
      config.firstNodeAwakeTime = firstNodeAwakeTime = doc["firstNodeAwakeTime"];
      config.stationNodeAwakeTime = stationNodeAwakeTime = doc["stationNodeAwakeTime"];
      file.close();
    }
  }
  
}
// config AP SSID
void configDeviceAP() {
  String Prefix = "MotionStation";
  String SSID = Prefix;
  String Password = "123456789";
  bool result = WiFi.softAP(SSID.c_str(), Password.c_str(), CHANNEL, 0);
  IPAddress Ip(192, 168, 1, 1);
  IPAddress NMask(255, 255, 255, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);
  
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
  }
}
void startWebInterface(){

  WiFi.mode(WIFI_AP);
  configDeviceAP();

  
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
    
  });
  server.on("/bulma.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/bulma.min.css", "text/css");
  });
  server.on("/jquery-3.6.2.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    
    request->send(SPIFFS, "/jquery-3.6.2.min.js", "application/javascript");
  });

   server.on("/config_update", HTTP_GET, [](AsyncWebServerRequest *request){
    int params = request->params();
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){ //p->isPost() is also true
        Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
      if(request->hasParam("pollingTimer")){
        AsyncWebParameter* p = request->getParam("pollingTimer");
        pollingTimer = p->value().toInt();
        config.pollingTimer= pollingTimer;
      }
      if(request->hasParam("firstNodeAwakeTime")){
        AsyncWebParameter* p = request->getParam("firstNodeAwakeTime");
        firstNodeAwakeTime = p->value().toInt();
        config.firstNodeAwakeTime= firstNodeAwakeTime;
      }
      if(request->hasParam("stationNodeAwakeTime")){
        AsyncWebParameter* p = request->getParam("stationNodeAwakeTime");
        stationNodeAwakeTime = p->value().toInt();
        config.stationNodeAwakeTime= stationNodeAwakeTime;
      }
    saveConfiguration(filename,config);
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  server.on("/scan", HTTP_POST,[](AsyncWebServerRequest *request) {},[](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,size_t len, bool final) {
    //handleDoUpdate(request, filename, index, data, len, final);
    }
  );
  Serial.println("Server started");
  ws.onEvent(onWSEvent);
  server.addHandler(&ws);
  server.begin();
}

// void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
//   if (!index){
//     Serial.println("Update");
//     content_len = request->contentLength();
//     // if filename includes spiffs, update the spiffs partition
//     int cmd = (filename.indexOf("spiffs") > -1) ? U_PART : U_FLASH;
// #ifdef ESP8266
//     Update.runAsync(true);
//     if (!Update.begin(content_len, cmd)) {
// #else
//     if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
// #endif
//       Update.printError(Serial);
//     }
//   }

//   if (Update.write(data, len) != len) {
//     Update.printError(Serial);
// #ifdef ESP8266
//   } else {
//     Serial.printf("Progress: %d%%\n", (Update.progress()*100)/Update.size());
// #endif
//   }

//   if (final) {
//     AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
//     response->addHeader("Refresh", "20");  
//     response->addHeader("Location", "/");
//     request->send(response);
//     if (!Update.end(true)){
//       Update.printError(Serial);
//     } else {
//       Serial.println("Update complete");
//       Serial.flush();
//       ESP.restart();
//     }
//   }
// }
void stopWebInterface(){
  // server.stop();
  // server.close();
  // WiFi.disconnect(true);
  // delay(1);
  // WiFi.mode(WIFI_OFF);
  // delay(1);
  // btStop();
  // delay(1);

  // adc_power_off();
  // delay(1);
  // esp_wifi_stop();
  // delay(1);
  // esp_bt_controller_disable();
  // delay(1);
  // //delay(500);
  // systemSleep();
}

