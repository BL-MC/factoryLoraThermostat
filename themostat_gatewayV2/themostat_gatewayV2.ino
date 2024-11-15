
boolean l_printDiagnostics = true;
boolean m_printDiagnostics = true;
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


WiFiClient    m_wifiClient;
PubSubClient  m_mqttClient(m_wifiClient);

#include "creds.h"
String        m_mqttClientId = "";
unsigned long m_mqttLastTry = 0;
int           m_noMqttErrors = 0; 
boolean       m_commLEDState = false;
boolean       m_publishNow = false;
unsigned long m_lastMsgTime = 0;
uint8_t       m_mqttDatabuffer[SIZE_OF_LORA_NODE];

CRC16   l_crc;
const long l_loraFreq = 868E6;  // LoRa Frequency
int16_t l_igatewayAddr = 10;

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
uint8_t l_sizeOfLoraNode = SIZE_OF_LORA_NODE;
uint8_t l_loraDatabuffer[SIZE_OF_LORA_NODE];

void m_mqttCubeCallback(char* topic, byte* payload, unsigned int length) 
{
  // handle message arrived
}
void m_setCommLED(boolean ledState)
{
  m_commLEDState = ledState;
  digitalWrite(COMPIN, m_commLEDState);
}
void m_blinkCommLED(int nblink, int imilli)
{
    m_setCommLED(false);
    for (int ii = 0; ii < nblink; ++ii) 
    {
      m_setCommLED(!m_commLEDState);
      delay(imilli);
    }
    m_setCommLED(false);
}
void m_rebootPico(String warning)
{
    if (m_printDiagnostics) Serial.println(warning);
    m_blinkCommLED(100, 100);
    rp2040.reboot();
}

boolean m_mqttConnect() 
{
  if ( WiFi.status() != WL_CONNECTED)
  {
    m_rebootPico("Unable to connect to WIFI network, rebooting in 10 seconds...");
  }
  unsigned long now = millis();
  boolean connected = m_mqttClient.connected();
  if (connected) return true;
  if ((now - m_mqttLastTry) < MQTT_RETRY) return false;

  if (m_printDiagnostics) Serial.print("Attempting MQTT connection using ID...");
  if (m_printDiagnostics)  Serial.println(m_mqttClientId);
  connected = m_mqttClient.connect(m_mqttClientId.c_str(),m_mqttUsername.c_str(), m_mqttPassword.c_str());
  m_mqttLastTry = now;
  if (connected) 
  {
    if (m_printDiagnostics) Serial.println("...connected");
    m_mqttClient.subscribe(m_mqttSubscribeTopic.c_str());
    m_noMqttErrors = 0;
    return true;
  } 
  int mqttState = m_mqttClient.state();
  if (m_printDiagnostics) Serial.print(" failed, rc=");
  if (m_printDiagnostics) Serial.print(mqttState);
  if (m_printDiagnostics) Serial.print(": ");

  switch (mqttState) 
  {
    case -4:
      if (m_printDiagnostics) Serial.println("MQTT_CONNECTION_TIMEOUT");
      break;
    case -3:
      if (m_printDiagnostics) Serial.println("MQTT_CONNECTION_LOST");
      break;
    case -2:
      m_noMqttErrors = m_noMqttErrors + 1;
      if (m_printDiagnostics) Serial.print("Number of MQTT connection attempts: ");
      if (m_printDiagnostics) Serial.print(m_noMqttErrors);
      if (m_noMqttErrors > MAX_NO_MQTT_ERRORS) m_rebootPico("Too may MQTT reconnect attempts. Rebooting...");
      break;
    case -1:
      if (m_printDiagnostics) Serial.println("MQTT_DISCONNECTED");
      break;
    case 0:
      if (m_printDiagnostics) Serial.println("MQTT_CONNECTED");
      break;
    case 1:
      if (m_printDiagnostics) Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
      break;
    case 2:
      if (m_printDiagnostics) Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
      break;
    case 3:
      if (m_printDiagnostics) Serial.println("MQTT_CONNECT_UNAVAILABLE");
      break;
    case 4:
      if (m_printDiagnostics) Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
      break;
    case 5:
      if (m_printDiagnostics) Serial.println("MQTT_CONNECT_UNAUTHORIZED");
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
  if (l_printDiagnostics) Serial.print("Received LoRa data at: ");
  if (l_printDiagnostics) Serial.println(millis());
  while (LoRa.available() )
  {
    numBytes = LoRa.readBytes(loraNode.buffer, l_sizeOfLoraNode);
  }
  if (numBytes != l_sizeOfLoraNode)
  {
    if (l_printDiagnostics) Serial.println("LoRa bytes do not match");
    return;
  }
  
  l_crc.restart();
  for (int ii = 2; ii < l_sizeOfLoraNode; ii++)
  {
    l_crc.add(loraNode.buffer[ii]);
  }
  uint16_t crcCalc = l_crc.calc();
  if (crcCalc != loraNode.header.icrc) 
  {
    if (l_printDiagnostics) Serial.println("LoRa CRC does not match");
    return;
  }

  if (loraNode.header.igatewayAddr != l_igatewayAddr) 
  {
    if (l_printDiagnostics) Serial.println("LoRa Gateway address do not match");
    return;
  }
  
  if (l_printDiagnostics)
  {
    Serial.print("Gateway Receive: ");
    Serial.println(numBytes);
    Serial.print("icrc           : ");
    Serial.println(loraNode.header.icrc);
  }
  if (!m_publishNow)
  {
    if (l_printDiagnostics) Serial.println("Sending data to MQTT");
    for (int ii = 0; ii < SIZE_OF_LORA_NODE; ++ii) l_loraDatabuffer[ii] = loraNode.buffer[ii];
    m_publishNow = true;
  }
  else
  {
    if (l_printDiagnostics) Serial.println("MQTT Core busy");
    
  }
  if (l_printDiagnostics) Serial.println("");
}

void onTxDone() 
{
  if (l_printDiagnostics) Serial.println("TxDone");
  LoRa_rxMode();
}
void setupLoRa(int16_t igatewayAddr)
{
  LoRa.setPins(CHSPIN, RSTPIN, IRQPIN);
  l_igatewayAddr = igatewayAddr;

  if (!LoRa.begin(l_loraFreq)) 
  {
    if (l_printDiagnostics) Serial.println("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }

  if (l_printDiagnostics)
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
  m_publishNow = true;
  pinMode(COMPIN, OUTPUT);
  m_setCommLED(false);
  if (m_printDiagnostics) Serial.begin(115200);
  m_blinkCommLED(20, 500);
  
  if (m_printDiagnostics) Serial.println("Reading creds.txt file");

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
        if (m_printDiagnostics) Serial.println(lines[ii]);
        data = data.substring(stopPos + 1);
      }
      m_ssid               = lines[0];
      m_wifiPassword       = lines[1];
     m_mqttServer         = lines[2];
      m_mqttUsername       = lines[3];
      m_mqttPassword       = lines[4];
      m_box                = lines[5];
      m_trayType           = lines[6];
      m_trayName           = lines[7];
      m_cubeType           = lines[8];
      file.close();
  }
  else
  {
    m_rebootPico("Unable to read config file, rebooting in 10 seconds...");
  }
  LittleFS.end();
*/
  if (m_printDiagnostics)
  {
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(m_ssid);
  
    Serial.print("SSID: ");
    Serial.println(m_ssid);
    Serial.print("Wifi Password: ");
    Serial.println(m_wifiPassword);
  }
  WiFi.mode(WIFI_STA);

  int itry = 0;
  while ((WiFi.status() != WL_CONNECTED) && (itry < 5))
  {
    m_setCommLED(!m_commLEDState);
    if (m_printDiagnostics) Serial.print("Attempt: ");
    if (m_printDiagnostics) Serial.println(itry);
    WiFi.begin(m_ssid.c_str(), m_wifiPassword.c_str()); 
    delay(10000);
    itry = itry + 1;
  }
  m_setCommLED(false);

  if (itry == 5) m_rebootPico("Unable to connect to WIFI network, rebooting in 10 seconds...");

  if (m_printDiagnostics) Serial.println("");
  if (m_printDiagnostics) Serial.println("WiFi connected");
  if (m_printDiagnostics) Serial.println("IP address: ");
  if (m_printDiagnostics) Serial.println(WiFi.localIP());
  if (m_printDiagnostics)
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
  m_blinkCommLED(50, 40);
  
  m_mqttClient.setServer(m_mqttServer.c_str(), 1883);
  m_mqttClient.setCallback(m_mqttCubeCallback);
  m_mqttClientId = m_box + "_" + m_trayType + "_" + m_trayName + "_" + m_cubeType;
  m_mqttSubscribeTopic = m_box + "/" + m_cubeType + "/" + m_trayType + "/" + m_trayName + "/setting";
  m_mqttPublishTopic   = m_box + "/" + m_cubeType + "/" + m_trayType + "/" + m_trayName + "/reading";
  m_mqttClient.setKeepAlive(MQTT_KEEP_ALIVE);
  m_mqttClient.setSocketTimeout(MQTT_SOCKETTIMEOUT);
  m_publishNow = false;
  setupLoRa((int16_t) m_trayName.toInt());  
}

void loop()
{
  unsigned long now = millis();
  m_mqttClient.loop();
  m_mqttConnect();
  if (m_publishNow)
  {
    m_setCommLED(true);
    m_lastMsgTime = now;
    for (int ii = 0; ii < SIZE_OF_LORA_NODE; ++ii) m_mqttDatabuffer[ii] = l_loraDatabuffer[ii];
    m_mqttClient.publish(m_mqttPublishTopic.c_str(), m_mqttDatabuffer, SIZE_OF_LORA_NODE);
    m_publishNow = false;
  }
  if (m_commLEDState)
  {
    if ((now - m_lastMsgTime) > MQTT_LED_FLASH_MS) m_setCommLED(false);  
  }

}
