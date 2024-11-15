
boolean printDiagnostics = true;
#include <WiFi.h>
#include <PubSubClient.h>
//#include "LittleFS.h"

#define COMPIN LED_BUILTIN
#define MQTT_RETRY            3000
#define MAX_NO_MQTT_ERRORS    5
#define MAX_NO_CONNECTION_ATTEMPTS 5
#define MQTT_KEEP_ALIVE    8
#define MQTT_SOCKETTIMEOUT 4
#define MQTT_LED_FLASH_MS 10

WiFiClient    g_wifiClient;
PubSubClient  g_mqttClient(g_wifiClient);

#include "creds.h"
String        g_mqttClientId = "";
unsigned long g_mqttLastTry = 0;
int           g_noMqttErrors = 0; 
boolean       g_commLEDState = false;
uint8_t*      g_mqttDatabuffer;
uint8_t       g_mqttDatabufferSize;
boolean       g_publishNow = false;
unsigned long g_lastMsgTime = 0;
unsigned long lastPublishTime;
unsigned long publishInterval = 4000;

uint8_t testData[] = {0,1,2,3,4,5,6,7};
uint8_t testDataSize = 8;

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
    if (printDiagnostics) Serial.println(warning);
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

  if (printDiagnostics) Serial.print("Attempting MQTT connection using ID...");
  if (printDiagnostics)  Serial.println(g_mqttClientId);
  connected = g_mqttClient.connect(g_mqttClientId.c_str(),g_mqttUsername.c_str(), g_mqttPassword.c_str());
  g_mqttLastTry = now;
  if (connected) 
  {
    if (printDiagnostics) Serial.println("...connected");
    g_mqttClient.subscribe(g_mqttSubscribeTopic.c_str());
    g_noMqttErrors = 0;
    return true;
  } 
  int mqttState = g_mqttClient.state();
  if (printDiagnostics) Serial.print(" failed, rc=");
  if (printDiagnostics) Serial.print(mqttState);
  if (printDiagnostics) Serial.print(": ");

  switch (mqttState) 
  {
    case -4:
      if (printDiagnostics) Serial.println("MQTT_CONNECTION_TIMEOUT");
      break;
    case -3:
      if (printDiagnostics) Serial.println("MQTT_CONNECTION_LOST");
      break;
    case -2:
      g_noMqttErrors = g_noMqttErrors + 1;
      if (printDiagnostics) Serial.print("Number of MQTT connection attempts: ");
      if (printDiagnostics) Serial.print(g_noMqttErrors);
      if (g_noMqttErrors > MAX_NO_MQTT_ERRORS) rebootPico("Too may MQTT reconnect attempts. Rebooting...");
      break;
    case -1:
      if (printDiagnostics) Serial.println("MQTT_DISCONNECTED");
      break;
    case 0:
      if (printDiagnostics) Serial.println("MQTT_CONNECTED");
      break;
    case 1:
      if (printDiagnostics) Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
      break;
    case 2:
      if (printDiagnostics) Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
      break;
    case 3:
      if (printDiagnostics) Serial.println("MQTT_CONNECT_UNAVAILABLE");
      break;
    case 4:
      if (printDiagnostics) Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
      break;
    case 5:
      if (printDiagnostics) Serial.println("MQTT_CONNECT_UNAUTHORIZED");
      break;
    default:
      break;
  }
  return false;
}
void setup()
{
  pinMode(COMPIN, OUTPUT);
  setCommLED(false);
  if (printDiagnostics) Serial.begin(115200);
  blinkCommLED(20, 500);
  
  if (printDiagnostics) Serial.println("Reading creds.txt file");

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
        if (printDiagnostics) Serial.println(lines[ii]);
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
  if (printDiagnostics)
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
    if (printDiagnostics) Serial.print("Attempt: ");
    if (printDiagnostics) Serial.println(itry);
    WiFi.begin(g_ssid.c_str(), g_wifiPassword.c_str()); 
    delay(10000);
    itry = itry + 1;
  }
  setCommLED(false);

  if (itry == 5) rebootPico("Unable to connect to WIFI network, rebooting in 10 seconds...");

  if (printDiagnostics) Serial.println("");
  if (printDiagnostics) Serial.println("WiFi connected");
  if (printDiagnostics) Serial.println("IP address: ");
  if (printDiagnostics) Serial.println(WiFi.localIP());
  if (printDiagnostics)
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
    g_mqttClient.publish(g_mqttPublishTopic.c_str(), g_mqttDatabuffer, g_mqttDatabufferSize);
    g_publishNow = false;
  }
  if (g_commLEDState)
  {
    if ((now - g_lastMsgTime) > MQTT_LED_FLASH_MS) setCommLED(false);  
  }
  if ((now - lastPublishTime) > publishInterval)
  {
    lastPublishTime = now;
    g_publishNow = true;
    g_mqttDatabuffer = testData;
    g_mqttDatabufferSize = testDataSize;
  }

}
