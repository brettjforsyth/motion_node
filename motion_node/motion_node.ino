

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
    String mac_addr;
} struct_message;
// Create a struct_message called BME280Readings to hold sensor readings
struct_message node_message;

RTC_DATA_ATTR int powerMode = 0;
RTC_DATA_ATTR int configMode = 0;
RTC_DATA_ATTR int bootCount = 0;

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
  StationCnt = 1;
  
  //stations[StationCnt].peer_addr = mac_addr;
  //memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  
  stations[StationCnt].channel = CHANNEL; // pick a channel
  stations[StationCnt].encrypt = 0; // no encryption
  StationCnt++;
      
    
}

void setup() {
  Serial.begin(115200);
 
  //TODO:: on boot there needs to be a mode check. If not in config mode but powered on this should just instantly boot esp-now and broadcast if second trigger and wait to be polled if first trigger
  Serial.println("ESPNow/Basic Node Example");
  //Set device in AP mode to begin with
  WiFi.mode(WIFI_AP);
  // configure device AP mode
  configDeviceAP();
  // This is the mac address of the Node in AP Mode
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
  esp_now_register_recv_cb(OnDataRecv);
}

// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Recv from: "); Serial.println(macStr);
  Serial.print("Last Packet Recv Data: "); Serial.println(*data);
  Serial.println("");
}

void loop() {
  // Chill
}
