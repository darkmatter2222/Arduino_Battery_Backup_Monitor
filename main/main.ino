#include <arduino_secrets.h>

#include <Adafruit_ADS1X15.h>
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
//Adafruit_ADS1015 ads;     /* Use this for the 12-bit version */
int LED1 = 2;      // Assign LED1 to pin GPIO2
/*
 * Circuits4you.com
 * Get IP Address of ESP8266 in Arduino IDE
*/
#define analogPin A0 /* ESP8266 Analog Pin ADC0 = A0 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

int adcValue = 0;  /* Variable to store Output of ADC */
const char* wifiName = SECRET_SSID;
const char* wifiPass = SECRET_PASS;
  

 
void setup(void)
{
  Serial.begin(115200);

  Serial.print("Connecting to ");
  Serial.println(wifiName);

  WiFi.begin(wifiName, wifiPass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());   //You can get IP address assigned to ESP
  pinMode(LED1, OUTPUT);
  // The ADC input range (or gain) can be changed via the following
  // functions, but be careful never to exceed VDD +0.3V max, or to
  // exceed the upper and lower limits if you adjust the input range!
  // Setting these values incorrectly may destroy your ADC!
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  //ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
  if (!ads.begin())
  {
    Serial.println("Failed to initialize ADS.");
    while (1);
  }
}

float avg0 = 0;
float avg1 = 0;
float avg2 = 0;
float avg3 = 0;
int N = 5;

// 75 mv = 20 amps
float shunt_amp = 20;
float shunt_drop_mv = 0.075; // 75 millivolts 
String battery_name = "Fiber_Internet";

int WriteToDB(float voltage, float amperage){
  WiFiClientSecure client;
  HTTPClient http;
  adcValue = analogRead(analogPin);

  client.setInsecure(); // Bypass SSL certificate verification
  http.addHeader("Content-Type", "application/json");
  String serverPath = "https://us-east-1.aws.data.mongodb-api.com/app/batteryupload-ayjsz/endpoint/BatteryUpdaterV3?secret=" + String(SECRET_MONGODBSECRET);
  String body = "{\"shunt_drop_voltage\":"+String(voltage, 7)+", \"calculated_amperage\":"+String(amperage, 7)+" , \"battery_name\":\""+battery_name+"\"}";
  http.begin(client, serverPath.c_str());
  int httpResponseCode = http.POST(body);
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
    // Free resources
  http.end();
  return httpResponseCode;
}

double approxRollingAverage(double avg, double new_sample) {
  avg -= avg / N;
  avg += new_sample / N;
  return avg;
}

int startup_loop = 0;

void loop(void)
{
  digitalWrite(LED1, HIGH);
  int16_t adc0, adc1, adc2, adc3;
  float volts0, volts1, volts2, volts3;
 
  adc0 = ads.readADC_SingleEnded(0);
  adc1 = ads.readADC_SingleEnded(1);
  adc2 = ads.readADC_SingleEnded(2);
  adc3 = ads.readADC_SingleEnded(3);
 
  volts0 = ads.computeVolts(adc0);
  volts1 = ads.computeVolts(adc1);
  volts2 = ads.computeVolts(adc2);
  volts3 = ads.computeVolts(adc3);

  avg0 = approxRollingAverage(avg0, volts0);
  avg1 = approxRollingAverage(avg1, volts1);
  avg2 = approxRollingAverage(avg2, volts2);
  avg3 = approxRollingAverage(avg3, volts3);

  if (startup_loop < N)
  {
    startup_loop++;
    Serial.print(".");
    return;
  }
  else if (startup_loop == N)
  {
    startup_loop++;
    Serial.println("Starting...");
  }


  Serial.println("-----------------------------------------------------------");
  Serial.print("Avg 0: "); Serial.print(String(avg0, 7)); Serial.print("V "); Serial.print("AIN0: "); Serial.print(adc0); Serial.print("  "); Serial.print(String(volts0, 7)); Serial.println("V");
  Serial.print("Avg 1: "); Serial.print(String(avg1, 7)); Serial.print("V "); Serial.print("AIN1: "); Serial.print(adc1); Serial.print("  "); Serial.print(String(volts1, 7)); Serial.println("V");
  Serial.print("Avg 2: "); Serial.print(String(avg2, 7)); Serial.print("V "); Serial.print("AIN2: "); Serial.print(adc2); Serial.print("  "); Serial.print(String(volts2, 7)); Serial.println("V");
  Serial.print("Avg 3: "); Serial.print(String(avg3, 7)); Serial.print("V "); Serial.print("AIN3: "); Serial.print(adc3); Serial.print("  "); Serial.print(String(volts3, 7)); Serial.println("V");

  float amperage1 = (avg1 * shunt_amp) / shunt_drop_mv;

  WriteToDB(avg1, amperage1);
  digitalWrite(LED1, LOW);
  delay(1000);
}