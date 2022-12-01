#include <esp_now.h>
#include <WiFi.h>
#include <UMS3.h>

UMS3 ums3;

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
    boolean motion;
    String node_name;
    int node_type; //0 = camera trigger, 1 = wakeup motion node, 2 = trigger motion node
} struct_message;
// Create a struct_message called BME280Readings to hold sensor readings
struct_message node_message;

RTC_DATA_ATTR int powerMode = 0;
RTC_DATA_ATTR int configMode = 0;
 int bootCount = 0;
void IRAM_ATTR powerDownInt() {
  powerMode = 0;
}
void IRAM_ATTR configInt() {
  configMode++;
}

#define CHANNEL 1
#define NUMNODES 1
RTC_DATA_ATTR esp_now_peer_info_t stations[NUMNODES] = {};
RTC_DATA_ATTR int StationCnt = 0;

boolean connected = false;
boolean awaitReply = false;
// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    // Retry InitESPNow, add a counte and then restart?
    // InitESPNow();
    // or Simply Restart
    ESP.restart();
  }
}

// config AP SSID
void configDeviceAP() {
  String Prefix = "Nodes:";
  String Mac = WiFi.macAddress();
  String SSID = Prefix + Mac;
  String Password = "123456789";
  bool result = WiFi.softAP(SSID.c_str(), Password.c_str(), CHANNEL, 0);
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
  }
}

void powerDown(){
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  detachInterrupt(GPIO_NUM_1);
  detachInterrupt(GPIO_NUM_2);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_1,1); //1 = High, 0 = Low
  
  delay(100);
  Serial.print("going to sleep");
  esp_deep_sleep_start();
  Serial.print("shouldn't see this");
}

void wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  if(wakeup_reason == ESP_SLEEP_WAKEUP_EXT0){
    powerMode=1;
  }
}

// rework this to log the station information when it gets pushed to the node
void logStation(const uint8_t *mac_addr) {
  
  //stations[StationCnt].peer_addr = mac_addr;
  //memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  for (int ii = 0; ii < 6; ++ii ) {
    Serial.println((uint8_t) mac_addr[ii]);
    stations[0].peer_addr[ii] = (uint8_t) mac_addr[ii];
  }	
  stations[0].channel = CHANNEL; // pick a channel
  stations[0].encrypt = 0; // no encryption
  stations[0].ifidx = WIFI_IF_AP;
  connected = true;
  // Nodes not paired, attempt pair
  esp_err_t addStatus = esp_now_add_peer(&stations[0]);
  if (addStatus == ESP_OK) {
    // Pair success
    Serial.println("Pair success");
  } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW Not Init");
  } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Add Peer - Invalid Argument");
  } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
    Serial.println("Peer list full");
  } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("Out of memory");
  } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
    Serial.println("Peer Exists");
  } else {
    Serial.println("Not sure what happened");
        }
  StationCnt++;
      
    
}
boolean sendTrigger = false;

void IRAM_ATTR sendTriggerEvt() {
 sendTrigger=true;
}


void setup() {
  Serial.begin(115200);
  // while (!Serial) {
  //   ; // wait for serial port to connect. Needed for native USB
  // }
  //TODO:: on boot there needs to be a mode check. If not in config mode but powered on this should just instantly boot esp-now and broadcast if second trigger and wait to be polled if first trigger
  Serial.println("ESPNow/Basic Node Example");
  //Set device in AP mode to begin with
  WiFi.mode(WIFI_AP);
  // configure device AP mode
  configDeviceAP();

  Serial.println(String(WiFi.macAddress()));
  // This is the mac address of the Node in AP Mode
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  // Init ESPNow with a fallback logic
  InitESPNow();

    attachInterrupt(digitalPinToInterrupt(GPIO_NUM_2), sendTriggerEvt, RISING);
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
}
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: "); Serial.println(macStr);
  Serial.print("Last Packet Send Status: "); Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if(status == ESP_NOW_SEND_SUCCESS){
    awaitReply = false;
  }
}
// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  
  //struct_message* test = (struct_message*) data;   
  Serial.print("Last Packet Recv from: "); Serial.println(macStr);
  //Serial.print("test->mac_address: "); Serial.println(test->mac_addr);
  logStation(mac_addr);
  

  Serial.println("");
}

void sendTriggerEvent(){
//station_message.mac_addr = "F4:12:FA:4F:A5:B8";
  const uint8_t *station_addr = stations[0].peer_addr;
  node_message.node_name = "Node 1";
  node_message.motion = true;
  node_message.node_type = 2;
  Serial.print("send trigger");
   for (int ii = 0; ii < 6; ++ii ) {
      Serial.print((uint8_t) stations[0].peer_addr[ii], HEX);
      if (ii != 5) Serial.print(":");
    }
  //Serial.println(station_message.mac_addr);
  esp_err_t result = esp_now_send(station_addr, (uint8_t *) &node_message, sizeof(node_message));
  Serial.print("Send Status: ");
    if (result == ESP_OK) {
      Serial.println("Success");
    } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
      // How did we get so far!!
      Serial.println("ESPNOW not Init.");
    } else if (result == ESP_ERR_ESPNOW_ARG) {
      Serial.println("Invalid Argument");
    } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
      Serial.println("Internal Error");
    } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
      Serial.println("ESP_ERR_ESPNOW_NO_MEM");
    } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
      Serial.println("Peer not found.");
    } else if(result == ESP_ERR_ESPNOW_IF){
      Serial.println("ESP_ERR_ESPNOW_IF");
    } else {
      Serial.println("Not sure what happened");
      Serial.println(result);
    }
}

void loop() {
  // Chill

  if(connected && sendTrigger){
    Serial.print("send trigger event");
    sendTriggerEvent();
    sendTrigger = false;
    //delay(1000);
  }

}
