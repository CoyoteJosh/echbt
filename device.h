// The Echelon Services
static NimBLEUUID    connectUUID("0bf669f1-45f2-11e7-9598-0800200c9a66");
static NimBLEUUID      writeUUID("0bf669f2-45f2-11e7-9598-0800200c9a66");
static NimBLEUUID     sensorUUID("0bf669f4-45f2-11e7-9598-0800200c9a66");

static NimBLEAdvertisedDevice * devices[20];
static int device_count = 0;

// button handling
#define BUTTON_DEBOUNCE_TIME 50
#define BUTTON_SHORT_PRESS_TIME 400

// Add a device to our device list (deduplicate by address)
void addDevice(NimBLEAdvertisedDevice * device) {
  if(device_count >= 20) return;
  for (uint8_t i = 0; i < device_count; i++) {
    if(device->getAddress().equals(devices[i]->getAddress())){
      return;
    }
  }
  devices[device_count] = device;
  device_count++;
}

// Select a device and return it
NimBLEAdvertisedDevice * selectDevice(void) {
  Heltec.display->setFont(ArialMT_Plain_10);

  if(device_count == 0) return nullptr;
  if(device_count == 1) return devices[0];

  int selected = 0;

  // Select a device
  while(true) {
    if(selected > device_count-1) selected = 0;
    int start = selected < 2 ? 0 : selected - 1;

    Heltec.display->clear();
    Heltec.display->setLogBuffer(10, 50);
    // Print to the screen
    for (int i = start; i < device_count; i++) {
      if(i == selected) {
        Heltec.display->print("->");
      } else {
        Heltec.display->print("   ");
      }
      if(devices[i]->getName().size() > 0) {
        Heltec.display->println(devices[i]->getName().c_str());
      } else {
        Heltec.display->println(devices[i]->getAddress().toString().c_str());
      }
    }
    Heltec.display->drawLogBuffer(0, 0);
    Heltec.display->display();

    int lastState = LOW;  // the previous state from the input pin
    int currentState;     // the current reading from the input pin
    unsigned long pressedTime  = 0;

    while(true) {
       // read the state of the switch/button:
       currentState = digitalRead(KEY_BUILTIN);
       if(lastState == HIGH && currentState == LOW) {        // button is pressed
         pressedTime = millis();
         Serial.println("DOWN");
       } else if(pressedTime != 0 && lastState == LOW && currentState == HIGH) { // button is released
         long pressDuration = millis() - pressedTime;
         Serial.println("UP");
         if(pressDuration < BUTTON_DEBOUNCE_TIME) {
            pressedTime = 0;
         } else if(pressDuration < BUTTON_SHORT_PRESS_TIME ) {
            Serial.println("A short press is detected");
            pressedTime = 0;
            selected++;
            break;
         } else {
            Serial.println("A long press is detected");
            pressedTime = 0;
            return devices[selected];
            break;
         }
       }


       // save the the last state
       lastState = currentState;    
    }
  }
  return nullptr;
}  
