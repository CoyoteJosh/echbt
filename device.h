#include <TFT_eSPI.h>

// The Echelon Services
static NimBLEUUID    connectUUID("0bf669f1-45f2-11e7-9598-0800200c9a66");
static NimBLEUUID      writeUUID("0bf669f2-45f2-11e7-9598-0800200c9a66");
static NimBLEUUID     sensorUUID("0bf669f4-45f2-11e7-9598-0800200c9a66");

static NimBLEAdvertisedDevice * devices[20];
static int device_count = 0;

extern TFT_eSPI tft;

// BOOT button on GPIO0 (active low, same as original Heltec KEY_BUILTIN)
#define SELECT_BUTTON     0
#define BUTTON_DEBOUNCE_MS   50
#define BUTTON_LONG_PRESS_MS 400

// Add a device to our device list (deduplicate by address)
void addDevice(NimBLEAdvertisedDevice * device) {
  if (device_count >= 20) return;
  for (uint8_t i = 0; i < device_count; i++) {
    if (device->getAddress().equals(devices[i]->getAddress())) return;
  }
  devices[device_count] = device;
  device_count++;
}

// Select a device and return it. Short press = cycle, long press = connect.
NimBLEAdvertisedDevice * selectDevice(void) {
  if (device_count == 0) return nullptr;
  if (device_count == 1) return devices[0];

  int selected = 0;

  while (true) {
    // Draw device list
    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Select Device", 160, 10);
    tft.drawFastHLine(0, 38, 320, TFT_CYAN);

    int maxVisible = min(device_count, 4);
    for (int i = 0; i < maxVisible; i++) {
      bool isSelected = (i == selected);
      if (isSelected) tft.fillRect(0, 45 + i * 42, 320, 40, TFT_NAVY);

      tft.setTextFont(2);
      tft.setTextColor(isSelected ? TFT_CYAN : TFT_WHITE, isSelected ? TFT_NAVY : TFT_BLACK);
      tft.setTextDatum(ML_DATUM);

      const char* name = devices[i]->getName().size() > 0
        ? devices[i]->getName().c_str()
        : devices[i]->getAddress().toString().c_str();

      tft.drawString(isSelected ? "> " : "  ", 10, 65 + i * 42);
      tft.drawString(name, 30, 65 + i * 42);
    }

    tft.setTextFont(1);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextDatum(BC_DATUM);
    tft.drawString("Short press: next   Long press: connect", 160, 238);

    // Wait for button input
    int lastState = HIGH;
    int currentState;
    unsigned long pressedTime = 0;

    while (true) {
      currentState = digitalRead(SELECT_BUTTON);

      if (lastState == HIGH && currentState == LOW) {
        pressedTime = millis();
      } else if (pressedTime != 0 && lastState == LOW && currentState == HIGH) {
        long duration = millis() - pressedTime;
        pressedTime = 0;
        if (duration >= BUTTON_DEBOUNCE_MS && duration < BUTTON_LONG_PRESS_MS) {
          selected = (selected + 1) % device_count;
          break;  // redraw
        } else if (duration >= BUTTON_LONG_PRESS_MS) {
          return devices[selected];
        }
      }

      lastState = currentState;
      delay(10);
    }
  }
  return nullptr;
}
