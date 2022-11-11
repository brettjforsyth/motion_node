#include <esp_now.h>
#include <WiFi.h>
#include <UMS3.h>

// Variable to store if sending data was successful
String success;
const int motionSensor = 5;
boolean motion = false;

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

// Create a struct_message to hold incoming sensor readings
struct_message incoming_node_message;

esp_now_peer_info_t peerInfo;


// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status ==0){
    success = "Delivery Success :)";
  }
  else{
    success = "Delivery Fail :(";
  }
}
// Thais:why in the OnDataSent is *mac_addr and OnDataRecv is *mac? // is incomingData the right name?
//I didn't understand where the incomingData came from and the len
// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&incoming_node_message, incomingData, sizeof(incoming_node_message));
  Serial.print("Bytes received: ");
  Serial.println(len);
  // incomingTemp = incomingReadings.temp;
  // incomingHum = incomingReadings.hum;
  // incomingPres = incomingReadings.pres;
}

uint8_t broadcastAddress[] = {0xF4, 0x12, 0xFA, 0x4E, 0xED, 0x84};
//Thais: question don't we have to declare uint8_t before the constant 
unsigned long broadcast_time;

void IRAM_ATTR detectsMovement() {
  motion = true;
}


void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }
  // PIR Motion Sensor mode INPUT_PULLUP
  pinMode(motionSensor, INPUT_PULLUP);
  // Set motionSensor pin as interrupt, assign interrupt function and set RISING mode
  attachInterrupt(digitalPinToInterrupt(motionSensor), detectsMovement, RISING);  

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  Serial.println(WiFi.macAddress());

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }else{
    Serial.println("peer add success");
  }
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
  node_message.motion = true;
  node_message.node_name = "wakeup";
  node_message.node_type = 1;
  broadcast_time = millis();

  ums3.begin();

  // Brightness is 0-255. We set it to 1/3 brightness here
  ums3.setPixelBrightness(255 / 3);
  ums3.setPixelPower(true);
  ums3.setPixelColor(0,255,0);
}



void loop() {
  if(motion){
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &node_message, sizeof(node_message));
    
    if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
    motion =false
  }
    
}
