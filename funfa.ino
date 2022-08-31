#include <UMS3.h>

UMS3 ums3;

#define timeSeconds 10 //the time to wait after motion isn’t detected to say motion has stopped


// Set GPIOs for LED and PIR Motion Sensor

const int led = 34;
const int motionSensor = 5;
const int greenOffTime = 5; //number of seconds to wait after motion stops to turn the green led off


// Timer: Auxiliary variables
unsigned long now = millis();
unsigned long lastTrigger = 0;
int startTimer = false;

boolean greenStartCountDown = false;
unsigned long greenStartTime = 0;


// Checks if motion was detected, sets LED HIGH and starts a timer
void IRAM_ATTR detectsMovement() {
  startTimer = true;
  lastTrigger = millis();
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  // PIR Motion Sensor mode INPUT_PULLUP
  pinMode(motionSensor, INPUT_PULLUP);
  // Set motionSensor pin as interrupt, assign interrupt function and set RISING mode
  attachInterrupt(digitalPinToInterrupt(motionSensor), detectsMovement, RISING);

  // Set LED to LOW
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  ums3.begin();
}



void loop() {
  // Current time
  now = millis();

  if (startTimer == true) {
    Serial.println("Motion detected...");
    Serial.println(digitalRead(motionSensor));
    digitalWrite(led, HIGH);


  }

  // Turn off the LED after the number of seconds defined in the timeSeconds variable
  if (startTimer == true && now - lastTrigger > (timeSeconds * 1000)) {
    digitalWrite(led, LOW);
    Serial.println("Motion stopped...");

  }

  //if statement true  would continue while led would be high, if it's high would break
  do {
    if (startTimer == true && now - lastTrigger > timeSeconds * 1000) {

      ums3.setPixelPower(true);
      ums3.setPixelColor(0, 255, 255);
      Serial.println("I'm trying");

    }
    now = lastTrigger;
    if (startTimer == true && now - lastTrigger > timeSeconds * 1000) {
      ums3.setPixelPower(false);
      startTimer = false;
      Serial.println("fuck");
    }
  }
  while (motionSensor == LOW && led == LOW);

}