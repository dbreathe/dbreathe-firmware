// dBreathe Arduino firmware for a NodeMCU dev board.
// By Shervin Emami (www.shervinemami.info)
// Based on the Ketose detector by Jens Clarholm (www.jenslabs.com)

#include <math.h>
#include "DHT.h"

// TGS882 sensor
const int gasValuePin = A0;

const int ledPin = 2;

// DHT22 Humidity & temperature sensor
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

// General Var
float R0 = 4500;

double currentHumidity;
double currentTemperature;
double scalingFactor;

int measurement = 0;



// Temperature sensor function, values has been hardcoded to humidity = 60 and temperature = 28 to speed up the measuring.
int tempHumidityCompensation(int value){
    //int chk = DHT11.read(DHT11PIN);
    //delay(300);
    //currentHumidity = ((double)DHT11.humidity);
    //Hardcoded after realizing that the temperature and humidity were beahaving stabilly.
    currentHumidity = 60;
    //currentTemperature = ((double)DHT11.temperature);
    currentTemperature = 28;
    //function derrived from regression analysis of the graph in the datasheet
    scalingFactor = (((currentTemperature * -0.02573)+1.898)+((currentHumidity*-0.011)+0.3966));
    //debug
    //clearLcd();
    //printToRow1("Scalefactor:");
    //printFloatToCurrentCursorPossition((float)scalingFactor);
    //delay(1000);
    //debugstop*/
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

  Serial.println("Waiting for sensors to stabilize");
/*  
  //Warmup, check that sensor is stabile.
  while (checkIfSensorIsStabile() == false){
    checkIfSensorIsStabile();
  }
*/
  // Just wait 30 seconds
  delay(30000);

  //read three times from gas sensor with 5ms between each reading
  readsensor();

  // Initialize the measurement
  measurement = (tempRead1 + tempRead2 + tempRead3)/3;
  
  Serial.println("Warmup finished");
  delay(1000);

  Serial.println("Blow into mouthpiece to start. Results are shown in mmol/l ...");

}


// Special Arduino main loop callback function
void loop() {
  digitalWrite(ledPin, HIGH);
  
  //read three times from gas sensor with 5ms between each reading
  readsensor();
  measurement = runningAverage(measurement, tempRead1);
  measurement = runningAverage(measurement, tempRead2);
  measurement = runningAverage(measurement, tempRead3);
  
  int compensated = tempHumidityCompensation(measurement);
  float resistance = toResistance(compensated);
  float ppmf = acetoneResistanceToPPMf(resistance);
  
  digitalWrite(ledPin, LOW);

  Serial.print("Measurement: ");
  Serial.print(measurement);
  Serial.print("   ");
  //Serial.print(resistance);
  //Serial.print(" ");
  Serial.print("PPMF: ");
  Serial.print(ppmf);
  Serial.print("          ");

  delay(1000);
  
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  Serial.print("Humidity: ");
  Serial.print((int)h);
  Serial.print("%  ");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.println();
    
}


