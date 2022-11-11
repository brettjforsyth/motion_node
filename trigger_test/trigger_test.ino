#define TRIGGER_PIN     4    // Analog output pin A1
#define FOCUS_PIN           3    // Analog input pin A2



void setup()
{
  pinMode(TRIGGER_PIN, OUTPUT);    
  digitalWrite(TRIGGER_PIN, LOW);  
  pinMode(FOCUS_PIN, OUTPUT);    
  digitalWrite(FOCUS_PIN, LOW);  
 
  Serial.begin(115200);  // uncomment for debugging
  delay(500);


}

void loop() {


    Serial.print("trigger on");
    digitalWrite(TRIGGER_PIN, HIGH);  
    delay(1000);      // wait 100ms
    Serial.print("trigger off");
    digitalWrite(TRIGGER_PIN, LOW);  
    delay(2000);      // wait 100ms

}