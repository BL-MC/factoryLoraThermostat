#define BLINKY_DIAG         0
#define LORA_DIAG         false
#define COMM_LED_PIN       12
#define RST_BUTTON_PIN     -1

#define SIZE_OF_NODE_DATA 16
#define GATEWAYADDRESS 10
#define CHSPIN 17           // LoRa radio chip select
#define RSTPIN 14           // LoRa radio reset
#define IRQPIN 15           // LoRa radio IRQ
#define LORSBW 62e3
#define LORSPF 9
#define LORFRQ 868E6

#include <BlinkyPicoW.h>
#include <BlinkyLoraGateway.h>

struct CubeSetting
{
  GatewayDataHeader gatewayDataHeader;
  uint8_t nodeData[SIZE_OF_NODE_DATA];
};
CubeSetting setting;

struct CubeReading
{
  uint16_t flag;
};
CubeReading reading;

unsigned long lastPublishTime;

void setupBlinky()
{
  if (BLINKY_DIAG > 0) Serial.begin(9600);

  BlinkyPicoW.setSsid("Blinky-Lite");
  BlinkyPicoW.setWifiPassword("SwedishChef");
  BlinkyPicoW.setMqttServer("hub.bl-mc.com");
  BlinkyPicoW.setMqttUsername("xxxx");
  BlinkyPicoW.setMqttPassword("areallybadpassword");
  BlinkyPicoW.setBox("blinky-lite-box-01");
  BlinkyPicoW.setTrayType("lora-gateway");
  BlinkyPicoW.setTrayName("10");
  BlinkyPicoW.setCubeType("blinky-hub");
  BlinkyPicoW.setMqttKeepAlive(15);
  BlinkyPicoW.setMqttSocketTimeout(4);
  BlinkyPicoW.setMqttPort(1883);
  BlinkyPicoW.setMqttLedFlashMs(100);
  BlinkyPicoW.setHdwrWatchdogMs(8000);

  BlinkyPicoW.begin(BLINKY_DIAG, COMM_LED_PIN, RST_BUTTON_PIN, true, sizeof(setting), sizeof(reading));
}

void setupCube()
{
  if (LORA_DIAG) 
  {
    Serial.begin(9600);
    delay(5000);
  }
  BlinkyLoraGateway.begin(SIZE_OF_NODE_DATA, LORA_DIAG, GATEWAYADDRESS, CHSPIN, RSTPIN, IRQPIN, LORFRQ, LORSPF, LORSBW);
}
void loopCube()
{
  if (BlinkyLoraGateway.retrieveNodeData((uint8_t*) &setting) )
  {
    reading.flag = 0;
    boolean successful = BlinkyPicoW.publishCubeData((uint8_t*) &setting, (uint8_t*) &reading, false);
  }
  if (BlinkyPicoW.retrieveCubeSetting((uint8_t*) &setting) )
  {
      if (LORA_DIAG)  Serial.println("New Setting recevied from MQTT");
      BlinkyLoraGateway.publishGatewayData((uint8_t*) &setting);
  }

}
