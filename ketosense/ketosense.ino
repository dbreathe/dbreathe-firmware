// dBreathe Arduino firmware for a NodeMCU dev board.
// By Shervin Emami (www.shervinemami.info)
// Based on the Ketose detector by Jens Clarholm (www.jenslabs.com)

#include <Arduino.h>

// WebSockets server for Arduino ("https://github.com/Links2004/arduinoWebSockets")
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <Hash.h>

ESP8266WiFiMulti WiFiMulti;
WebSocketsServer webSocket = WebSocketsServer(81);
#define USE_SERIAL Serial


// TGS882 sensor
const int gasValuePin = A0;

const int ledPin = 2;


// DHT22 Humidity & temperature sensor
#include <math.h>
#include "DHT.h"
const int humidityPin = 4;
// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE   DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(humidityPin, DHTTYPE);


int currentMode=1;

// Read gas variables
int tempRead1 = 0;
int tempRead2 = 0;
int tempRead3 = 0;

const float R0 = 4500;

double currentHumidity;
double currentTemperature;
double scalingFactor;

int measurement = 0;


// Milli-seconds between WebSockets update
const int WEB_TICK_MS = 10;
// Milli-seconds between sensor updates
const int UPDATE_TICKS = 1000 / WEB_TICK_MS;

int updateCountDown = UPDATE_TICKS;


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

    switch(type) {
        case WStype_DISCONNECTED:
            USE_SERIAL.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        
        // send message to client
        webSocket.sendTXT(num, "Connected");
            }
            break;
        case WStype_TEXT:
            USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);

            // send message to client
            // webSocket.sendTXT(num, "message here");

            // send data to all connected clients
            // webSocket.broadcastTXT("message here");
            break;
        case WStype_BIN:
            USE_SERIAL.printf("[%u] get binary lenght: %u\n", num, lenght);
            hexdump(payload, lenght);

            // send message to client
            // webSocket.sendBIN(num, payload, lenght);
            break;
    }

}


// Temperature sensor function, with humidity as a percentage and temperature as degrees Celcius.
int tempHumidityCompensation(int value, float currentHumidity, float currentTemperature)
{
    //currentHumidity = 60;
    //currentTemperature = 28;
    //function derrived from regression analysis of the graph in the datasheet
    scalingFactor = (((currentTemperature * -0.02573)+1.898)+((currentHumidity*-0.011)+0.3966));
    double scaledValue = value * scalingFactor;
    return (int)scaledValue; 
}


// Smoothen a 1D signal so it only changes gradually.
int runningAverage(int previous, int input)
{
  float alpha = 0.9f;
  return (int)((1.0f - alpha) * input + alpha * previous);
}


// Convert the 1-1023 value from analog read to a voltage.
float toVoltage(int reading){
  //constant derived from 5/1023 = 0.0048875
  float voltageConversionConstant = 0.0048875;
  float voltageRead = reading * voltageConversionConstant;
  return voltageRead;
}


// Convert the 1-1023 voltage value from gas sensor analog read to a resistance, 9800 is the value of the other resistor in the voltage divide.
float toResistance(int reading){
  float resistance = ((5/toVoltage(reading) - 1) * 9800);
  return resistance;
}


// Calculate the gas concentration relative to the resistance
float acetoneResistanceToPPMf(float resistance){
  double tempResistance = (double)resistance;
  double PPM; 
  // Original threshold used by Jens was 50,000 ohm, but he had a bug in the next line anyway, so it's probably not an important threshold.
  //if (tempResistance > 50000) {
    // BUG: In original code by Jens, he accidentally declared a new variable "PPM" instead of using the variable "PPM" of this function,
    // so the next statement below was being ignored!
    //double PPM = 0;
  //}
  //else {
    double logPPM = (log10(tempResistance/R0)*-2.6)+2.7;
    PPM = pow(10, logPPM);
  //}
  return (float)PPM;
}


float ppmToMmol(float PPMf)
{
  float ppmInmmol = ((PPMf / 1000.0f) / 58.08f);
  ppmInmmol = ppmInmmol * 1000.0f;
  return ppmInmmol;
}


// Read three times from gas sensor with 5ms between each reading
void readsensor() {
  tempRead1 = analogRead(0);
  delay(5);
  tempRead2 = analogRead(0);
  delay(5);
  tempRead3 = analogRead(0);
}



// Special Arduino initialization callback function
void setup() {
  Serial.begin(9600);      // open the serial port at 9600 bps:    

  pinMode(ledPin,OUTPUT);
    
  pinMode(gasValuePin,INPUT);

  // Initialize the DHT humidity sensor
  dht.begin();

/*  
  //Warmup, check that sensor is stabile.
  while (checkIfSensorIsStabile() == false){
    checkIfSensorIsStabile();
  }
*/

  // Initialize the WebSockets server
  // Serial.setDebugOutput(true);
  USE_SERIAL.setDebugOutput(true);
  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();  
  for (uint8_t t = 4; t > 0; t--) {
      USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
      USE_SERIAL.flush();
      delay(1000);
  }
  WiFiMulti.addAP("breath", "breath##");
  while(WiFiMulti.run() != WL_CONNECTED) {
      delay(100);
  }
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
    
  Serial.println("Waiting for sensors to stabilize");

  // Just wait 30 seconds
  delay(30000);
  

  // Read three times from gas sensor with 5ms between each reading
  readsensor();

  // Initialize the measurement
  measurement = (tempRead1 + tempRead2 + tempRead3)/3;
  
  Serial.println("Warmup finished");
  delay(1000);

  Serial.println("Blow into mouthpiece to start. Results are shown in mmol/l ...");

}


// Special Arduino main loop callback function
void loop()
{
  // Keep the WebSockets server socket alive atleast 100 times per second!
  webSocket.loop();

  // Sleep about 10ms
  delay(WEB_TICK_MS);

  updateCountDown--;

  // Only do a sensor update roughly once per second
  if (updateCountDown <= 0) {
    updateCountDown = UPDATE_TICKS;
    digitalWrite(ledPin, HIGH);
      
    //read three times from gas sensor with 5ms between each reading
    readsensor();
    measurement = runningAverage(measurement, tempRead1);
    measurement = runningAverage(measurement, tempRead2);
    measurement = runningAverage(measurement, tempRead3);
  
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float humidity = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float temperature = dht.readTemperature();
    
    int compensated = tempHumidityCompensation(measurement, humidity, temperature);
    float resistance = toResistance(compensated);
    float ppmf = acetoneResistanceToPPMf(resistance);
    float mmol = ppmToMmol(ppmf);
  
    // Publish our results through Wifi!
    char str[512];
    snprintf(str, sizeof(str), "%d,%d,%d,%d,%d", (int)(mmol*1000.0f), (int)(ppmf*1000.0f), (int)(measurement*1.0f), (int)(humidity*1000.0f), (int)(temperature*1000.0f));
    webSocket.broadcastTXT(str);
    
    digitalWrite(ledPin, LOW);
  
    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    else {
      Serial.print("Humidity: ");
      Serial.print((int)humidity);
      Serial.print("%  ");
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.print(" *C ");
    }
    Serial.print("      ");
  
    Serial.print("Gas (V): ");
    Serial.print(measurement);
    Serial.print("   ");
    //Serial.print(resistance);
    //Serial.print(" ");
    Serial.print("PPMF: ");
    Serial.print(ppmf);
    Serial.print("   ");
    Serial.print("MMOL: ");
    Serial.print(mmol);
  
    Serial.println();  
  }
  
    
}


