#include "Arduino.h"
#include <TFT_eSPI.h>
#include "NimBLEDevice.h"
#include "device.h"
#include "power.h"

TFT_eSPI tft = TFT_eSPI();

// CYD RGB LED (active low)
#define LED_R  4
#define LED_G 16
#define LED_B 17

static boolean connected = false;
static bool screenOn = false;

static NimBLERemoteCharacteristic* writeCharacteristic;
static NimBLERemoteCharacteristic* sensorCharacteristic;
static NimBLEAdvertisedDevice* device;
static NimBLEClient* client;
static NimBLEScan* scanner;

const unsigned long ScreenTimeoutMillis = 120000;

static int cadence = 0;
static int resistance = 0;
static int power = 0;
static unsigned long runtime = 0;
static unsigned long last_millis = 0;
static unsigned long last_cadence = 0;

#define debug 0
#define maxResistance 32

// Called when device sends update notification
static void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* data, size_t length, bool isNotify) {
  // Log non-D1 packets always, D1 only when data changes
  static uint8_t prev_d1[13] = {0};
  if(data[1] != 0xD1) {
    Serial.printf("PKT[%02X] len=%d:", data[1], length);
    for(int i = 0; i < length; i++) Serial.printf(" %02X", data[i]);
    Serial.println();
  } else {
    bool changed = false;
    for(int i = 0; i < length && i < 13; i++) {
      if(data[i] != prev_d1[i]) { changed = true; break; }
    }
    if(changed) {
      Serial.printf("D1 CHANGED:");
      for(int i = 0; i < length; i++) Serial.printf(" %02X", data[i]);
      Serial.println();
      memcpy(prev_d1, data, length < 13 ? length : 13);
    }
  }

  switch(data[1]) {
    // Cadence notification
    case 0xD1:
      cadence = int((data[9] << 8) + data[10]);
      power = getPower(cadence, resistance);
      break;
    // Resistance notification
    case 0xD2:
      resistance = int(data[3]);
      power = getPower(cadence, resistance);
      break;
  }

  if(debug) {
    Serial.print("CALLBACK(");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(":");
    Serial.print(length);
    Serial.print("):");
    for(int x = 0; x < length; x++) {
      if(data[x] < 16) Serial.print("0");
      Serial.print(data[x], HEX);
    }
    Serial.println();
  }
}

// Called on connect or disconnect
class ClientCallback : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pclient) {
    digitalWrite(LED_G, LOW);   // green on
    digitalWrite(LED_R, HIGH);  // red off
    Serial.println("Connected!");
  }
  void onDisconnect(NimBLEClient* pclient) {
    connected = false;
    NimBLEDevice::deleteClient(client);
    client = nullptr;
    digitalWrite(LED_G, HIGH);  // green off
    digitalWrite(LED_R, LOW);   // red on
    Serial.println("Disconnected!");
  }
};

class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    std::string name = advertisedDevice->getName();
    bool isEchelon = advertisedDevice->isAdvertisingService(connectUUID) ||
                     (name.size() >= 3 && name.substr(0, 3) == "ECH");
    if(isEchelon) {
      NimBLEAdvertisedDevice * d = new NimBLEAdvertisedDevice;
      *d = *advertisedDevice;
      addDevice(d);
    }
  }
};

// Draw the static display layout (called once on connect / screen wake)
void drawLayout() {
  tft.fillScreen(TFT_BLACK);

  // Header bar
  tft.fillRect(0, 0, 320, 40, TFT_NAVY);
  tft.setTextFont(2);
  tft.setTextColor(TFT_CYAN, TFT_NAVY);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("ECHBT", 10, 20);
  tft.drawFastHLine(0, 40, 320, TFT_CYAN);

  // Dividers
  tft.drawFastVLine(160, 40, 152, TFT_DARKGREY);
  tft.drawFastHLine(0, 192, 320, TFT_DARKGREY);

  // Metric labels
  tft.setTextFont(2);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("CADENCE", 80, 46);
  tft.drawString("RPM", 80, 158);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("POWER", 240, 46);
  tft.drawString("W", 240, 158);

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("RESISTANCE", 5, 213);
}

void updateDisplay() {
  bool shouldBeOn = (last_millis - last_cadence <= ScreenTimeoutMillis);

  if (!shouldBeOn) {
    if (screenOn) {
      tft.fillScreen(TFT_BLACK);
      screenOn = false;
    }
    return;
  }

  if (!screenOn) {
    drawLayout();
    screenOn = true;
  }

  // Runtime
  const int minutes = int(runtime / 60000);
  const int seconds = int(runtime % 60000) / 1000;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d", minutes, seconds);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(buf, 310, 20);

  // Cadence
  tft.setTextFont(7);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(tft.textWidth("000", 7));
  itoa(cadence, buf, 10);
  tft.drawString(buf, 80, 108);

  // Power
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  itoa(power, buf, 10);
  tft.drawString(buf, 240, 108);
  tft.setTextPadding(0);

  // Resistance progress bar
  int barWidth = (245 * resistance) / maxResistance;
  tft.fillRect(5, 225, 245, 10, TFT_DARKGREY);
  if (barWidth > 0) tft.fillRect(5, 225, barWidth, 10, TFT_GREEN);

  // Peloton-equivalent resistance percentage
  int pelRes = getPeletonResistance(resistance);
  snprintf(buf, sizeof(buf), "%d%%", pelRes);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MR_DATUM);
  tft.setTextPadding(tft.textWidth("100%", 2));
  tft.drawString(buf, 315, 213);
  tft.setTextPadding(0);
}

bool connectToServer() {
  Serial.print("Connecting to ");
  Serial.println(device->getName().c_str());

  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Connecting to:", 160, 100);
  tft.drawString(device->getName().c_str(), 160, 130);

  client = NimBLEDevice::createClient();
  client->setClientCallbacks(new ClientCallback());
  if (!client->connect(device)) {
    NimBLEDevice::deleteClient(client);
    client = nullptr;
    return false;
  }

  NimBLERemoteService* remoteService = client->getService(connectUUID);
  if (remoteService == nullptr) {
    Serial.print("Failed to find service UUID: ");
    Serial.println(connectUUID.toString().c_str());
    client->disconnect();
    return false;
  }
  Serial.println("Found device.");

  sensorCharacteristic = remoteService->getCharacteristic(sensorUUID);
  if (sensorCharacteristic == nullptr) {
    Serial.print("Failed to find sensor characteristic UUID: ");
    Serial.println(sensorUUID.toString().c_str());
    client->disconnect();
    return false;
  }
  sensorCharacteristic->subscribe(true, notifyCallback);
  Serial.println("Enabled sensor notifications.");

  writeCharacteristic = remoteService->getCharacteristic(writeUUID);
  if (writeCharacteristic == nullptr) {
    Serial.print("Failed to find write characteristic UUID: ");
    Serial.println(writeUUID.toString().c_str());
    client->disconnect();
    return false;
  }
  byte message[] = {0xF0, 0xB0, 0x01, 0x01, 0xA2};
  writeCharacteristic->writeValue(message, 5);
  Serial.println("Activated status callbacks.");

  last_cadence = millis();
  screenOn = false;  // force layout redraw on first updateDisplay()

  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.flush();
  delay(50);

  // TFT init — backlight must be on before init
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(1);  // landscape 320x240
  tft.fillScreen(TFT_BLACK);

  // RGB LED (active low on CYD)
  pinMode(LED_R, OUTPUT); digitalWrite(LED_R, HIGH);
  pinMode(LED_G, OUTPUT); digitalWrite(LED_G, HIGH);
  pinMode(LED_B, OUTPUT); digitalWrite(LED_B, HIGH);

  // BOOT button used for device selection
  pinMode(0, INPUT_PULLUP);

  tft.fillScreen(TFT_BLACK);

  NimBLEDevice::init("");
  scanner = NimBLEDevice::getScan();
  scanner->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  scanner->setInterval(100);
  scanner->setWindow(100);
  scanner->setActiveScan(false);
}

void loop() {
  if (!connected) {
    Serial.println("Start Scan!");
    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Scanning for Echelon...", 160, 110);

    device_count = 0;
    scanner->clearResults();
    scanner->setActiveScan(true);
    scanner->setInterval(100);
    scanner->setWindow(100);
    scanner->start(15, false);
    NimBLEDevice::getScan()->stop();

    device = selectDevice();
    if (device != nullptr) {
      connected = connectToServer();
      if (!connected) {
        Serial.println("Failed to connect...");
        tft.fillScreen(TFT_BLACK);
        tft.setTextFont(4);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Connection failed", 160, 100);
        tft.drawString("Retrying...", 160, 135);
        delay(1100);
        return;
      }
    } else {
      Serial.println("No device found...");
      tft.fillScreen(TFT_BLACK);
      tft.setTextFont(4);
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("No Echelon found", 160, 100);
      tft.drawString("Retrying...", 160, 135);
      delay(1100);
      return;
    }
  }

  // Send keepalive to prevent bike from disconnecting
  static unsigned long last_keepalive = 0;
  if (millis() - last_keepalive > 10000) {
    byte message[] = {0xF0, 0xB0, 0x01, 0x01, 0xA2};
    writeCharacteristic->writeValue(message, 5);
    last_keepalive = millis();
  }

  // Update timer if cadence is rolling
  if (cadence > 0) {
    unsigned long now = millis();
    runtime += now - last_millis;
    last_millis = now;
    last_cadence = now;
  } else {
    last_millis = millis();
  }

  delay(500);
  // Temporarily skip display to test if SPI interferes with BLE
  // updateDisplay();
  Serial.printf("cad=%d res=%d pwr=%d\n", cadence, resistance, power);
}
