/**
   ESPNOW - Basic communication - Node
   Date: 26th September 2017
   Author: Arvind Ravulavaru <https://github.com/arvindr21>
   Purpose: ESPNow Communication between a Master ESP32 and multiple ESP32 Nodes
   Description: This sketch consists of the code for the Node module.
   Resources: (A bit outdated)
   a. https://espressif.com/sites/default/files/documentation/esp-now_user_guide_en.pdf
   b. http://www.esploradores.com/practica-6-conexion-esp-now/

   << This Device Node >>

   Flow: Master
   Step 1 : ESPNow Init on Master and set it in STA mode
   Step 2 : Start scanning for Node ESP32 (we have added a prefix of  Node` to the SSID of Node for an easy setup)
   Step 3 : Once found, add Node as peer
   Step 4 : Register for send callback
   Step 5 : Start Transmitting data from Master to Node(s)

   Flow: Node
   Step 1 : ESPNow Init on Node
   Step 2 : Update the SSID of Node with a prefix of  Node`
   Step 3 : Set Node in AP mode
   Step 4 : Register for receive callback and wait for data
   Step 5 : Once data arrives, print it in the serial monitor

   Note: Master and Node have been defined to easily understand the setup.
         Based on the ESPNOW API, there is no concept of Master and Node.
         Any devices can act as master or salve.
*/

#include <esp_now.h>
#include <WiFi.h>

#define CHANNEL 1

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

void setup() {
  Serial.begin(115200);
 
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
