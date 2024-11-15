/* 
Equipment and Sensor Setup
Dry Bulb Temperature Sensor: The dry bulb temperature is the standard air temperature, which can be measured with a simple temperature sensor (such as the DHT11 for basic applications or more precise sensors for accuracy).
Wet Bulb Temperature Sensor: The wet bulb temperature measures air cooling due to evaporation and requires a sensor to be wrapped in a wet wick and ventilated. You may use a second temperature sensor and keep its probe wet. The wet bulb temperature will generally be lower than the dry bulb temperature unless the air is saturated.

Edge Device (ESP8266/ESP32) Setup
Connect and configure both temperature sensors to measure dry and wet bulb temperatures.
Set up the humidity sensor to capture real-time relative humidity.

1. Understanding Dry and Wet Bulb Temperatures
Dry Bulb Temperature (DBT):
This is the air temperature measured by a thermometer freely exposed to the air but shielded from radiation and moisture.
Wet Bulb Temperature (WBT):
This is measured using a thermometer with a wet cloth covering the bulb. The evaporation of water from the cloth cools the thermometer, depending on the ambient humidity.
The difference between DBT and WBT indicates the amount of moisture in the air, which is crucial for calculating cloud height.

2. Equipment Setup
Sensors:

DHT11/DHT22: Measures dry bulb temperature and relative humidity.
Wet Bulb Temperature: Calculated indirectly using the dry bulb temperature and relative humidity with an empirical formula:
WBT=DBT × arctan(0.151977 × RH+8.313659) + arctan(DBT+RH)−arctan(RH−1.676331)+0.00391838×(RH)1.5 × arctan(0.023101×RH)−4.686035

Edge Processing:

Calculate wet bulb temperature and cloud height on the ESP8266.
Publish only the computed cloud height and key metrics to the cloud to reduce bandwidth usage.
*/
 
#include <ESP8266WiFi.h> // handling Wi-Fi 
#include <WiFiClientSecure.h> // secure SSL connections
#include <PubSubClient.h> // MQTT 
#include <ArduinoJson.h> // JSON serialization
#include <time.h> // time functions
#include "secrets.h"
#include <DHT.h>

#define DHTPIN 2        // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 11

DHT dht(DHTPIN, DHTTYPE);

float h; // Relative Humidity
float t; // Dry Bulb Temperature
float wbt; // Wet Bulb Temperature
float cloudHeight; // Calculated Cloud Height
unsigned long lastMillis = 0;
const long interval = 5000;

// MQTT topics
#define AWS_IOT_PUBLISH_TOPIC   "CLOUD_HEIGHT/pub"

WiFiClientSecure net;
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
PubSubClient client(net);

time_t now;
struct tm timeinfo;

void NTPConnect() {
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < 1510592825) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");

  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

void connectAWS() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  NTPConnect();

  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);

  client.setServer(MQTT_HOST, 8883);

  Serial.println("Connecting to AWS IoT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(1000);
  }

  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }

  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("AWS IoT Connected!");
}

float calculateWetBulbTemperature(float dryBulbTemp, float relativeHumidity) {
  // Empirical formula for wet bulb temperature
  return dryBulbTemp * atan(0.151977 * sqrt(relativeHumidity + 8.313659)) +
         atan(dryBulbTemp + relativeHumidity) - atan(relativeHumidity - 1.676331) +
         0.00391838 * pow(relativeHumidity, 1.5) * atan(0.023101 * relativeHumidity) - 4.686035;
}

float calculateCloudHeight(float dryBulbTemp, float wetBulbTemp) {
  return (dryBulbTemp - wetBulbTemp) * 400;
}

void publishMessage() {
  StaticJsonDocument<200> doc;
  doc["TimeStamp"] = asctime(&timeinfo);
  doc["DryBulbTemp"] = t;
  doc["WetBulbTemp"] = wbt;
  doc["Humidity"] = h;
  doc["CloudHeight"] = cloudHeight;

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void setup() {
  Serial.begin(115200);
  connectAWS();
  dht.begin();
}

void loop() {
  h = dht.readHumidity();
  t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  wbt = calculateWetBulbTemperature(t, h);
  cloudHeight = calculateCloudHeight(t, wbt);

  Serial.print("Dry Bulb Temp: ");
  Serial.print(t);
  Serial.print("°C, Wet Bulb Temp: ");
  Serial.print(wbt);
  Serial.print("°C, Cloud Height: ");
  Serial.print(cloudHeight);
  Serial.println(" meters");

  if (!client.connected()) {
    connectAWS();
  } else {
    client.loop();
    if (millis() - lastMillis > interval) {
      lastMillis = millis();
      publishMessage();
    }
  }
}
