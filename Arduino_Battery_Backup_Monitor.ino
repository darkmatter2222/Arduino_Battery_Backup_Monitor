/**
 * @file main.cpp
 * @brief Battery Monitoring and Data Logging System
 *
 * This program is designed to monitor battery usage by measuring the voltage drop across a shunt resistor
 * using an ADS1115 Analog-to-Digital Converter. It calculates the current draw from the battery and
 * computes the remaining battery capacity. The data is then uploaded to a database for tracking and analysis.
 *
 * The system is designed to be versatile for deployment in multiple scenarios. Upon initialization,
 * it uses the unique MAC address of the device to download specific configurations for the particular
 * deployment from a remote server. This approach allows for easy customization and scalability of the system.
 *
 * The development of this program involved leveraging OpenAI's GPT-4 for a significant portion of the code
 * authoring, in collaboration with Ryan Susman, who contributed to the development and refinement of the system.
 *
 * @author Ryan Susman
 * @date 2024-01-21
 */

// Includes
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_ADS1X15.h>
#include <Chrono.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"
#include <time.h>

// Constants
const int kLedPin = 2;
const int kAnalogPin = A0;
const int kAdcPin = 1;
const int kDefaultAvgDistance = 5;
const float kShuntAmp = 20.0f; // 75 mV = 20 Amps
const float kShuntDropMv = 0.075f;  // 75 millivolts
const float kBatteryCapacityAh = 9.0f;
const bool kSmsEnabled = false;
const float kDischargingThresholdAmps = 0.2f;
const float kChargingThresholdAmps = 0.2f;
const bool kWriteRecordingsToDB = true;


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
bool smsEnabled = kSmsEnabled;
float dischargingThresholdAmps = kDischargingThresholdAmps;
float chargingThresholdAmps = kChargingThresholdAmps;
bool writeRecordingsToDB = kWriteRecordingsToDB;

bool smsSent = false;
bool currentExceeded = false;
Chrono smsTimer;
String batteryState = "Charging";

// Function Prototypes
// Connects to a WiFi network using the provided SSID and password
void connectToWiFi(const char* ssid, const char* password);

// Retrieves battery configuration data from a database using the MAC address
void getBatteryConfig(const String& macAddress);

// Initializes the ADS1115 Analog-to-Digital Converter (ADC)
void initializeADS1115();

// Takes a voltage measurement from a specified ADC pin
float takeMeasurement(int adcPin);

// Writes battery data to a database, including voltage, amperage, remaining capacity, and other details
void writeToDB(float voltage, float amperage, float remainingAh, const String& remainingTime, const String& state, const String& macAddress, const String& ipAddress, float remainingPercent);

// Calculates a rolling average for a given sample and current average over a specified number of samples
float calculateRollingAverage(float currentAverage, float newSample, int sampleCount);

// Formats a given time in seconds into a string in the format "HH:MM:SS"
String formatTime(long seconds);

// Function to send a text message using Twilio API
void sendTextMessage(const String& messageBody);

void setup() {
    // Serial output
    Serial.begin(115200);
    // Connect to WiFi (SSID and Password in arduino_secrets.h)
    connectToWiFi(SECRET_SSID, SECRET_PASS);
    // using the mac_address, pull the configurations for this deployment from MongoDB
    getBatteryConfig(macAddress);

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // Configure NTP (you might need to adjust this for your timezone)
    Serial.println("Waiting for time sync...");
    while (!time(nullptr)) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("\nTime synchronized");

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

    // Check if the SMS alert feature is enabled and if the current measurement conditions warrant an SMS alert.
    if (smsEnabled) {
        // Check if the current exceeds the threshold of 0.2 amps.
        if (current > dischargingThresholdAmps) {
            currentExceeded = true; // Set a flag indicating that the current has exceeded the threshold.

            // Check if an SMS has not been sent yet or if it has been more than 2 hours since the last SMS.
            if (!smsSent || smsTimer.hasPassed(7200000)) { // 7200000 milliseconds = 2 hours
                // Send an SMS message to alert that the current has exceeded 0.2 amps. Customize the message and recipient.
                sendTextMessage("Battery:" + batteryName + " Current exceeds 0.2 Amps");
                smsSent = true; // Set the flag to indicate that an SMS has been sent.
                batteryState = "Discharging";
                smsTimer.restart(); // Restart the timer to track the interval for the next SMS alert.
            }
        } 
        // Check if the current has dropped below 0.2 amps after having previously exceeded it.
        else if (currentExceeded && current <= chargingThresholdAmps) {
            currentExceeded = false; // Reset the flag as the current is now below the threshold.

            // Check if an SMS was sent when the current exceeded the threshold.
            if (smsSent) {
                // Send an SMS message to alert that the current has dropped back below 0.2 amps. Customize the message and recipient.
                sendTextMessage("Battery:" + batteryName + " Current has dropped below 0.2 Amps");
                smsSent = false; // Reset the flag to allow a new SMS to be sent when the current goes above 0.2 amps again.
                batteryState = "Charging";
                smsTimer.restart(); // Restart the timer for timing the next SMS alert.
            }
        }
    }

    Serial.println("-----------|-------");
    Serial.print("V:          "); Serial.println(String(voltage, 7));
    Serial.print("Avg V:      "); Serial.println(String(voltageAverage, 7));
    Serial.print("I:          "); Serial.println(String(current, 7));
    Serial.print("Remain Ah:  "); Serial.println(String(remainingCapacityAh, 7));
    Serial.print("Remain %:   "); Serial.println(String(remainingBatteryPercent, 2));
    Serial.print("Remain Time:"); Serial.println(formattedRemainingBatteryLife);
    Serial.print("Battery V:"); Serial.println((takeMeasurement(0) * 13.35) / 0.23);
    

    if (writeRecordingsToDB){
        writeToDB(voltage, current, remainingCapacityAh, formattedRemainingBatteryLife, "", macAddress, ipAddress, remainingBatteryPercent, batteryState);
    }

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
            smsEnabled = configDoc["smsEnabled"].as<bool>();
            dischargingThresholdAmps = configDoc["dischargingThresholdAmps"].as<float>();
            chargingThresholdAmps = configDoc["chargingThresholdAmps"].as<float>();
            writeRecordingsToDB = configDoc["writeRecordingsToDB"].as<bool>();

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

void writeToDB(float voltage, float amperage, float remainingAh, const String& remainingTime, const String& state, const String& macAddress, const String& ipAddress, float remainingPercent, String batteryState) {
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
    doc["batteryState"] = batteryState;

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

// Function to send a text message using Twilio API
void sendTextMessage(const String& messageBody) {
    WiFiClientSecure client;
    HTTPClient http;

    client.setInsecure();  // Bypass SSL certificate verification

    String serverPath = "https://api.twilio.com/2010-04-01/Accounts/" + String(SECRET_TWILIOUSERNAME) + "/Messages.json";

    http.begin(client, serverPath);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.setAuthorization(String(SECRET_TWILIOUSERNAME).c_str(), String(SECRET_TWILIOPASSWORD).c_str());

    String postData = "To=" + String(SECRET_TWILIOTO) + "&From=" + String(SECRET_TWILIOFROM) + "&Body=" + messageBody;

    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
        // Optionally handle the response content
        String payload = http.getString();
        Serial.println(payload);
    } else {
        Serial.print("HTTP POST request failed: ");
        Serial.println(httpResponseCode);
        // Handle the error appropriately
    }

    http.end();
}

int month() {
    time_t now = time(nullptr);  // Get the current time as a time_t object
    struct tm *timeStruct = localtime(&now);  // Convert time to struct tm form

    return timeStruct->tm_mon + 1;  // tm_mon is months since January (0-11), so add 1 for human-readable months (1-12)
}

bool isSummer(int month) {
    // Summer months are June to September (6 to 9)
    return month >= 6 && month <= 9;
}
