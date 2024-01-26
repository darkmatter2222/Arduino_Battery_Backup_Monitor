# Arduino Battery Backup Monitor

## Overview
The Arduino Battery Backup Monitor is a sophisticated system designed to monitor the health of several battery backups in a house and upload the status to MongoDB Cloud. It leverages a NodeMCU microcontroller in conjunction with an ADS1115 Analog-to-Digital Converter for accurate voltage measurements. The system is versatile and scalable, with configurations that can be customized for different deployment scenarios.

## Features
- **Real-time Monitoring:** Monitors battery usage by measuring voltage drop across a shunt resistor.
- **Battery Health Analysis:** Calculates current draw and computes remaining battery capacity.
- **Cloud Integration:** Uploads data to MongoDB for tracking and analysis.
- **Customizable Configurations:** Downloads specific configurations using the unique MAC address of the device.
- **Data Visualization:** Utilizes MongoDB Charts for an interactive dashboard of battery status.

## Demonstration Video

For a detailed demonstration and explanation of the Arduino Battery Backup Monitor project, check out the following video:

[![Arduino Battery Backup Monitor Demonstration](https://img.youtube.com/vi/NTwmsFVzf2c/0.jpg)](https://www.youtube.com/watch?v=NTwmsFVzf2c)  

Click the image above to watch the video on YouTube.  

## Hardware Requirements
- NodeMCU microcontroller (ESP8266)
- ADS1115 Analog-to-Digital Converter
- Shunt resistor (specific to your application)
- Power supply for the NodeMCU

## Software Dependencies
- Arduino IDE for code development and uploading
- Libraries:
  - ESP8266WiFi
  - ESP8266HTTPClient
  - WiFiClientSecure
  - Adafruit_ADS1X15
  - Chrono
  - ArduinoJson

## Installation
1. **Set Up Hardware:** Assemble the hardware components as per the circuit diagram.
2. **Configure Software:** Update `arduino_secrets.h` with your WiFi credentials and MongoDB secret.
3. **Compile and Upload:** Open `Arduino_Battery_Backup_Monitor.ino` in Arduino IDE, compile the code, and upload it to the NodeMCU.

## Circuit Diagram

A detailed circuit diagram is provided to help with the hardware setup of the Arduino Battery Backup Monitor. The diagram illustrates how the NodeMCU, ADS1115 Analog-to-Digital Converter, shunt resistor, and other components are interconnected.

![Circuit Diagram](https://github.com/darkmatter2222/Arduino_Battery_Backup_Monitor/blob/main/images/circuit-diagram.png)

Please refer to this diagram for accurate connections and setup to ensure the proper functioning of the system.  

## Code Structure
- **Arduino_Battery_Backup_Monitor.ino:** Main file containing the logic for battery monitoring and data uploading.
- **arduino_secrets.h:** Contains sensitive data like WiFi credentials and MongoDB API secret (not included for security reasons).

## MongoDB Document Structure

The Arduino Battery Backup Monitor project uses a specific MongoDB document structure to store and manage the data collected from the battery backups. This structure is defined in the `Battery_Management_Template.json` file, and it includes several key fields:

- `device_id`: A unique identifier for each monitoring device. (Set by mongo when creating the document)  
- `timestamp`: The date and time when the data was recorded, stored in ISO 8601 format. (Set by the insert function in mongo app service)
- `voltage`: The measured voltage of the voltage drop across the shunt in volts.
- `current`: The current being drawn from the battery in amperes.
- `battery_level`: An estimate of the remaining battery capacity as a percentage.

## Usage
After successful deployment, the system will:
- Connect to WiFi and download configuration based on the MAC address.
- Begin monitoring battery health and uploading data to the cloud.
- The battery status can be viewed in real-time using MongoDB Charts.

## Dashboard
The real-time data is visualized using MongoDB Charts, which can be accessed [here](https://charts.mongodb.com/charts-homeautomation-snhch/public/dashboards/bce944aa-81ec-43b3-b50e-45cdf96755d5).

## Resources
- [NodeMCU on Amazon](https://www.amazon.com/s?k=node+mcu)
- [ADS1115 on Amazon](https://www.amazon.com/s?k=ads1115)

## Acknowledgments
- Development and code authoring involved leveraging OpenAI's GPT-4.
- Special thanks to Ryan Susman for his contributions to the development and refinement of this system.

## License
This project is licensed under the Apache License - see the LICENSE file for details.
