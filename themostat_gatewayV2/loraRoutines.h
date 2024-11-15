#include "CRC16.h"
#include <SPI.h>
#include <LoRa.h>

#define CHSPIN 17          // LoRa radio chip select
#define RSTPIN 14          // LoRa radio reset
#define IRQPIN 15          // LoRa radio IRQ

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
