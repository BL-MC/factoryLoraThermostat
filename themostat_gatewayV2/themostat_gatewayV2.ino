
boolean l_printDiagnostics = false;
boolean m_printDiagnostics = false;
boolean m_publishNow = false;
boolean l_publishNow = false;

#include "creds.h"
#include "mqttRoutines.h"
#include "loraRoutines.h"


void setup()
{
  if (m_printDiagnostics || l_printDiagnostics) Serial.begin(115200);
  m_setup();
  setupLoRa((int16_t) m_trayName.toInt());  
}
void loop()
{
  unsigned long now = millis();
  m_loop(now, l_loraDatabuffer, l_sizeOfLoraNode);
  l_loop(now, m_mqttDatabuffer, m_sizeOfMqttData);
}
