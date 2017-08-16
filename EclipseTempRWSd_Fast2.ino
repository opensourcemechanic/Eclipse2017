/*
 * Copyright (c) 2015, Majenko Technologies
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 * 
 * * Neither the name of Majenko Technologies nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Create a WiFi access point and provide a web server on it. */

#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <SPI.h>
#include <SD.h>

#define DHTTYPE DHT11   // DHT Shield uses DHT 11
#define DHTPIN D4       // DHT Shield uses pin D4

/* Set these to your desired credentials. */
const char *ssid = "EclipseTemp";
const char *password = "totalsolar";
const char *log_file = "ecltmp.txt";
const int relayPin = D1;
const int chipSelect = 4;  //d8
 

ESP8266WebServer server(80);

DHT dht(DHTPIN, DHTTYPE);

float humidity, temperature;                 // Raw float values from the sensor
char str_humidity[10], str_temperature[10];  // Rounded sensor values and as strings
char response[50];
char buffer[1024];
int log_interval = 0;
File dataFile;

// Generally, you should use "unsigned long" for variables that hold time
unsigned long previousMillis = 0;            // When the sensor was last read
const long interval = 2000;                  // Wait this long until reading again

void handle_root() {
  server.send(200, "text/plain", "WeMos DHT Server. Get /temp or /humidity");
  delay(100);
}

void read_sensor() {
  // Wait at least 2 seconds seconds between measurements.
  // If the difference between the current time and last time you read
  // the sensor is bigger than the interval you set, read the sensor.
  // Works better than delay for things happening elsewhere also.
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // Save the last time you read the sensor
    previousMillis = currentMillis;

    // Reading temperature and humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
    humidity = dht.readHumidity();        // Read humidity as a percent
    temperature = dht.readTemperature();  // Read temperature as Celsius

    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    // Convert the floats to strings and round to 2 decimal places
    dtostrf(humidity, 1, 2, str_humidity);
    dtostrf(temperature, 1, 2, str_temperature);
    //Serial.print("Humidity: ");
    //Serial.print(str_humidity);
    //Serial.print(" %\t");
    //Serial.print("Temperature: ");
    //Serial.print(str_temperature);
    //Serial.println(" °C");
  }
}

void setup() {
  pinMode(relayPin, OUTPUT);
	delay(1000);
	Serial.begin(9600);
   while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");
	Serial.println();
  Serial.println("Creating weather data file.");
  Serial.println();

	//Serial.print("Configuring access point...");
	/* You can remove the password parameter if you want the AP to be open. */
  delay(1000);
	WiFi.softAP(ssid, password);

	IPAddress myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);

  // Initial read
  read_sensor();

  // Handle http requests
  server.on("/", handle_root);

  server.on("/temp", [](){
    read_sensor();
    snprintf(response, 50, "Temperature: %s C", str_temperature);
    server.send(200, "text/plain", response);
  });

  server.on("/humidity", [](){
    read_sensor();
    snprintf(response, 50, "Humidity: %s %", str_humidity);
    server.send(200, "text/plain", response);
  });

  server.on("/on", [](){
    snprintf(response, 50, "Relay On");
    digitalWrite(relayPin, HIGH); // turn on relay with voltage HIGH
    server.send(200, "text/plain", response);
  });

    server.on("/off", [](){
    snprintf(response, 50, "Relay Off");
    digitalWrite(relayPin, LOW); // turn on relay with voltage Low
    server.send(200, "text/plain", response);
  });

    server.on("/rm", [](){
    snprintf(response, 50, "Removing ecltmp.txt");
    SD.remove("ecltmp.txt");
    server.send(200, "Removing ecltmp.txt", response);
  });

    server.on("/log", [](){
      dataFile = SD.open("ecltmp.txt");
      // if the file is available, read from it:
      if (dataFile) {
        while (dataFile.available()) {
              //sprintf(buffer, "%s", dataFile.read());
              //server.send(200, "text/plain",dataFile.read());
              dataFile.read(buffer,1024);
              server.send(1024, "text/plain", buffer);
              // print to the serial port too:
              //Serial.write(dataFile.read());
        }
        dataFile.close();
      }
  });

  // Start the web server
  server.begin();
  Serial.println("HTTP server started");
  log_interval = 0;
}

void loop(void)
{
  // Listen for http requests
  server.handleClient();
  delay(1000);
  read_sensor();
  snprintf(response, 50, "Temperature: %s °C", str_temperature);
  // print to the serial port:
  Serial.print("Temperature:");
  Serial.print(str_temperature);  // so you have to close this one before opening another.
  Serial.println("°C");
  Serial.print("Humidity: ");
  Serial.print(str_humidity);
  Serial.println("%");
  if (log_interval % 10 == 0) {
      dataFile = SD.open("ecltmp.txt",O_CREAT | O_APPEND | O_RDWR );
      // if the file is available, write to it:
      if (dataFile) {
          dataFile.print("Temperature: ");
          dataFile.print(str_temperature);
          dataFile.println("C");
          dataFile.print("Humidity: ");
          dataFile.print(str_humidity);
          dataFile.println("%\t");
          dataFile.close();
       }  // if the file isn't open, pop up an error:
      else {
          Serial.println("error opening ecltmp.txt");
      }
    server.send(200, "text/plain", response);
  }
  log_interval = log_interval + 1;
}
