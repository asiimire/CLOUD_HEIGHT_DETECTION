#include <DHT.h>

#define DHTPIN 2     // Pin where the DHT sensor is connected
#define DHTTYPE DHT11 // or DHT22, depending on your sensor

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);
  dht.begin();
}

void loop() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" Degrees Celsius");
  }
  delay(2000); // Wait a few seconds between measurements.
}
