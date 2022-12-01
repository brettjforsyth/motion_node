#include <esp_now.h>
#include <WiFi.h>
#include <UMS3.h>

UMS3 ums3;

RTC_DATA_ATTR int powerMode = 0;
RTC_DATA_ATTR int configMode = 0;
RTC_DATA_ATTR int bootCount = 0;

void IRAM_ATTR powerDownInt() {
  powerMode = 0;
}
void IRAM_ATTR configInt() {
  configMode++;
}

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
    boolean motion;
    String node_name;
    int node_type; //0 = camera trigger, 1 = wakeup motion node, 2 = trigger motion node
    int message_type; //0 = not, 1 = configuration
} struct_message;
// Create a struct_message called BME280Readings to hold sensor readings
struct_message station_message;

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

// Global copy of Nodes
#define NUMNODES 2
RTC_DATA_ATTR esp_now_peer_info_t nodes[NUMNODES] = {};
RTC_DATA_ATTR int nodeTypes[NUMNODES] = {};
RTC_DATA_ATTR int NodeCnt = 0;

#define CHANNEL 1
#define PRINTSCANRESULTS 0

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

// Scan for nodes in AP mode
void ScanForNodes() {
  int8_t scanResults = WiFi.scanNetworks();
  //reset nodes
  //TODO:: Need to stop clearing every scan and actually seeing if the scanned wifi addresses exist or not
  memset(nodes, 0, sizeof(nodes));
  NodeCnt = 0;
  Serial.println("");
  if (scanResults == 0) {
    Serial.println("No WiFi devices in AP Mode found");
  } else {
    Serial.print("Found "); Serial.print(scanResults); Serial.println(" devices ");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);

      if (PRINTSCANRESULTS) {
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
      }
      delay(10);
      // Check if the current device starts with `Nodes`
      if (SSID.indexOf("Nodes") == 0) {
        // SSID of interest
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
        // Get BSSID => Mac Address of the Nodes
        int mac[6];

        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            nodes[NodeCnt].peer_addr[ii] = (uint8_t) mac[ii];
          }
        }
        nodes[NodeCnt].channel = CHANNEL; // pick a channel
        nodes[NodeCnt].encrypt = 0; // no encryption
        NodeCnt++;
      }
    }
  }

  if (NodeCnt > 0) {
    Serial.print(NodeCnt); Serial.println(" Nodes(s) found, processing..");
  } else {
    Serial.println("No Nodes Found, trying again.");
  }

  // clean up ram
  WiFi.scanDelete();
}


// Check if the Nodes is already paired with the master.
// If not, pair the Nodes with master
void manageNodes() {
  if (NodeCnt > 0) {
    for (int i = 0; i < NodeCnt; i++) {
      Serial.print("Processing: ");
      for (int ii = 0; ii < 6; ++ii ) {
        Serial.print((uint8_t) nodes[i].peer_addr[ii], HEX);
        if (ii != 5) Serial.print(":");
      }
      Serial.print(" Status: ");
      // check if the peer exists
      bool exists = esp_now_is_peer_exist(nodes[i].peer_addr);
      if (exists) {
        // Nodes already paired.
        Serial.println("Already Paired");
      } else {
        // Nodes not paired, attempt pair
        esp_err_t addStatus = esp_now_add_peer(&nodes[i]);
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
        delay(100);
      }
    }
  } else {
    // No Nodes found to process
    Serial.println("No Nodes found to process");
  }
}


//String station_mac = "";
// send data
void sendConfigData() {
  //station_mac = WiFi.macAddress();


  for (int i = 0; i < NodeCnt; i++) {
    const uint8_t *peer_addr = nodes[i].peer_addr;
    // if (i == 0) { // print only for first Nodes
    //   Serial.print("Sending: ");
    //   Serial.println(station_mac);
    // }
    //station_message.mac_addr = "F4:12:FA:4F:A5:B8";
    station_message.node_name = "Station 1";
    station_message.node_type = 0;
    station_message.message_type = 1;
    //Serial.print("struct mac: ");
    //Serial.println(station_message.mac_addr);
    esp_err_t result = esp_now_send(peer_addr, (uint8_t *) &station_message, sizeof(station_message));
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
    } else {
      Serial.println("Not sure what happened");
    }
    delay(100);
  }
}

// callback when data is sent from Master to Nodes
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: "); Serial.println(macStr);
  Serial.print("Last Packet Send Status: "); Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  Serial.println("OnDataRecv");
  //ums3.setPixelColor(UMS3::colorWheel(150));
  struct_message* test = (struct_message*) data;   
   Serial.println(test->node_name);
   Serial.println(test->motion);
}

void setup() {
  wakeup_reason();
  if(powerMode == 0){
    powerDown();
  }else{
    //TODO:: setup a timer to watch for a set amout of time to power things down
    attachInterrupt(digitalPinToInterrupt(GPIO_NUM_1), powerDownInt, RISING);
    attachInterrupt(digitalPinToInterrupt(GPIO_NUM_2), configInt, RISING);
  };
  ums3.begin();
  ums3.setPixelBrightness(255 / 3);
  ++bootCount;
  Serial.begin(115200);
  // while (!Serial) {
  //   ; // wait for serial port to connect. Needed for native USB
  // }
  //Set device in STA mode to begin with
  WiFi.mode(WIFI_STA);
  //station_mac = WiFi.macAddress();
  //Serial.println(station_mac[0]);
  Serial.println("ESPNow/Multi-Nodes/Master Example");
  // This is the mac address of the Master in Station Mode
  Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // Serial.print("power mode");
  // Serial.println(powerMode);
  // Serial.print("config mode");
  // Serial.println(configMode);
  // delay(100);
  if(powerMode == 0){
    Serial.println("power down");
    powerDown();
  }
  
  // In the loop we scan for Nodes
  if(configMode % 2 == 1){
    ums3.setPixelPower(true);
    ums3.setPixelColor(UMS3::colorWheel(37));

    //TODO:: Check for the number of found nodes
    ScanForNodes();
    // If Nodes is found, it would be populate in `Nodes` variable
    // We will check if `Nodes` is defined and then we proceed further
    if (NodeCnt > 0) { // check if Nodes channel is defined
      // `Nodes` is defined
      // Add Nodes as peer if it has not been added already
      manageNodes();
      // pair success or already paired
      // Send data to device
      sendConfigData();
    } else {
      // No Nodes found to process
    }
    for(int x = 0; x < NodeCnt; x++){
      ums3.setPixelBrightness(0);
      delay(100);
      ums3.setPixelBrightness(255 / 3);
    }
    // wait for 3seconds to run the logic again
    delay(1000);
  }else{
    ums3.setPixelPower(false);
  }
  // if (NodeCnt > 0) {
  //   Serial.print("node count: ");
  //   Serial.println(NodeCnt);
  //   Serial.print("Nodes: ");
  //   Serial.write((byte*)&nodes, sizeof(nodes));
  //   delay(1000);
  // }
}
 