
//SPDX-FileCopyrightText: © 2024 Junsu Lee <junsulee119@gmail.com>
//SPDX-License-Identifier: GNU Affero General Public License v3.0
//Version 1.0.0

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <HIDTypes.h>
#include <HIDKeyboardTypes.h>

#define HID_CONSUMER_CONTROL 0x030C

#define BUTTON_PIN 7 // GPIO pin connected to the button

// Global variables for BLE HID
BLEHIDDevice* hid;
BLECharacteristic* input;
BLEServer* server;
BLEAdvertising* advertising;

// HID Report Descriptor for Consumer Control (Volume Up)
uint8_t reportMap[] = {
    0x05, 0x0C,       // Usage Page (Consumer Page)
    0x09, 0x01,       // Usage (Consumer Controls)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x01,       // Report ID (1)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x01,       // Logical Maximum (1)
    0x75, 0x01,       // Report Size (1)
    0x95, 0x02,       // Report Count (2) - Need 2 bits for Up and Down
    0x09, 0xE9,       // Usage (Volume Up)
    0x09, 0xEA,       // Usage (Volume Down)
    0x81, 0x02,       // Input (Data, Variable, Absolute)
    0x95, 0x06,       // Report Count (6) - Padding bits
    0x75, 0x01,       // Report Size (1)
    0x81, 0x03,       // Input (Constant)
    0xC0              // End Collection
};

// Button flag for button shifting (VolUp <-> VolDown)
int btnFlag = 1; // 1 -> VolUp, -1 -> VolDown

// Callback class for connection events
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        Serial.println("Device connected");
    }

    void onDisconnect(BLEServer* pServer) {
        Serial.println("Device disconnected");
        advertising->start(); // Restart advertising
    }
};

// Functions to send a Consumer Control key press
void sendVolumeUp() {
    Serial.println("Sending Volume Up...");
    uint8_t report[] = {0b00000001}; // Bit 0 for Volume Up
    
    // Check if input characteristic exists and a device is connected
    if (input && server->getConnectedCount() > 0) {
        input->setValue(report, sizeof(report));
        input->notify();
        Serial.println("Key press sent!");
    } else {
        Serial.println("Cannot send key press - no connection");
    }
}

void sendVolumeDown() {
    Serial.println("Sending Volume Down...");
    uint8_t report[] = {0b00000010}; // Bit 1 for Volume Down
    
    // Check if input characteristic exists and a device is connected
    if (input && server->getConnectedCount() > 0) {
        input->setValue(report, sizeof(report));
        input->notify();
        Serial.println("Key press sent!");
    } else {
        Serial.println("Cannot send key press - no connection");
    }
}

// Function to send key release event
void sendKeyRelease() {
    Serial.println("Sending key release...");
    
    // Empty key report indicates no keys are pressed
    uint8_t emptyReport[] = {0b00000000};
    
    // Check if input characteristic exists and a device is connected
    if (input && server->getConnectedCount() > 0) {
        input->setValue(emptyReport, sizeof(emptyReport));  // Set empty report
        input->notify();  // Send notification
        Serial.println("Key release sent!");
    } else {
        Serial.println("Cannot send key release - no connection");
    }
}

// Initial setup function - runs once when device starts
void setup() {
    // Initialize serial communication for debugging
    Serial.begin(115200);  // Set high baud rate for fast communication
    while (!Serial) { delay(10); } // Wait for Serial to be ready
    
    Serial.println("==== ESP32 BLE Volume Control ====");

// ButtonPress setup
    // Configure the button pin as input with an internal pull-down resistor
    pinMode(BUTTON_PIN, INPUT_PULLDOWN);

// BLE setup
    // Initialize BLE
    BLEDevice::init("OpenCameraTrigger");  // Set device name
    server = BLEDevice::createServer();  // Create BLE server and set connection callbacks
    server->setCallbacks(new MyServerCallbacks());

    // Security Settings
    BLESecurity* security = new BLESecurity();
    security->setAuthenticationMode(ESP_LE_AUTH_BOND);
    security->setCapability(ESP_IO_CAP_NONE);
    security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    // Initialize BLE HID
    hid = new BLEHIDDevice(server);
    input = hid->inputReport(1); // Report ID 1
    input->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
    hid->reportMap((uint8_t*)reportMap, sizeof(reportMap));
    hid->manufacturer()->setValue("ESP32 BLE Volume Control");
    hid->pnp(0x02, 0x1234, 0x5678, 0x0100);
    hid->hidInfo(0x00, 0x01);

    // Start HID services
    hid->startServices();

    // Configure advertising
    advertising = BLEDevice::getAdvertising();
    advertising->setAppearance(HID_CONSUMER_CONTROL);
    advertising->addServiceUUID(hid->hidService()->getUUID());
    advertising->start();

    Serial.println("==== BLE Volume Control Ready ====");
}

void loop() {
    // Read the state of the button
    int buttonState = digitalRead(BUTTON_PIN);

    if (server->getConnectedCount() > 0) {
        //Serial.println("Connected - Redy to send keys...");
       
        // Check if the button is pressed (HIGH because of the pull-down resistor)
        if (buttonState == HIGH) {
          Serial.println("Button press detected, sending signal...");
          if (btnFlag > 0) { // If btnFlag == 1
            sendVolumeUp(); // Send VolUp
            delay(100); // Short delay to simulate key press
            sendKeyRelease();
            Serial.println("Volume Up sent!");
            btnFlag *= -1; // Shift btnFlag to -1
          } 
          else if (btnFlag < 0) { // If btnFlag == -1
            sendVolumeDown(); // Send VolDown
            delay(100); // Short delay to simulate key press
            sendKeyRelease();
            Serial.println("Volume Down sent!");
            btnFlag *= -1; // Shift btnFlag to 1
          }
          delay(300); // Short delay to prevent fast shifting
        }
    } else {
        // If no device is connected, wait and continue checking
        Serial.println("Waiting for connection...");
        delay(1000);      // No device connected, wait and retry
    }
}
