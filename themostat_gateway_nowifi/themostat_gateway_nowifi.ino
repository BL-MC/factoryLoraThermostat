boolean printDiagnostics = true;

#define CHSPIN 17          // LoRa radio chip select
#define RSTPIN 14          // LoRa radio reset
#define IRQPIN 15          // LoRa radio IRQ
#define COMPIN LED_BUILTIN

#include "CRC16.h"
#include <SPI.h>
#include <LoRa.h>

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
    int16_t data[16];
  };
  uint8_t buffer[28];
};
uint8_t sizeOfLoraNode = 28;

void setup() 
{
  if (printDiagnostics) Serial.begin(9600);
  delay(5000);

  LoRa.setPins(CHSPIN, RSTPIN, IRQPIN);

  if (!LoRa.begin(loraFreq)) 
  {
    if (printDiagnostics) Serial.println("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }

  if (printDiagnostics)
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
void loop()
{
  
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
  LoraNode loraNode;
  uint8_t numBytes = 0;

  while (LoRa.available() )
  {
    numBytes = LoRa.readBytes(loraNode.buffer, sizeOfLoraNode);
  }
  if (numBytes != sizeOfLoraNode) return;
  
  crc.restart();
  for (int ii = 2; ii <sizeOfLoraNode; ii++)
  {
    crc.add(loraNode.buffer[ii]);
  }
  uint16_t crcCalc = crc.calc();
  if (crcCalc != loraNode.header.icrc) return;

  if (loraNode.header.igatewayAddr != igatewayAddr) return;
  
  if (printDiagnostics)
  {
    Serial.print("Gateway Receive: ");
    Serial.println(numBytes);
    Serial.print("icrc           : ");
    Serial.println(loraNode.header.icrc);
    Serial.println("");
  }
}

void onTxDone() 
{
  if (printDiagnostics) Serial.println("TxDone");
  LoRa_rxMode();
}
