bool core1_separate_stack = true;
#define BLINKY_DIAG         false
#define NODE_DIAG         false
#define NODEADDRESS 11
#define GATEWAYADDRESS 10
#define DHTPIN 13
#define REDPIN 10
#define GRNPIN 12
#define YELPIN 11
#define RLYPIN 7
#define DHTTYPE DHT11   
#define CHSPIN 17          // LoRa radio chip select
#define RSTPIN 14       // LoRa radio reset
#define IRQPIN 15         // LoRa radio IRQ
#define LORSBW 62e3
#define LORSPF 9
#define LORFRQ 868E6

#include <BlinkyLoraNode.h>
#include <DHT.h>
DHT     dht(DHTPIN, DHTTYPE);

unsigned long lastPublishTime;
unsigned long publishInterval = 2000;
unsigned long lastRelayTime;
unsigned long relayInterval = 20000;
unsigned long lastGreenTime;
unsigned long greenInterval = 200;
boolean greenLed = false;
unsigned long yellowInterval = 1000;
unsigned long lastYellowTime = 200;
boolean yellowLed = false;
boolean redLed = false;

struct NodeData
{
  int16_t imode;           //  0=OFF, 1=ON, 2=AUTO;
  int16_t ipubInterval;    //  0.10 sec
  int16_t itemp;           //  0.01 degC
  int16_t ihumid;          //  0.01 %
  int16_t irelay;          //  0=OFF, 1=ON
  int16_t isetTemp;        //  0.01 degC
  int16_t iwindowTemp;     //  0.01 degC
  int16_t irelayInterval;  //  0.10 sec
}; 
NodeData nodeData;
NodeData nodeReceivedData;
    
void setupLora()
{
  if (BLINKY_DIAG > 0)
  {
     Serial.begin(9600);
     delay(10000);
  }
  BlinkyLoraNode.begin(sizeof(nodeData), BLINKY_DIAG, NODEADDRESS, GATEWAYADDRESS, CHSPIN, RSTPIN, IRQPIN, LORFRQ, LORSPF, LORSBW);
}

void setupNode() 
{
  if (NODE_DIAG > 0)
  {
     Serial.begin(9600);
     delay(10000);
  }
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(REDPIN, OUTPUT);
  pinMode(YELPIN, OUTPUT);
  pinMode(GRNPIN, OUTPUT);
  pinMode(RLYPIN, OUTPUT);
  for (int ii = 0; ii < 25; ++ii)
  {
    digitalWrite(LED_BUILTIN, HIGH);   
    digitalWrite(REDPIN, HIGH);   
    digitalWrite(YELPIN, HIGH);   
    digitalWrite(GRNPIN, HIGH);   
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);   
    digitalWrite(REDPIN, LOW);   
    digitalWrite(YELPIN, LOW);   
    digitalWrite(GRNPIN, LOW);   
    delay(100);
  }
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);   
  digitalWrite(REDPIN, LOW);   
  digitalWrite(YELPIN, false);   
  digitalWrite(GRNPIN, false);   
  digitalWrite(RLYPIN, LOW); 
  dht.begin();

  nodeData.imode = 0;
  nodeData.ipubInterval = 100;
  nodeData.itemp = 0;
  nodeData.ihumid = 0;
  nodeData.irelay = 0;
  nodeData.isetTemp = 1000;
  nodeData.iwindowTemp = 200;
  nodeData.irelayInterval = 600;
  publishInterval = nodeData.ipubInterval * 100;
  lastPublishTime = millis();
  relayInterval = nodeData.irelayInterval * 100;
  lastRelayTime = lastPublishTime;
  lastGreenTime = lastPublishTime;
}

void loopNode() 
{

  unsigned long nowTime = millis();
  if (greenLed)
  {
    if ((nowTime - lastGreenTime) > greenInterval)
    {
      greenLed = false;
      digitalWrite(GRNPIN, greenLed);
    }
  }
  if (yellowLed)
  {
    if ((nowTime - lastYellowTime) > yellowInterval)
    {
      yellowLed = false;
      digitalWrite(YELPIN, yellowLed);
    }
  }
  if ((nowTime - lastPublishTime) > publishInterval)
  {
    lastPublishTime = nowTime;
    float humid = dht.readHumidity();
    float temp = dht.readTemperature(); 
    if (isnan(humid) || isnan(temp)) 
    {
      if (NODE_DIAG) Serial.println(F("Failed to read from DHT sensor!"));
      humid = 0;
      temp = -20;
    }
    else
    {
      if (NODE_DIAG) 
      {
        Serial.print(F("Humidity: "));
        Serial.print(humid);
        Serial.print(F("%  Temperature: "));
        Serial.print(temp);
        Serial.println(F("°C "));
      }
    }
    humid = humid * 100;
    temp = temp * 100;
    nodeData.ihumid = (int16_t) humid;
    nodeData.itemp  = (int16_t) temp;
    if (nodeData.imode == 0)
    {
      digitalWrite(RLYPIN, LOW);
      digitalWrite(REDPIN, LOW);
      nodeData.irelay = 0;
    }
    if (nodeData.imode == 1) 
    {
      digitalWrite(RLYPIN, HIGH);
      digitalWrite(REDPIN, HIGH);
      nodeData.irelay = 1;
    }
    if (nodeData.imode == 2)
    {
      int16_t irelay = 0;
      if (((nowTime - lastRelayTime) > relayInterval) && (humid > 0))
      {
        if (nodeData.itemp > (nodeData.isetTemp + nodeData.iwindowTemp)) irelay = 0;
        if (nodeData.itemp < (nodeData.isetTemp - nodeData.iwindowTemp)) irelay = 1;
        if (irelay != nodeData.irelay)
        {
          nodeData.irelay = irelay;
          if (nodeData.irelay == 0)
          {
            digitalWrite(RLYPIN, LOW);
            digitalWrite(REDPIN, LOW);
          }
          if (nodeData.irelay == 1)
          {
            digitalWrite(RLYPIN, HIGH);
            digitalWrite(REDPIN, HIGH);
          }
          lastRelayTime = nowTime;
        }
      }
    }
    boolean successful = BlinkyLoraNode.publishNodeData((uint8_t*) &nodeData, false);
    greenLed = true;
    digitalWrite(GRNPIN, greenLed);
    lastGreenTime = nowTime;
    lastYellowTime = nowTime;
  }
  if (BlinkyLoraNode.retrieveGatewayData((uint8_t*) &nodeReceivedData) )
  {
    nodeData.imode            = nodeReceivedData.imode;
    nodeData.ipubInterval    = nodeReceivedData.ipubInterval;
    nodeData.isetTemp        = nodeReceivedData.isetTemp;
    nodeData.iwindowTemp     = nodeReceivedData.iwindowTemp;
    nodeData.irelayInterval  = nodeReceivedData.irelayInterval;
      
    if (NODE_DIAG) 
    {
      Serial.println(" ");
      Serial.println("New Setting received");
      Serial.print("imode: ");
      Serial.println(nodeData.imode);
      Serial.print("ipubInterval: ");
      Serial.println(nodeData.ipubInterval);
      Serial.print("isetTemp: ");
      Serial.println(nodeData.isetTemp);
      Serial.print("iwindowTemp: ");
      Serial.println(nodeData.iwindowTemp);
      Serial.print("irelayInterval: ");
      Serial.println(nodeData.irelayInterval);
    }

    publishInterval = nodeData.ipubInterval * 100;
    relayInterval = nodeData.irelayInterval * 100;
    lastPublishTime = 1;
    
    yellowLed = true;
    digitalWrite(YELPIN, yellowLed);
    lastYellowTime = millis();
    
  }

}
