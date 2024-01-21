// Includes
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_ADS1X15.h>
#include <Chrono.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

// Constants
const int kLedPin = 2;
const int kAnalogPin = A0;
const int kAdcPin = 1;
const int kDefaultAvgDistance = 5;
const float kShuntAmp = 20.0f; // 75 mV = 20 Amps
const float kShuntDropMv = 0.075f;  // 75 millivolts
const float kBatteryCapacityAh = 9.0f;

// Global Variables
WiFiClientSecure client;
HTTPClient http;
Adafruit_ADS1115 ads;
Chrono myChrono;
float batteryCapacityAh = kBatteryCapacityAh;
float remainingCapacityAh = kBatteryCapacityAh;
String batteryName = "Unknown";
String state = "Unknown";
String ipAddress;
String macAddress;
int rollingAvgDistance = kDefaultAvgDistance;
float shuntAmp = kShuntAmp; // 75 mV = 20 Amps
float shuntDropMv = kShuntDropMv;  // 75 millivolts
float shuntOhms = shuntDropMv / shuntAmp;
float currentAverage = 0.0;
float voltageAverage = 0.0;

// Function Prototypes
void connectToWiFi(const char* ssid, const char* password);
void getBatteryConfig(const String& macAddress);
void initializeADS1115();
float takeMeasurement(int adcPin);
void writeToDB(float voltage, float amperage, float remainingAh, const String& remainingTime, const String& state, const String& macAddress, const String& ipAddress, float remainingPercent);
float calculateRollingAverage(float currentAverage, float newSample, int sampleCount);
String formatTime(long seconds);

void setup() {
    // Serial output
    Serial.begin(115200);
    // Connect to WiFi (SSID and Password in arduino_secrets.h)
    connectToWiFi(SECRET_SSID, SECRET_PASS);
    // using the mac_address, pull the configurations for this deployment from MongoDB
    getBatteryConfig(macAddress);
    // Set up ADC (ADS1115)
    pinMode(kLedPin, OUTPUT);
    initializeADS1115();

    Serial.println("Starting...");
    // Setup complete!
}

int startup_loop = 0;

void loop() {
    digitalWrite(kLedPin, HIGH);

    float measurementInterval = myChrono.elapsed();
    float voltage = takeMeasurement(kAdcPin);
    myChrono.restart();

    voltageAverage = calculateRollingAverage(voltageAverage, voltage, rollingAvgDistance);

    if (startup_loop < rollingAvgDistance) {
      startup_loop++;
      Serial.print(".");
      return;
    } else if (startup_loop == rollingAvgDistance) {
      startup_loop++;
      Serial.println("Starting...");
    }

    // Calculate the current in amperes by dividing the average voltage by the shunt resistance
    float current = voltageAverage / shuntOhms;
    // Convert the measurement interval from milliseconds to hours for capacity calculation
    float timeHours = measurementInterval / 3600000.0;
    // Calculate the amount of capacity (in ampere-hours) used in this measurement interval
    float usedCapacity = current * timeHours;
    // Subtract the used capacity from the remaining battery capacity to update its value
    remainingCapacityAh -= usedCapacity;
    // Constrain the remaining capacity to be within the range of 0 to the maximum battery capacity
    remainingCapacityAh = constrain(remainingCapacityAh, 0.0, batteryCapacityAh);
    // Calculate the remaining battery capacity as a percentage of the total capacity
    float remainingBatteryPercent = (remainingCapacityAh * 100) / batteryCapacityAh;
    // Calculate remaining battery life in hours
    float remainingBatteryLifeHours = (current != 0) ? (remainingCapacityAh / current) : 0;
    // Convert remaining battery life from hours to seconds
    long remainingBatteryLifeSeconds = static_cast<long>(remainingBatteryLifeHours * 3600);
    // Format the remaining battery life in HH:MM:SS format
    String formattedRemainingBatteryLife = formatTime(remainingBatteryLifeSeconds);

    Serial.println("-----------|-------");
    Serial.print("V:          "); Serial.println(String(voltage, 7));
    Serial.print("Avg V:      "); Serial.println(String(voltageAverage, 7));
    Serial.print("I:          "); Serial.println(String(current, 7));
    Serial.print("Remain Ah:  "); Serial.println(String(remainingCapacityAh, 7));
    Serial.print("Remain %:   "); Serial.println(String(remainingBatteryPercent, 2));
    Serial.print("Remain Time:"); Serial.println(formattedRemainingBatteryLife);

    writeToDB(voltage, current, remainingCapacityAh, formattedRemainingBatteryLife, "", macAddress, ipAddress, remainingBatteryPercent);

    digitalWrite(kLedPin, LOW);
    delay(1000);
}

// Function to connect to WiFi
void connectToWiFi(const char* ssid, const char* password) {
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    ipAddress = WiFi.localIP().toString();
    macAddress = WiFi.macAddress();
    Serial.println("");
    Serial.print("WiFi connected");
    Serial.print("IP address: "); Serial.println(ipAddress);
    Serial.print("MAC address: "); Serial.println(macAddress);
}

// Function to get battery configuration
void getBatteryConfig(const String& macAddress) {
    WiFiClientSecure client;
    HTTPClient http;
    StaticJsonDocument<200> configDoc;

    client.setInsecure(); // Bypass SSL certificate verification
    http.addHeader("Content-Type", "application/json");

    const String serverPath = "https://us-east-1.aws.data.mongodb-api.com/app/batterymanagementv1-jkqqf/endpoint/GetBatteryDataV1?secret=" + String(SECRET_MONGODBSECRET);
    configDoc["mac_address"] = macAddress;

    String jsonString;
    serializeJson(configDoc, jsonString);

    http.begin(client, serverPath.c_str());
    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode > 0) {
        String payload = http.getString();
        // Serial.println(payload);
        DeserializationError error = deserializeJson(configDoc, payload);

        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            // Handle the deserialization error (e.g., set default values or take appropriate action)
        } else {
            // Update global variables based on the configuration data
            batteryName = configDoc["battery_name"].as<String>();
            batteryCapacityAh = configDoc["battery_capasity_ah"].as<float>();
            shuntAmp = configDoc["shunt_amp"].as<float>();
            shuntDropMv = configDoc["shunt_drop_mv"].as<float>();
            rollingAvgDistance = configDoc["rollingAvgDistance"].as<int>();

            remainingCapacityAh = batteryCapacityAh;
        }
    } else {
        Serial.print("HTTP POST request failed: ");
        Serial.println(httpResponseCode);
        // Handle the HTTP request error (e.g., set default values or take appropriate action)
    }

    http.end();
}

void initializeADS1115() {
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  ads.setGain(GAIN_SIXTEEN);  // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
    ads.setGain(GAIN_SIXTEEN);
    if (!ads.begin()) {
        Serial.println("Failed to initialize ADS.");
        while (1);
    }
}

float takeMeasurement(int adcPin) {
  int16_t adc = ads.readADC_SingleEnded(adcPin);
  float volts = ads.computeVolts(adc);
  return volts;
}

void writeToDB(float voltage, float amperage, float remainingAh, const String& remainingTime, const String& state, const String& macAddress, const String& ipAddress, float remainingPercent) {
    WiFiClientSecure client;
    HTTPClient http;

    client.setInsecure();  // Bypass SSL certificate verification
    http.addHeader("Content-Type", "application/json");
    String serverPath = "https://us-east-1.aws.data.mongodb-api.com/app/batteryupload-ayjsz/endpoint/BatteryUpdaterV3?secret=" + String(SECRET_MONGODBSECRET); // Replace with your actual server URL

    StaticJsonDocument<200> doc;
    doc["battery_name"] = batteryName;
    doc["voltage"] = voltage;
    doc["amperage"] = amperage;
    doc["remaining_ah"] = remainingAh;
    doc["remaining_time"] = remainingTime;
    doc["state"] = state;
    doc["mac_address"] = macAddress;
    doc["ip_address"] = ipAddress;
    doc["remaining_percent"] = remainingPercent;

    String jsonString;
    serializeJson(doc, jsonString);

    http.begin(client, serverPath.c_str());
    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode > 0) {
        // Optionally handle the response content
        // String payload = http.getString();
    } else {
        Serial.print("HTTP POST request failed: ");
        Serial.println(httpResponseCode);
        // Handle the error appropriately
    }

    http.end();
}

float calculateRollingAverage(float currentAverage, float newSample, int sampleCount) {
    currentAverage -= currentAverage / sampleCount;
    currentAverage += newSample / sampleCount;
    return currentAverage;
}

String formatTime(long seconds) {
    long hours = seconds / 3600;  // Convert seconds to hours
    long remainingSeconds = seconds % 3600;
    long minutes = remainingSeconds / 60;  // Convert remaining seconds to minutes
    long secs = remainingSeconds % 60;  // Remaining seconds

    char formattedTime[9];  // Buffer to hold the formatted time
    sprintf(formattedTime, "%02ld:%02ld:%02ld", hours, minutes, secs);

    return String(formattedTime);
}
