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
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <array>

#define ONE_WIRE_BUS D3  // Pin D3 on NodeMCU connected to the data line of DS18B20

// Constants
const int kLedPin = 2;
const int kAnalogPin = A0;
const int kShuntVoltageAdcPin = 1;
const int kBatteryVoltageAdcPin = 0;
const int kDefaultAvgDistance = 5;
const float kShuntAmp = 20.0f; // 75 mV = 20 Amps
const float kShuntDropMv = 0.075f;  // 75 millivolts
const float kBatteryCapacityAh = 9.0f;
const bool kSmsEnabled = false;
const float kDischargingThresholdAmps = 0.2f;
const float kChargingThresholdAmps = 0.2f;
const bool kWriteRecordingsToDB = true;
const float kMaxBatteryVoltage = 29.2;
const int SCREEN_WIDTH = 128; // OLED display width, in pixels 
const int SCREEN_HEIGHT = 64; // OLED display height, in pixels
const float kShuntOhms = 0.30; // Shunt resistor value in ohms
const int kRollingAverageSize = 5;  // Set the size of the rolling average
const float kCalibrationOffset = 0.0;

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
float shuntOhms = kShuntOhms; // Shunt resistance set to 0.25 ohms
bool smsEnabled = kSmsEnabled;
float dischargingThresholdAmps = kDischargingThresholdAmps;
float chargingThresholdAmps = kChargingThresholdAmps;
bool writeRecordingsToDB = kWriteRecordingsToDB;
float maxBatteryVoltage = kMaxBatteryVoltage;
float calibrationOffset = kCalibrationOffset;
std::array<float, kRollingAverageSize> rollingMeasurements0{};
std::array<float, kRollingAverageSize> rollingMeasurements1{};

bool smsSent = false;
bool currentExceeded = false;
Chrono smsTimer;
String batteryState = "Charging";

struct MeasurementValues {
    float calculatedVoltage;
    float measuredVoltage;
};

// Declaration for an SSD1306 display connected to I2C (SCL, SDA pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// Temp Setup
OneWire oneWire(ONE_WIRE_BUS);  // Create a OneWire instance
DallasTemperature sensors(&oneWire);  // Pass the OneWire reference to Dallas Temperature library

// Function Prototypes
// Connects to a WiFi network using the provided SSID and password
void connectToWiFi(const char* ssid, const char* password, int maxRetries);

// Retrieves battery configuration data from a database using the MAC address
void getBatteryConfig(const String& macAddress);

// Initalize the OLED Screen
void initializeOLED();

// get temp of env
float getTemp();

// Set OLED Screen
void writeOledLine(String text, int line);
void setScreen(String arr[], int size);
void clearScreen();

// Initializes the ADS1115 Analog-to-Digital Converter (ADC)
void initializeADS1115();
void calibrateOffset();

// Takes a voltage measurement from a specified ADC pin
MeasurementValues takeMeasurement(int adcPin);

// Writes battery data to a database, including shuntVoltage, amperage, remaining capacity, and other details
void writeToDB(float shuntVoltage, float amperage, float remainingAh, const String& remainingTime, const String& state, const String& macAddress, const String& ipAddress, float remainingPercent, float batteryVoltage, float tempC);
void saveCalibration(const String& macAddress, float calibrationValue);

// Formats a given time in seconds into a string in the format "HH:MM:SS"
String formatTime(long seconds);

void setup() {
    pinMode(D8, INPUT);
    // Temp Vars
    String tempArray[8] = {"Starting..."};

    // Set up the OLED
    initializeOLED();
    clearScreen();
    // Print State
    tempArray[1] = "WiFi:Connecting";
    setScreen(tempArray, 1);
    // Serial output
    Serial.begin(115200);
    // Connect to WiFi (SSID and Password in arduino_secrets.h)
    connectToWiFi(SECRET_SSID, SECRET_PASS, 5);  // Retry up to 5 times
    // Print State
    tempArray[1] = "WiFi:Connected";
    tempArray[2] = "Configs:Downloading";
    setScreen(tempArray, 2);
    // using the mac_address, pull the configurations for this deployment from MongoDB
    getBatteryConfig(macAddress);
    // Print State
    tempArray[2] = "Configs:Downloaded";
    tempArray[3] = "ADS:Initializing";
    setScreen(tempArray, 3);
    // Set up ADC (ADS1115)
    pinMode(kLedPin, OUTPUT);
 
    initializeADS1115();
    // Print State
    tempArray[3] = "ADS:Initialized";
    setScreen(tempArray, 3);

    // Calibrate if necessary
    calibrateOffset();

    Serial.println("Starting...");
    // Setup complete!
}

void loop() {
    digitalWrite(kLedPin, HIGH);

    // Take a measurment of the shunt
    float measurementInterval = myChrono.elapsed();
    MeasurementValues measurementShuntValues = takeMeasurement(kShuntVoltageAdcPin);

    // Calculate the actual battery voltage using the voltage divider formula
    MeasurementValues measurementBatteryValues = takeMeasurement(kBatteryVoltageAdcPin);
    float batteryVoltage = (measurementBatteryValues.calculatedVoltage * maxBatteryVoltage)/0.256; // highest = 29.2 (batteryVoltageMeasured * 29.2)/100, because the voltage is greater than 0.256

    myChrono.restart();

    Serial.println("Starting...");

    // Calculate the current in amperes by dividing the average voltage by the shunt resistance
    float current = (measurementShuntValues.calculatedVoltage / shuntOhms) * 100;
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
                smsSent = false; // Reset the flag to allow a new SMS to be sent when the current goes above 0.2 amps again.
                batteryState = "Charging";
                smsTimer.restart(); // Restart the timer for timing the next SMS alert.
            }
        }
    }
    float tempC = getTemp();
    Serial.println("-----------|-------");
    Serial.print("Shunt Calculated V:    "); Serial.println(String(measurementShuntValues.calculatedVoltage, 7));
    Serial.print("Shunt Measured V:    "); Serial.println(String(measurementShuntValues.measuredVoltage, 7));
    Serial.print("Shunt I:    "); Serial.println(String(current, 7));
    Serial.print("Remain Ah:  "); Serial.println(String(remainingCapacityAh, 7));
    Serial.print("Remain %:   "); Serial.println(String(remainingBatteryPercent, 2));
    Serial.print("Remain Time:"); Serial.println(formattedRemainingBatteryLife);
    Serial.print("Battery V:  "); Serial.println(String(batteryVoltage, 7)); // Testing
    Serial.print("Temp C:    "); Serial.println(String(tempC, 7)); // Testing

    clearScreen();
    String screenStringArray[6] = {
        "Batty V:" + String(batteryVoltage, 6),
        "Shunt V:" + String(measurementShuntValues.calculatedVoltage, 6),
        "Shunt I:" + String(current, 6),
        "Time   :" + formattedRemainingBatteryLife,
        "Batty %:" + String(remainingBatteryPercent, 2),
        "Temp C :" + String(tempC, 4)
    };

    int screenArrayLength = sizeof(screenStringArray) / sizeof(screenStringArray[0]);
    setScreen(screenStringArray, screenArrayLength);

    if (writeRecordingsToDB){
        writeToDB(measurementBatteryValues.measuredVoltage, current, remainingCapacityAh, formattedRemainingBatteryLife, "", macAddress, ipAddress, remainingBatteryPercent, batteryState, batteryVoltage, tempC);
    }

    digitalWrite(kLedPin, LOW);
    delay(1000);
}

// Function to connect to WiFi
void connectToWiFi(const char* ssid, const char* password, int maxRetries = 5) {
    int retryCount = 0;
    bool connected = false;

    Serial.print("Attempting to connect to WiFi");

    while (retryCount < maxRetries && !connected) {
        WiFi.begin(ssid, password);  // Start the connection process

        // Wait for connection
        for (int i = 0; i < 10; i++) {  // Wait 5 seconds for connection
            if (WiFi.status() == WL_CONNECTED) {
                connected = true;
                break;
            }
            delay(500);
            Serial.print(".");
        }

        if (connected) {
            break;
        }

        retryCount++;
        Serial.println("\nRetry " + String(retryCount) + "/" + String(maxRetries));
    }

    if (connected) {
        ipAddress = WiFi.localIP().toString();
        macAddress = WiFi.macAddress();
        Serial.println("\nWiFi connected");
        Serial.print("IP address: "); Serial.println(ipAddress);
        Serial.print("MAC address: "); Serial.println(macAddress);
    } else {
        Serial.println("\nFailed to connect to WiFi after retries. Rebooting...");
        ESP.restart();  // Reboot the microcontroller
    }
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

    if (httpResponseCode == 200) {
        String payload = http.getString();
        // Serial.println(payload);
        DeserializationError error = deserializeJson(configDoc, payload);

        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            // Handle the deserialization error (e.g., set default values or take appropriate action)
            ESP.restart();  // Restart the ESP if the HTTP request fails
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
            maxBatteryVoltage = configDoc["maxBatteryVoltage"].as<float>();
            calibrationOffset = configDoc["adc1115A1CalibrationOffset"].as<float>();

            remainingCapacityAh = batteryCapacityAh;
        }
    } else {
        Serial.print("HTTP POST request failed: ");
        Serial.println(httpResponseCode);
        // Handle the HTTP request error (e.g., set default values or take appropriate action)
        ESP.restart();  // Restart the ESP if the HTTP request fails
    }

    http.end();
}

void initializeOLED() {
    // Initialize with the I2C addr 0x3C (for the 128x32)
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
      Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
    }
    else {
      Serial.println(F("SSD1306 allocation success"));
    }
}

float getTemp() {
    sensors.requestTemperatures();  // Send the command to get temperatures
    return sensors.getTempCByIndex(0); // temp in Celsius
}

void clearScreen() {
    display.clearDisplay();
    display.setTextSize(1);      // Normal 1:1 pixel scale
    display.setTextColor(WHITE); // Draw white text

    display.display();
}

void writeOledLine(String text, int line){
    display.setCursor(0, line * 8);      // Start at top-left corner
    display.println(text);
}

void setScreen(String arr[], int size) {
    clearScreen();
    Serial.println(String(size));
    for (int i = 0; i < size; ++i) {
      writeOledLine(arr[i], i);
    }
    display.display();
}

void initializeADS1115() {
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  ads.setGain(GAIN_SIXTEEN);       // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
    ads.setGain(GAIN_SIXTEEN);
    if (!ads.begin()) {
        Serial.println("Failed to initialize ADS.");
        while (1);
    }
}

int rollingIndex0 = 0;
int rollingIndex1 = 0;

MeasurementValues takeMeasurement(int adcPin) {
    MeasurementValues mv;
    float currentMeasurement = ads.readADC_SingleEnded(adcPin);

    // Decide which rolling average array to use based on the ADC pin
    if (adcPin == 0) {
        // Update rolling average for ADC Pin 0
        rollingMeasurements0[rollingIndex0] = currentMeasurement;
        rollingIndex0 = (rollingIndex0 + 1) % kRollingAverageSize;

        // Calculate the rolling average for ADC Pin 0
        float sum0 = 0;
        for (float measurement : rollingMeasurements0) {
            sum0 += measurement;
        }
        mv.measuredVoltage = sum0 / kRollingAverageSize;
    } else if (adcPin == 1) {
        // Update rolling average for ADC Pin 1
        rollingMeasurements1[rollingIndex1] = currentMeasurement;
        rollingIndex1 = (rollingIndex1 + 1) % kRollingAverageSize;

        // Calculate the rolling average for ADC Pin 1
        float sum1 = 0;
        for (float measurement : rollingMeasurements1) {
            sum1 += measurement;
        }
        mv.measuredVoltage = (sum1 / kRollingAverageSize) - calibrationOffset;
    }

    // Convert the average measurement to volts for the specific pin
    mv.calculatedVoltage = ads.computeVolts(mv.measuredVoltage);

    return mv;
}

void calibrateOffset() {
    pinMode(D8, INPUT); // Ensure D8 is set as input
    if (digitalRead(D8) == HIGH) {
        clearScreen();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("Calibrating...");

        float sum = 0;
        int samples = 1000;
        for (int i = 0; i < samples; i++) {
            sum += ads.readADC_SingleEnded(1);
            delay(50); // Wait for 50 milliseconds between samples
            
            // Update progress bar on the OLED
            int progress = (i + 1) * 100 / samples; // Calculate progress in percentage
            displayProgressBar(progress);
        }
        calibrationOffset = sum / samples;
        
        display.clearDisplay();
        display.setCursor(0, 0);
        display.print("Calibration Complete");
        display.setCursor(0, 8);
        display.print("Offset: " + String(calibrationOffset, 6));
        display.display(); // Display the final message
        // Save calibration to the database after calibration is complete
        saveCalibration(macAddress, calibrationOffset);
        display.setCursor(0, 16);
        display.print("Saved...");
        display.display(); // Display the final message
        delay(2000); // Hold the final message for 2 seconds
    }
}

void displayProgressBar(int progress) {
    display.drawRect(0, 16, 124, 8, WHITE); // Draw progress bar border
    display.fillRect(2, 18, progress * 120 / 100, 4, WHITE); // Fill progress inside the border based on the current progress
    display.display(); // Update display with the progress
    display.fillRect(2, 18, 120, 4, BLACK); // Clear the progress area for the next update
}

void writeToDB(float shuntVoltage, float amperage, float remainingAh, const String& remainingTime, const String& state, const String& macAddress, const String& ipAddress, float remainingPercent, String batteryState, float batteryVoltage, float tempC) {
    WiFiClientSecure client;
    HTTPClient http;

    client.setInsecure();  // Bypass SSL certificate verification
    http.addHeader("Content-Type", "application/json");
    String serverPath = "https://us-east-1.aws.data.mongodb-api.com/app/batteryupload-ayjsz/endpoint/BatteryUpdaterV3?secret=" + String(SECRET_MONGODBSECRET); // Replace with your actual server URL

    StaticJsonDocument<200> doc;
    doc["battery_name"] = batteryName;
    doc["shuntVoltage"] = shuntVoltage;
    doc["amperage"] = amperage;
    doc["remaining_ah"] = remainingAh;
    doc["remaining_time"] = remainingTime;
    doc["state"] = state;
    doc["mac_address"] = macAddress;
    doc["ip_address"] = ipAddress;
    doc["remaining_percent"] = remainingPercent;
    doc["batteryState"] = batteryState;
    doc["batteryVoltage"] = batteryVoltage;
    doc["batteryTempC"] = tempC;

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

void saveCalibration(const String& macAddress, float calibrationValue) {
    WiFiClientSecure client;
    HTTPClient http;
    
    // Prepare the JSON document with the MAC address and calibration value
    StaticJsonDocument<200> doc;
    doc["mac_address"] = macAddress;
    doc["calibration_value"] = calibrationValue;
    
    String jsonString;
    serializeJson(doc, jsonString);  // Serialize the JSON data to a string

    // Set up the HTTPS connection
    client.setInsecure();  // Note: Only use setInsecure() in non-production environments
    http.addHeader("Content-Type", "application/json");
    String serverPath = "https://us-east-1.aws.data.mongodb-api.com/app/batterymanagementv1-jkqqf/endpoint/SaveCalibrationV1?secret=" + String(SECRET_MONGODBSECRET);

    // Begin the HTTP POST request
    http.begin(client, serverPath.c_str());
    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode == 200) {
        // Optionally log the response or handle it further
        Serial.print("Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();  // Get the response payload
        Serial.println(payload);
    } else {
        Serial.print("HTTP POST request failed with error: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();  // Get the response payload
        Serial.println(payload);
    }

    http.end();  // Close the HTTP connection
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