
boolean printDiagnostics0 = true;
boolean printDiagnostics1 = true;
#include <WiFi.h>
#include <PubSubClient.h>
#include "CRC16.h"
#include <SPI.h>
#include <LoRa.h>
//#include "LittleFS.h"

#define CHSPIN 17          // LoRa radio chip select
#define RSTPIN 14          // LoRa radio reset
#define IRQPIN 15          // LoRa radio IRQ
#define COMPIN LED_BUILTIN
#define MQTT_RETRY            3000
#define MAX_NO_MQTT_ERRORS    5
#define MAX_NO_CONNECTION_ATTEMPTS 5
#define MQTT_KEEP_ALIVE    8
#define MQTT_SOCKETTIMEOUT 4
#define MQTT_LED_FLASH_MS 10
#define SIZE_OF_LORA_NODE 28
#define SIZE_OF_LORA_DATA 16


WiFiClient    g_wifiClient;
PubSubClient  g_mqttClient(g_wifiClient);

#include "creds.h"
String        g_mqttClientId = "";
unsigned long g_mqttLastTry = 0;
int           g_noMqttErrors = 0; 
boolean       g_commLEDState = false;
boolean       g_publishNow = false;
unsigned long g_lastMsgTime = 0;
unsigned long lastPublishTime;
unsigned long publishInterval = 4000;

CRC16   crc;
const long loraFreq = 868E6;  // LoRa Frequency
int16_t igatewayAddr = 10;

struct LoraDataHeader
{
  uint16_t icrc;
  int16_t istate;
  int16_t inodeAddr;
  int16_t igatewayAddr;
  int16_t iwatchdog;
  int16_t inewData;  
}; 

union LoraNode
{
  struct
  {
    LoraDataHeader header;
    int16_t data[SIZE_OF_LORA_DATA];
  };
  uint8_t buffer[SIZE_OF_LORA_NODE];
};
uint8_t sizeOfLoraNode = SIZE_OF_LORA_NODE;
uint8_t g_mqttDatabuffer[SIZE_OF_LORA_NODE];
uint8_t loraDatabuffer[SIZE_OF_LORA_NODE];

void mqttCubeCallback(char* topic, byte* payload, unsigned int length) 
{
  // handle message arrived
}
void setCommLED(boolean ledState)
{
  g_commLEDState = ledState;
  digitalWrite(COMPIN, g_commLEDState);
}
void blinkCommLED(int nblink, int imilli)
{
    setCommLED(false);
    for (int ii = 0; ii < nblink; ++ii) 
    {
      setCommLED(!g_commLEDState);
      delay(imilli);
    }
    setCommLED(false);
}
void rebootPico(String warning)
{
    if (printDiagnostics1) Serial.println(warning);
    blinkCommLED(100, 100);
    rp2040.reboot();
}

boolean mqttConnect() 
{
  if ( WiFi.status() != WL_CONNECTED)
  {
    rebootPico("Unable to connect to WIFI network, rebooting in 10 seconds...");
  }
  unsigned long now = millis();
  boolean connected = g_mqttClient.connected();
  if (connected) return true;
  if ((now - g_mqttLastTry) < MQTT_RETRY) return false;

  if (printDiagnostics1) Serial.print("Attempting MQTT connection using ID...");
  if (printDiagnostics1)  Serial.println(g_mqttClientId);
  connected = g_mqttClient.connect(g_mqttClientId.c_str(),g_mqttUsername.c_str(), g_mqttPassword.c_str());
  g_mqttLastTry = now;
  if (connected) 
  {
    if (printDiagnostics1) Serial.println("...connected");
    g_mqttClient.subscribe(g_mqttSubscribeTopic.c_str());
    g_noMqttErrors = 0;
    return true;
  } 
  int mqttState = g_mqttClient.state();
  if (printDiagnostics1) Serial.print(" failed, rc=");
  if (printDiagnostics1) Serial.print(mqttState);
  if (printDiagnostics1) Serial.print(": ");

  switch (mqttState) 
  {
    case -4:
      if (printDiagnostics1) Serial.println("MQTT_CONNECTION_TIMEOUT");
      break;
    case -3:
      if (printDiagnostics1) Serial.println("MQTT_CONNECTION_LOST");
      break;
    case -2:
      g_noMqttErrors = g_noMqttErrors + 1;
      if (printDiagnostics1) Serial.print("Number of MQTT connection attempts: ");
      if (printDiagnostics1) Serial.print(g_noMqttErrors);
      if (g_noMqttErrors > MAX_NO_MQTT_ERRORS) rebootPico("Too may MQTT reconnect attempts. Rebooting...");
      break;
    case -1:
      if (printDiagnostics1) Serial.println("MQTT_DISCONNECTED");
      break;
    case 0:
      if (printDiagnostics1) Serial.println("MQTT_CONNECTED");
      break;
    case 1:
      if (printDiagnostics1) Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
      break;
    case 2:
      if (printDiagnostics1) Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
      break;
    case 3:
      if (printDiagnostics1) Serial.println("MQTT_CONNECT_UNAVAILABLE");
      break;
    case 4:
      if (printDiagnostics1) Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
      break;
    case 5:
      if (printDiagnostics1) Serial.println("MQTT_CONNECT_UNAUTHORIZED");
      break;
    default:
      break;
  }
  return false;
}
void LoRa_rxMode()
{
  LoRa.disableInvertIQ();               // normal mode
  LoRa.receive();                       // set receive mode
}

void LoRa_txMode()
{
  LoRa.idle();                          // set standby mode
  LoRa.enableInvertIQ();                // active invert I and Q signals
}

void LoRa_sendMessage(String message) 
{
  LoRa_txMode();                        // set tx mode
  LoRa.beginPacket();                   // start packet
  LoRa.print(message);                  // add payload
  LoRa.endPacket(true);                 // finish packet and send it
}

void onReceive(int packetSize) 
{
  uint8_t numBytes = 0;
  LoraNode loraNode;
  if (printDiagnostics0) Serial.print("Received LoRa data at: ");
  if (printDiagnostics0) Serial.println(millis());
  while (LoRa.available() )
  {
    numBytes = LoRa.readBytes(loraNode.buffer, sizeOfLoraNode);
  }
  if (numBytes != sizeOfLoraNode)
  {
    if (printDiagnostics0) Serial.println("LoRa bytes do not match");
    return;
  }
  
  crc.restart();
  for (int ii = 2; ii <sizeOfLoraNode; ii++)
  {
    crc.add(loraNode.buffer[ii]);
  }
  uint16_t crcCalc = crc.calc();
  if (crcCalc != loraNode.header.icrc) 
  {
    if (printDiagnostics0) Serial.println("LoRa CRC does not match");
    return;
  }

  if (loraNode.header.igatewayAddr != igatewayAddr) 
  {
    if (printDiagnostics0) Serial.println("LoRa Gateway address do not match");
    return;
  }
  
  if (printDiagnostics0)
  {
    Serial.print("Gateway Receive: ");
    Serial.println(numBytes);
    Serial.print("icrc           : ");
    Serial.println(loraNode.header.icrc);
  }
  if (!g_publishNow)
  {
    if (printDiagnostics0) Serial.println("Sending data to MQTT");
    for (int ii = 0; ii < SIZE_OF_LORA_NODE; ++ii) loraDatabuffer[ii] = loraNode.buffer[ii];
    g_publishNow = true;
  }
  else
  {
    if (printDiagnostics0) Serial.println("MQTT Core busy");
    
  }
  if (printDiagnostics0) Serial.println("");
}

void onTxDone() 
{
  if (printDiagnostics0) Serial.println("TxDone");
  LoRa_rxMode();
}
void setupLoRa()
{
  LoRa.setPins(CHSPIN, RSTPIN, IRQPIN);

  if (!LoRa.begin(loraFreq)) 
  {
    if (printDiagnostics0) Serial.println("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }

  if (printDiagnostics0)
  {
    Serial.println("LoRa init succeeded.");
    Serial.println();
    Serial.println("LoRa Simple Gateway");
    Serial.println("Only receive messages from nodes");
    Serial.println("Tx: invertIQ enable");
    Serial.println("Rx: invertIQ disable");
    Serial.println();
  }

  LoRa.onReceive(onReceive);
  LoRa.onTxDone(onTxDone);
  LoRa_rxMode();

}
void setup()
{
  g_publishNow = true;
  pinMode(COMPIN, OUTPUT);
  setCommLED(false);
  if (printDiagnostics1) Serial.begin(115200);
  blinkCommLED(20, 500);
  
  if (printDiagnostics1) Serial.println("Reading creds.txt file");

/*
  LittleFS.begin();
  File file = LittleFS.open("/creds.txt", "r");
  if (file) 
  {
      String lines[8];
      String data = file.readString();
      int startPos = 0;
      int stopPos = 0;
      for (int ii = 0; ii < 9; ++ii)
      {
        startPos = data.indexOf("{") + 1;
        stopPos = data.indexOf("}");
        lines[ii] = data.substring(startPos,stopPos);
        if (printDiagnostics1) Serial.println(lines[ii]);
        data = data.substring(stopPos + 1);
      }
      g_ssid               = lines[0];
      g_wifiPassword       = lines[1];
      g_mqttServer         = lines[2];
      g_mqttUsername       = lines[3];
      g_mqttPassword       = lines[4];
      g_box                = lines[5];
      g_trayType           = lines[6];
      g_trayName           = lines[7];
      g_cubeType           = lines[8];
      file.close();
  }
  else
  {
    rebootPico("Unable to read config file, rebooting in 10 seconds...");
  }
  LittleFS.end();
*/
  if (printDiagnostics1)
  {
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(g_ssid);
  
    Serial.print("SSID: ");
    Serial.println(g_ssid);
    Serial.print("Wifi Password: ");
    Serial.println(g_wifiPassword);
  }
  WiFi.mode(WIFI_STA);

  int itry = 0;
  while ((WiFi.status() != WL_CONNECTED) && (itry < 5))
  {
    setCommLED(!g_commLEDState);
    if (printDiagnostics1) Serial.print("Attempt: ");
    if (printDiagnostics1) Serial.println(itry);
    WiFi.begin(g_ssid.c_str(), g_wifiPassword.c_str()); 
    delay(10000);
    itry = itry + 1;
  }
  setCommLED(false);

  if (itry == 5) rebootPico("Unable to connect to WIFI network, rebooting in 10 seconds...");

  if (printDiagnostics1) Serial.println("");
  if (printDiagnostics1) Serial.println("WiFi connected");
  if (printDiagnostics1) Serial.println("IP address: ");
  if (printDiagnostics1) Serial.println(WiFi.localIP());
  if (printDiagnostics1)
  {
    byte mac[6];
    WiFi.macAddress(mac);
    Serial.print("MAC: ");
    Serial.print(mac[0],HEX);
    Serial.print(":");
    Serial.print(mac[1],HEX);
    Serial.print(":");
    Serial.print(mac[2],HEX);
    Serial.print(":");
    Serial.print(mac[3],HEX);
    Serial.print(":");
    Serial.print(mac[4],HEX);
    Serial.print(":");
    Serial.println(mac[5],HEX);
  }

  delay(2000);
  blinkCommLED(50, 40);
  
  g_mqttClient.setServer(g_mqttServer.c_str(), 1883);
  g_mqttClient.setCallback(mqttCubeCallback);
  g_mqttClientId = g_box + "_" + g_trayType + "_" + g_trayName + "_" + g_cubeType;
  g_mqttSubscribeTopic = g_box + "/" + g_cubeType + "/" + g_trayType + "/" + g_trayName + "/setting";
  g_mqttPublishTopic   = g_box + "/" + g_cubeType + "/" + g_trayType + "/" + g_trayName + "/reading";
  g_mqttClient.setKeepAlive(MQTT_KEEP_ALIVE);
  g_mqttClient.setSocketTimeout(MQTT_SOCKETTIMEOUT);
  g_publishNow = false;
  setupLoRa();  
}

void loop()
{
  unsigned long now = millis();
  g_mqttClient.loop();
  mqttConnect();
  if (g_publishNow)
  {
    setCommLED(true);
    g_lastMsgTime = now;
    for (int ii = 0; ii < SIZE_OF_LORA_NODE; ++ii) g_mqttDatabuffer[ii] = loraDatabuffer[ii];
    g_mqttClient.publish(g_mqttPublishTopic.c_str(), g_mqttDatabuffer, SIZE_OF_LORA_NODE);
    g_publishNow = false;
  }
  if (g_commLEDState)
  {
    if ((now - g_lastMsgTime) > MQTT_LED_FLASH_MS) setCommLED(false);  
  }

}
