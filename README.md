# Arduino Battery Backup Monitor

## Overview
The Arduino Battery Backup Monitor is a sophisticated system designed to monitor the health of several battery backups in a house and upload the status to MongoDB Cloud. It leverages a NodeMCU microcontroller in conjunction with an ADS1115 Analog-to-Digital Converter for accurate voltage measurements. The system is versatile and scalable, with configurations that can be customized for different deployment scenarios.

## Features
- **Real-time Monitoring:** Monitors battery usage by measuring voltage drop across a shunt resistor.
- **Battery Health Analysis:** Calculates current draw and computes remaining battery capacity.
- **Cloud Integration:** Uploads data to MongoDB for tracking and analysis.
- **Customizable Configurations:** Downloads specific configurations using the unique MAC address of the device.
- **Data Visualization:** Utilizes MongoDB Charts for an interactive dashboard of battery status.
- **SMS Notifications:** Sends alerts via SMS based on predefined criteria, such as low battery level or significant changes in battery health.

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
  
[![How To Build the battery backup monitor](https://img.youtube.com/vi/o8SodRMNmwI/0.jpg)](https://www.youtube.com/watch?v=o8SodRMNmwI)  

Click the image above to watch the video on YouTube.  
  
The Arduino Battery Backup Monitor project uses a specific MongoDB document structure to store and manage the data collected from the battery backups. This structure is defined in the `Battery_Management_Template.json` file, and it includes several key fields:  
  
### Logging  

- `_id`: A unique identifier generated by MongoDB, represented by `$oid`, ensuring each document's uniqueness in the database.
- `battery_name`: The name of the battery being monitored, e.g., "Fiber_Internet".
- `voltage`: The measured voltage of the battery, in volts. For example, `-0.00009375` volts in this document.
- `amperage`: The current flowing through the battery, in amperes. In this case, `-0.027991947` amperes.
- `remaining_ah`: The remaining capacity of the battery in ampere-hours (Ah). Here, it is `18` Ah.
- `remaining_time`: An estimation of the remaining time before the battery is depleted, shown as "hours:minutes:seconds". In this example, it is "-643:-2:-31".
- `state`: The current state of the battery. This field can be used to indicate specific conditions or states of the battery.
- `mac_address`: The MAC address of the device monitoring the battery, in this instance "48:55:19:ED:97:A1".
- `ip_address`: The IP address of the monitoring device, e.g., "192.168.86.20".
- `remaining_percent`: The remaining battery capacity as a percentage, which is `100` in this document.
- `date`: The date and time when the data was recorded, stored in ISO 8601 format. In this example, it is "2024-01-26T01:25:30.568Z".

### Configuration

- `_id`: A unique identifier automatically generated by MongoDB. This `$oid` field is used to uniquely identify each document in the database.
- `mac_address`: The MAC address of the device monitoring the battery. This address is used to uniquely identify the hardware device in the network.
- `battery_name`: A user-defined name for the battery, useful for easy identification and reference.
- `battery_capasity_ah`: The capacity of the battery in ampere-hours (Ah). This value is crucial for calculating the battery's remaining life and performance.
- `shunt_amp`: The maximum current rating of the shunt resistor used in the measurement setup, in amperes.
- `shunt_drop_mv`: The voltage drop across the shunt resistor, measured in millivolts (mV). This is used for calculating the current draw from the battery.
- `rollingAvgDistance`: The number of data points used for calculating the rolling average in data analysis. This helps in smoothing out fluctuations and getting a more accurate representation of the data.
- `smsEnabled`: A boolean value indicating whether SMS notifications are enabled for this battery. This can be used for alerting purposes in case of specific battery conditions.


## Usage
After successful deployment, the system will:
- Connect to WiFi and download configuration based on the MAC address.
- Begin monitoring battery health and uploading data to the cloud.
- The battery status can be viewed in real-time using MongoDB Charts.
- Send SMS notifications based on specific conditions such as low battery level or significant changes in battery health.

## Dashboard
The real-time data is visualized using MongoDB Charts, which can be accessed [here](https://charts.mongodb.com/charts-homeautomation-snhch/public/dashboards/bce944aa-81ec-43b3-b50e-45cdf96755d5).

## Resources
- [NodeMCU on Amazon](https://www.amazon.com/s?k=node+mcu)
- [ADS1115 on Amazon](https://www.amazon.com/s?k=ads1115)
- [20 Amp Shunt on Amazon](https://www.amazon.com/s?k=shunt+20A+75mV)

## Acknowledgments
- Development and code authoring involved leveraging OpenAI's GPT-4.
- Special thanks to Ryan Susman for his contributions to the development and refinement of this system.

## License
This project is licensed under the Apache License - see the LICENSE file for details.
