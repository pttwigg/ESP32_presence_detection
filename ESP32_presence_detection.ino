// Peyton Twigg
//
// This program uses the BLE function on the ESP32 in order to detect devices nearby and send a signal to turn a smart device(s) 
// on or off. It will detect any device (unless its address is designated to be ignored) in range rather than looking for one
// specific one as I am using an iPhone, which does not use a permanent address. 
//
// The way the program works is by scanning all devices for scanTime seconds, searching though these devices, and determining if
// each device that is not in the ignore list is in range or not. If any device enters or leaves the range for a sufficient
// number of consecutive scans, it will publish the on or off signal to the MQTT topic, delay for a certain amount of time, and
// then skip the rest of the scan results and start over.
// Uses the NimBLE library as it uses much less memory than the built in BLE library.

#include "NimBLEDevice.h"
#include "NimBLEAdvertisedDevice.h"

#include <WiFi.h>
#include <PubSubClient.h>

#define WIFI_SSID "<SSID>"
#define WIFI_PASSWORD "<Password>"
#define MQTT_HOST IPAddress(X, X, X, X)
#define MQTT_PORT <PORT>

// MQTT Topics
#define MQTT_PUB "<topic>"

#define LED 2   // Pin number that controls the LED

WiFiClient espClient;
PubSubClient MQTTclient(espClient);

// How long to scan for devices in seconds.
// Because of the fact that all devices will be scanned first
// and then parsed after the scan period, this variable should
// be small as to not delay the triggering of the action.
int scanTime = 1;

int RSSIThresh = -80;
int onThresh = 3;         // Number of times to get a positive result to trigger action (avoid false positives)
int offThresh = 10;       // Number of times to get a negative result to trigger action (avoid false negatives)
int onCount = onThresh;   // Actual on count
int offCount = 0;         // Actual off count
int deviceStatus = 0;     // 0 if device should be off, 1 if on
unsigned long interval = 30000;
unsigned long entry;

// Devices that should not trigger the service (like the TV. Nobody carries a TV around.)
// Devices in list: 
String ignoredDevices[] = {"xx:xx:xx:xx:xx:xx"};

BLEScan* pScan;

// Connect to the WiFi and MQTT server, then publish the signal
void MQTTMessage() {
  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  Serial.println("...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  entry = millis();

  // Keep trying to connect to WiFi
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - entry >= 15000) esp_restart();
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
  MQTTclient.setServer(MQTT_HOST, MQTT_PORT);
  Serial.println("Connect to MQTT server...");
  while (!MQTTclient.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    if (MQTTclient.connect("Presence Detector")) {
      Serial.println("connected");
      MQTTclient.publish(MQTT_PUB, String(deviceStatus).c_str());   // Publish on signal
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(MQTTclient.state());
      Serial.println("try again in 1 second");

      // Wait before retrying
      delay(1000);
    }
  }
  for (int i = 0; i > 10; i++) {
    MQTTclient.loop();
    delay(100);
  }
  MQTTclient.disconnect();
  delay(100);
  WiFi.mode(WIFI_OFF);
  btStart();
}

void positive() {
  offCount = offThresh;         // Reset the off counter

  // Range not met enough times
  if (onCount > 0 && !deviceStatus) {
    Serial.println("\nDevice in range, decrementing onCount...");
    --onCount;
  }
  // Range met enough times, send on signal
  else if (!deviceStatus) {
    Serial.println("\nDevice in range, sending positive signal...");
    digitalWrite(LED, HIGH);    // Turn on the LED to indicate device in range
    deviceStatus = 1;

    MQTTMessage();  // Connect to WiFi and MQTT, then publish the on signal
  }
  // Already on, just delay if need to
  else {
    Serial.println("\nDevice already on, no signal to send...");
  }
}

void negative() {
  onCount = onThresh;

  // Not out of range enough times, decrement offCount
  if (offCount > 0 && deviceStatus) {
    Serial.println("\nDevice not in range, decrementing offCount...");
    --offCount;
  }
  // Out of range sufficient times, send off signal if device set to on
  else if (deviceStatus) {
    Serial.println("\nNone in range, sending negative signal...");
    digitalWrite(LED, LOW);   // Turn off the LED if all the scans have been parsed and no device was in range
    deviceStatus = 0;

    MQTTMessage();    // Connect to WiFi and MQTT, then Publish the off signal
  }
  // Out of range and device already off, so do nothing
  else {
    Serial.println("\nNone in range, no signal sent...");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  BLEDevice::init("");   // Initialize the NimBLE thing

  // Create a NimBLE instance and return a pointer to it
  pScan = BLEDevice::getScan();

  // Gives more accurate results
  pScan->setActiveScan(true);

  Serial.print("Scan time:\t");
  Serial.println(scanTime);
  Serial.print("RSSI Thresh:\t");
  Serial.println(RSSIThresh);
}

void loop() {
  Serial.println("\nScanning...");
  Serial.print("Off Count: ");
  Serial.println(offCount);
  Serial.print("On Count: ");
  Serial.println(onCount);
  Serial.println();

  bool inRange = false;

  // Scan for scanTime seconds and store devices in BLEUtilsResults instance named results
  BLEScanResults results = pScan->start(scanTime);

  // Loop through the list of devices to see if any match the conditions
  for (int i = 0; i < results.getCount(); i++) {

    // Ignore device if in ignoredDevices
    bool ignore = false;

    BLEAdvertisedDevice device = results.getDevice(i);

    Serial.print("Device name: ");
    Serial.print(device.getName().c_str());
    Serial.print("   /   MAC: ");
    Serial.print(device.getAddress().toString().c_str());
    Serial.print("   /   RSSI: ");
    Serial.print(device.getRSSI());
    Serial.print("   /   Ignored: ");

    // Loop through the list of devices to ignore and see if they match the currently scanned device
    for (int i = 0; i < (sizeof(ignoredDevices) / sizeof(ignoredDevices[0])); i++) {

      // Convert device address to string and test to see if it is in the ignoredDevices list
      if (strcmp(device.getAddress().toString().c_str(), ignoredDevices[i].c_str()) == 0) {
        ignore = true;  // Ignore device
        break;          // Dont keep looping because there is no point
      }
    }

    Serial.println(ignore);   // Print whether the device was in the ignored list

    // Test to see if the RSSI value is in range
    if ((device.getRSSI() > RSSIThresh) && (!ignore)) {
      positive();

      // Restart the scan because if the delay is set, it can use out of date scans that make it
      // think the device is still in the room when it is not
      inRange = true;
      break;
    }
  }
  // Reached the end with no matches, send negative result
  if (!inRange) {
    negative();
  }
}
