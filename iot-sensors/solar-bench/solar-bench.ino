#include <ModbusMaster.h>
#include <lmic.h>
#include <hal/hal.h>
#include "QuickStats.h"
#include "settings.h"

#define RXD2 13
#define TXD2 15
#define RXenable 14
#define pm_array_size 120

QuickStats stats;
uint32_t pm_array_counter = 0;
float volt[pm_array_size], amp[pm_array_size], watt[pm_array_size];
uint32_t runTime;

uint8_t VERSION = 0x2C; // REMEMBER TO INCREASE BY ONE HEX DIGIT WHEN THERE IS CHANGES IN DATA!

uint32_t lastRead = 0;
uint32_t readDelay = 1000; //Sec delay

// LoRa payload
#define payloadSize 20
static uint8_t payload[payloadSize];

// instantiate ModbusMaster object
ModbusMaster node;

void preTransmission()
{
  digitalWrite(RXenable, HIGH);
}

void postTransmission()
{
  digitalWrite(RXenable, LOW);
}

void modbusSetup() {
  Serial2.begin(19200, SERIAL_8N1, RXD2, TXD2);
  pinMode(RXenable, OUTPUT);
  digitalWrite(RXenable, LOW);

  // Modbus slave ID 1
  node.begin(1, Serial2);

  // Callbacks allow us to configure the RS485 transceiver correctly
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
}

static osjob_t sendjob;

void setup() {
  Serial.begin(115200);
  modbusSetup();
  printLoRaWANkeys();
  lmic_init();
  // Schedule next transmission
  os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
  // Start job
  //do_send(&sendjob);
  // Initialise payload
  for (uint8_t i = 0; i < payloadSize; i++) {
    payload[i] = 0;
  }
}

void loop() {
  uint32_t now = millis();
  if ( lastRead + readDelay > now or now < lastRead ) {
    volt[pm_array_counter] = modbusReadFloat(0x0200);
    Serial.print("V: ");
    Serial.println( volt[pm_array_counter] );

    amp[pm_array_counter] = modbusReadFloat(0x0202);
    Serial.print("A: ");
    Serial.println( amp[pm_array_counter] );

    watt[pm_array_counter] = modbusReadFloat(0x0204);
    Serial.print("W: ");
    Serial.println( watt[pm_array_counter] );

    runTime = modbusReadRunTime();
    Serial.print("Running time: ");
    Serial.println( runTime );

    lastRead = millis();
    pm_array_counter++;
  }

  os_runloop_once();
}

float modbusReadFloat(uint16_t addr) {
  uint8_t result;
  uint16_t data[2];
  float volt;

  result = node.readHoldingRegisters(addr, 2);
  if (result != node.ku8MBSuccess) {
    Serial.print("Modbus read failed: ");
    Serial.println(String(result));
    return (-1);
  }

  data[0] = node.getResponseBuffer(1);
  data[1] = node.getResponseBuffer(0);
  memcpy(&volt, data, 4);
  return (volt);
}

uint32_t modbusReadRunTime() {
  uint8_t result;
  result = node.readHoldingRegisters(0x0280, 2);
  if (result != node.ku8MBSuccess) {
    Serial.print("Modbus read failed: ");
    Serial.println(String(result));
    return (0);
  }
  return ((uint32_t)node.getResponseBuffer(0) << 16) | node.getResponseBuffer(1);

  while (Serial2.available()) {
    Serial.print("Garbage from serial buffer: ");
    Serial.println(String(Serial2.read()));
  }
}

void printArray(const char *keyName, u1_t x[], uint8_t s) {
  char buf [2];
  Serial.print(keyName);
  Serial.print(": ");
  for ( int i = 0 ; i < s; i++ ) {
    sprintf(buf, "%02x", x[i]);
    Serial.print(buf);
  }
  Serial.println();
}

void printLoRaWANkeys() {
#ifdef PRINT_KEYS
  printArray("NWKSKEY", NWKSKEY, 16);
  printArray("APPSKEY", APPSKEY, 16);
  printArray("DEVEUI", DEVEUI, 8);
  Serial.print(F("DevAddr: 0x"));
  Serial.println(DEVADDR, HEX);
#endif
}

int32_t addToPayload(uint8_t pl[], uint16_t val, uint32_t i) {
  pl[i++] = val >> 8;
  pl[i++] = val & 0x00FF;
  return i;
}

/**
   Generate payload which is an uint8_t array full of uint16_t values meaning
   bytes 0 and 1 contain the first uint16_t value and so on.
*/
void generatePayload() {
  char buffer [200];
  int cx;
  cx = snprintf ( buffer, 200, "Values to send: volt %.1f amp %.1f watt %.1f",
                  stats.average(volt, pm_array_counter), stats.average(amp, pm_array_counter), stats.average(watt, pm_array_counter) );

  Serial.println(buffer);

  uint16_t tmp;
  uint8_t i = 0;

  // first byte defines protocol (EnergiaBurk)
  payload[i++] = 0x3A;
  // second byte defines version
  payload[i++] = VERSION;
  i = addToPayload(payload, (uint16_t)(stats.average(volt, pm_array_counter) * 10), i);
  i = addToPayload(payload, (uint16_t)(stats.average(amp, pm_array_counter) * 10), i);
  i = addToPayload(payload, (uint16_t)(stats.average(watt, pm_array_counter) * 10), i);
  i = addToPayload(payload, (uint16_t)(runTime * 10), i);

  pm_array_counter = 0;
}
