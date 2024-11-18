#include "CRC16.h"
#include <SPI.h>
#include <LoRa.h>

#define CHSPIN 17          // LoRa radio chip select
#define RSTPIN 14          // LoRa radio reset
#define IRQPIN 15          // LoRa radio IRQ
#define LORSBW 62e3
#define LORSPF 9
#define LORFRQ 868E6

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
  int16_t irssi;  
  int16_t isnr;  
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

void LoRa_sendMessage(uint8_t *buffer, uint8_t size) 
{
  LoRa_txMode();                        // set tx mode
  LoRa.beginPacket();                   // start packet
  LoRa.write(buffer, (size_t) size);    // add payload
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
  loraNode.header.irssi = (int16_t)   LoRa.packetRssi();
  loraNode.header.isnr  = (int16_t)  (LoRa.packetSnr() * 100);
  if (l_printDiagnostics)
  {
    Serial.print("Gateway Receive: ");
    Serial.println(numBytes);
    Serial.print("packetRssi     : ");
    Serial.println(loraNode.header.irssi);
    Serial.print("packetSnr      : ");
    float fsnr = ((float)loraNode.header.isnr) / 100;
    Serial.println(fsnr);
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
  LoRa.setSpreadingFactor(LORSPF);
  LoRa.setSignalBandwidth(LORSBW);

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
void l_loop(unsigned long now, uint8_t* mqttDatabuffer, uint8_t sizeOfMqttData)
{
  if (l_publishNow)
  {
    LoraNode loraNode;
    for (int ii = 0; ii < sizeOfMqttData; ++ii) loraNode.buffer[ii] = mqttDatabuffer[ii];
    l_crc.restart();
    for (int ii = 2; ii < l_sizeOfLoraNode; ii++)
    {
      l_crc.add(loraNode.buffer[ii]);
    }
    uint16_t crcCalc = l_crc.calc();
    loraNode.header.icrc = l_crc.calc();
    LoRa_sendMessage(loraNode.buffer, l_sizeOfLoraNode);
    if (l_printDiagnostics) Serial.println("Sending data to LoRa Node");

    l_publishNow = false;
  }
  
}
