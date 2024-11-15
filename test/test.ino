#include "DHT.h"
#define DHTPIN 13
#define REDPIN 10
#define GRNPIN 12
#define YELPIN 11
#define RLYPIN 7
#define DHTTYPE DHT11   

DHT dht(DHTPIN, DHTTYPE);

void setup() 
{
  Serial.begin(9600);
  delay(5000);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(REDPIN, OUTPUT);
  pinMode(YELPIN, OUTPUT);
  pinMode(GRNPIN, OUTPUT);
  pinMode(RLYPIN, OUTPUT);
  dht.begin();
}

void loop() 
{
  digitalWrite(LED_BUILTIN, HIGH);   
  digitalWrite(REDPIN, HIGH);   
  digitalWrite(YELPIN, HIGH);   
  digitalWrite(GRNPIN, HIGH);   
//  digitalWrite(RLYPIN, HIGH); 
  float h = dht.readHumidity();
  float t = dht.readTemperature(); 
  if (isnan(h) || isnan(t)) 
  {
    Serial.println(F("Failed to read from DHT sensor!"));
  }
  else
  {
    Serial.print(F("Humidity: "));
    Serial.print(h);
    Serial.print(F("%  Temperature: "));
    Serial.print(t);
    Serial.println(F("°C "));

  }
 
  delay(5000);                       
  digitalWrite(LED_BUILTIN, LOW);    
  digitalWrite(REDPIN, LOW);    
  digitalWrite(YELPIN, LOW);    
  digitalWrite(GRNPIN, LOW);    
//  digitalWrite(RLYPIN, LOW);    
  delay(5000);                       
}
