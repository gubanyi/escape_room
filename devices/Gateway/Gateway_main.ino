/*******************************************************************************
  Copyright (c) 2015 Matthijs Kooijman
  Copyright (c) 2018 Terry Moore, MCCI Corporation

  Permission is hereby granted, free of charge, to anyone
  obtaining a copy of this document and accompanying files,
  to do whatever they want with them without any restriction,
  including, but not limited to, copying, modification and redistribution.
  NO WARRANTY OF ANY KIND IS PROVIDED.

  This example transmits data on hardcoded channel and receives data
  when not transmitting. Running this sketch on two nodes should allow
  them to communicate.
*******************************************************************************/
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <TemperatureZero.h>

/** Defines **/
#define NODE_ID  1
#define NETWORK_FREQUENCY 915

// clock parameters
#define CPU_HZ 48000000
#define TIMER_PRESCALER_DIV 1024

// Error codes
#define ERROR_CODE_MISSING_PACKET     0x01
#define ERROR_CODE_RECEPTION_FAILURE    0x02
#define ERROR_CODE_WDT_EARLY_WARNING    0x04
#define ERROR_CODE_INVALID_TEMPERATURE    0x08
#define ERROR_CODE_MISMATCHED_PACKET_NUM  0x10


#define MAX_MESSAGE_LENGTH 32
/** Type Definitions **/
typedef struct
{
  uint8_t NodeID;
  uint16_t PacketID;
  uint32_t Timestamp;
  float AverageTemperature;
  uint8_t ErrorType;
} Lab3Packet_t;

/** Static Variables **/
static TemperatureZero TempZero = TemperatureZero();
static float g_temperature_samples[5];
static float g_temperature_samples_count = 0;
static int32_t initialMillis = 0;
static volatile Lab3Packet_t txPacket;
static volatile unsigned int counterISR = 1;

// we formerly would check this configuration; but now there is a flag,
// in the LMIC, LMIC.noRXIQinversion;
// if we set that during init, we get the same effect. If
// DISABLE_INVERT_IQ_ON_RX is defined, it means that LMIC.noRXIQinversion is
// treated as always set.
//
// #if !defined(DISABLE_INVERT_IQ_ON_RX)
// #error This example requires DISABLE_INVERT_IQ_ON_RX to be set. Update \
// lmic_project_config.h in arduino-lmic/project_config to set it.
// #endif
// How often to send a packet. Note that this sketch bypasses the normal
// LMIC duty cycle limiting, so when you change anything in this sketch
// (payload length, frequency, spreading factor), be sure to check if
// this interval should not also be increased.
// See this spreadsheet for an easy airtime and duty cycle calculator:
// https://docs.google.com/spreadsheets/d/1voGAtQAjC1qBmaVuP1ApNKs1ekgUjavHuVQIXyYSvNc
#define TX_INTERVAL 60000 //Delay between each message in millisecond.
#define INTERVAL_NODE 1000

// Pin mapping for SAMD21
const lmic_pinmap lmic_pins = {
  .nss = 12,//RFM Chip Select
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 7,//RFM Reset
  .dio = {6, 10, 11}, //RFM Interrupt, RFM LoRa pin, RFM LoRa pin
};

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in arduino-lmoc/project_config/lmic_project_config.h,
// otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }
void onEvent (ev_t ev) { }

osjob_t txjob;
osjob_t timeoutjob;
static void tx_func (osjob_t* job);

// Transmit the given string and call the given function afterwards
/*
void tx(const char *str, osjobcb_t func)
{
  os_radio(RADIO_RST); // Stop RX first
  delay(1); // Wait a bit, without this os_radio below asserts, apparently because the state hasn't changed yet
  LMIC.dataLen = 0;
  while (*str)
    LMIC.frame[LMIC.dataLen++] = *str++;
  LMIC.osjob.func = func;
  os_radio(RADIO_TX);
  SerialUSB.println("TX");
}
*/
void tx(const void* msg, uint8_t len, osjobcb_t func)
{
  os_radio(RADIO_RST); // Stop RX first
  delay(1); // Wait a bit, without this os_radio below asserts, apparently because the state hasn't changed yet
  LMIC.dataLen = len;
  uint16_t i;
  for (i = 0; i < len; i++)
  {
    LMIC.frame[i] = *(((uint8_t*)msg) + i);
  }
  LMIC.osjob.func = func;
  os_radio(RADIO_TX);
  SerialUSB.println("TX");
}

// Enable rx mode and call func when a packet is received
void rx(osjobcb_t func)
{
  LMIC.osjob.func = func;
  LMIC.rxtime = os_getTime(); // RX _now_
  // Enable "continuous" RX (e.g. without a timeout, still stops after
  // receiving a packet)
  os_radio(RADIO_RXON);
  SerialUSB.println("RX");
}

static void rxtimeout_func(osjob_t *job)
{
  digitalWrite(LED_BUILTIN, LOW); // off
}

static void rx_func (osjob_t* job)
{
  // Blink once to confirm reception and then keep the led on
  digitalWrite(LED_BUILTIN, LOW); // off
  delay(10);
  digitalWrite(LED_BUILTIN, HIGH); // on
  // Timeout RX (i.e. update led status) after 3 periods without RX
  os_setTimedCallback(&timeoutjob, os_getTime() + ms2osticks(3*TX_INTERVAL), rxtimeout_func);
  // Reschedule TX so that it should not collide with the other side's next TX
  //os_setTimedCallback(&txjob, os_getTime() + ms2osticks(TX_INTERVAL / 2), tx_func);

  // Print the received output
  if (LMIC.dataLen != sizeof(Lab3Packet_t))
  {
    SerialUSB.println("Error: improper RX packet size.");
  }
  else
  {
    Lab3Packet_t* packet = (Lab3Packet_t*)LMIC.frame;

    if (initialMillis == 0)
    {
      initialMillis = millis() - packet->Timestamp;
    }
    printPacket(packet);
  }

  //SerialUSB.print("Got ");
  //SerialUSB.print(LMIC.dataLen);
  //SerialUSB.println(" bytes");
  //SerialUSB.write(LMIC.frame, LMIC.dataLen);
  //SerialUSB.println();

  // Restart RX
  rx(rx_func);
}

static void txdone_func (osjob_t* job)
{
  rx(rx_func);
}

// log text to USART and toggle LED
static void tx_func (osjob_t* job)
{
  // say hello
  //tx("Hello, world!", 14, txdone_func);
  //SerialUSB.println("Hello world");
  
  // Construct packet
  constructTxPacket((Lab3Packet_t*)&txPacket);

  // Send the packet
  tx((Lab3Packet_t*)&txPacket, sizeof(txPacket), txdone_func);

  // Print the packet
  printPacket((Lab3Packet_t*)&txPacket);
  IsErrorDetect((Lab3Packet_t*)&txPacket);

  // Set up the next packet's packet ID
  txPacket.PacketID++;
  txPacket.ErrorType = 0;

  // reschedule job every TX_INTERVAL (plus a bit of random to prevent systematic collisions), unless packets are received, then rx_func will reschedule at half this time.
  os_setTimedCallback(job, os_getTime() + ms2osticks(TX_INTERVAL + random(500)), tx_func);
}

// application entry point
void setup()
{
  // Initialize temperature readings, start timer that gets temperature samples
  TempZero.init();
  startTimer(1);

  // Initialize Serial USB interface
  SerialUSB.begin(115200);
  while (!SerialUSB); // wait for Serial Monitor to be attached to SparkFun Pro RF board
  SerialUSB.println("Starting");

  // Initialize transmit packet
  initPacket((Lab3Packet_t*)&txPacket);
  
  pinMode(LED_BUILTIN, OUTPUT);
  // initialize runtime env
  os_init();
  
  // this is automatically set to the proper bandwidth in kHz, based on the selected channel
  uint32_t uBandwidth;
  LMIC.freq = 903900000; // originally was 903900000 in the example
  uBandwidth = 125;
  LMIC.datarate = US915_DR_SF9; // DR4 // originally US915_DR_SF7
  LMIC.txpow = 20;  // originally was 21 in the example
  
  // disable RX IQ inversion
  LMIC.noRXIQinversion = true;
  
  // This sets CR 4/5, BW125 (except for EU/AS923 DR_SF7B, which uses BW250)
  LMIC.rps = updr2rps(LMIC.datarate);
  SerialUSB.print("Frequency: "); SerialUSB.print(LMIC.freq / 1000000);
  SerialUSB.print("."); SerialUSB.print((LMIC.freq / 100000) % 10);
  SerialUSB.print("MHz");
  SerialUSB.print(" LMIC.datarate: "); SerialUSB.print(LMIC.datarate);
  SerialUSB.print(" LMIC.txpow: "); SerialUSB.println(LMIC.txpow);
  
  // This sets CR 4/5, BW125 (except for DR_SF7B, which uses BW250)
  LMIC.rps = updr2rps(LMIC.datarate);
  
  // disable RX IQ inversion
  LMIC.noRXIQinversion = true;
  SerialUSB.println("Started");
  SerialUSB.flush();
  
  // setup initial job
  //os_setCallback(&txjob, tx_func);
}

void loop()
{
  // execute scheduled jobs and events
  os_runloop_once();
  //Check to see if anything is available in the serial receive buffer
   while (SerialUSB.available() > 0)
   {
     
     //Create a place to hold the incoming message
     static char message[MAX_MESSAGE_LENGTH];
     static unsigned int message_pos = 0;
  
     //Read the next available byte in the serial receive buffer
     char inByte = SerialUSB.read();
  
     //Message coming in (check not terminating character) and guard for over message size
     if ( inByte != '\n' && (message_pos < MAX_MESSAGE_LENGTH - 1) )
     {
       //Add the incoming byte to our message
       message[message_pos] = inByte;
       message_pos++;
     }
     //Full message received...
     else
     {
       //Add null character to string
       message[message_pos] = '\0';
  
       //Print the message (or do other things)
       SerialUSB.println("command:");
       SerialUSB.println(message);

       tx(message, message_pos, txdone_func);
       //Reset for the next message
       message_pos = 0;
     }
     //SerialUSB.print("hello");
   }
}



/** Function Definitions **/
int getNewTemperatureSample()
{
  // Returns 0 if new temperature sample is valid, returns -1 if not
  float temperature = TempZero.readInternalTemperature();
  
  // Is this a valid temperature?
  if ((temperature >= 0.1) && (temperature <= 100.0))
  {
    // This is a valid temperature
    // Shift over the sample buffer
    int i;
    for (i = 4; i > 0; i--)
    {
      g_temperature_samples[i] = g_temperature_samples[i - 1];
    }
    
    // Put the new temperature in the sample buffer
    g_temperature_samples[0] = temperature;
    
    // If the sample buffer is not full, increment the number of samples in it
    if (g_temperature_samples_count < 5)
    {
      g_temperature_samples_count++;
    }
    
    return 0;
  }
  else
  {
    return -1;
  }
}

float getAverageTemperature()
{
  // Returns the average temperature if there are 5 valid temperature samples in the buffer, returns NAN otherwise
  if (g_temperature_samples_count == 5)
  {
    // Calculate average
    float sum = 0;
    int i;
    for (i = 0; i < 5; i++)
    {
      sum += g_temperature_samples[i];
    }
    return sum / 5.0;
  }
  else
  {
    return NAN;
  }
}

void initPacket(Lab3Packet_t* packet)
{
  packet->NodeID = NODE_ID;
  packet->PacketID = 1;
  packet->Timestamp = 0;
  packet->AverageTemperature = 0;
  packet->ErrorType = 0x00;
}

String packetToString(Lab3Packet_t* packet, bool printThisDeviceTag)
{
  String s = String("Node ID = ")
    + String(packet->NodeID)
    + String(", Packet ID = ")
    + String(packet->PacketID)
    + String(", Timestamp = ")
    + String(packet->Timestamp)
    + String(", Average Temperature = ")
    + String(packet->AverageTemperature)
    + String(", Error Type = 0x")
    + String(packet->ErrorType, HEX)
  ;

  if (printThisDeviceTag && (packet->NodeID == NODE_ID))
  {
    s += String(" (this device)");
  }

  return s;
}

void printPacket(Lab3Packet_t* packet)
{
  SerialUSB.println(packetToString(packet, true));
}

void constructTxPacket(Lab3Packet_t* packet_out)
{
  // Assumes that you have added all the errors to packet->ErrorType before calling this function
  
  // Get the average temperature
  float averageTemperature = getAverageTemperature();
  if ((averageTemperature != averageTemperature) || (averageTemperature > 60) || (averageTemperature < 0))
  {
    packet_out->ErrorType |= ERROR_CODE_INVALID_TEMPERATURE;
  }

  // Populate packet with data
  packet_out->Timestamp = millis() - initialMillis;
  packet_out->AverageTemperature = averageTemperature;

  return;
}

void setTimerFrequency(int frequencyHz)
{
  int compareValue3 = (CPU_HZ / (TIMER_PRESCALER_DIV * frequencyHz)) - 1;
  TcCount16* TC4p = (TcCount16*) TC4;
  // Make sure the count is in a proportional position to where it was
  // to prevent any jitter or disconnect when changing the compare value.
  TC4p->COUNT.reg = map(TC4p->COUNT.reg, 0, TC4p->CC[0].reg, 0, compareValue3);
  TC4p->CC[0].reg = compareValue3;
  while (TC4p->STATUS.bit.SYNCBUSY == 1);
}

void startTimer(int frequencyHz)
{
  REG_GCLK_CLKCTRL = (uint16_t) (GCLK_CLKCTRL_CLKEN |
  GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TC4_TC5) ;
  while ( GCLK->STATUS.bit.SYNCBUSY == 1 ); // wait for sync
  TcCount16* TC4p = (TcCount16*) TC4;
  TC4p->CTRLA.reg &= ~TC_CTRLA_ENABLE; //Disable timer
  TC4p->CTRLA.reg |= TC_CTRLA_MODE_COUNT16 | TC_CTRLA_WAVEGEN_MFRQ | TC_CTRLA_PRESCALER_DIV1024;
  while (TC4p->STATUS.bit.SYNCBUSY == 1); // wait for sync
  setTimerFrequency(1);
  TC4p->INTENSET.reg = 0;
  TC4p->INTENSET.bit.OVF = 1;
  NVIC_EnableIRQ(TC4_IRQn);
  TC4p->CTRLA.reg |= TC_CTRLA_ENABLE;
  while (TC4p->STATUS.bit.SYNCBUSY == 1); // wait for sync
}

bool IsErrorDetect(Lab3Packet_t* packet)
{
  if (packet->ErrorType == 0)
  {
    return false;
  }
  
  uint32_t errorCode = 0
    | ((uint32_t)packet->ErrorType) << 24 // ErrorType occupies bits 31 downto 24
    | ((uint32_t)packet->NodeID) << 16    // NodeID occupies bits 23 downto 16
    | ((uint32_t)packet->PacketID)      // PacketID occupies bits 15 downto 0
  ;
  
  // Print an error message
  printError(errorCode);
  
  return true;
}

void printError(uint32_t errorCode)
{
  uint8_t errorType = errorCode >> 24;
  uint8_t nodeID = errorCode >> 16;
  uint16_t packetID = errorCode;
  
  SerialUSB.print("Error(s) on Node ID ");
  SerialUSB.print(": ");
  
  if (errorType == 0)
  {
    SerialUSB.println("No errors");
    return;
  }
  
  SerialUSB.print("Error(s): ");
  if (errorType & ERROR_CODE_MISSING_PACKET)
  {
    SerialUSB.print("{Missing packet} ");
  }
  if (errorType & ERROR_CODE_RECEPTION_FAILURE)
  {
    SerialUSB.print("{Reception failure} ");
  }
  if (errorType & ERROR_CODE_WDT_EARLY_WARNING)
  {
    SerialUSB.print("{WDT early warning} ");
  }
  if (errorType & ERROR_CODE_INVALID_TEMPERATURE)
  {
    SerialUSB.print("{Invalid temperature} ");
  }
  SerialUSB.println("");
}

/** ISRs **/
void TC4_Handler()
{
  TcCount16* TC4p = (TcCount16*) TC4;
  // If this interrupt is due to the compare register matching the timer count
  // we toggle the LED.
  if (TC4p->INTFLAG.bit.OVF == 1)
  {
    TC4p->INTFLAG.bit.OVF = 1;
    // Write callback here!!!
    // This handler executes every 1.0 seconds to measure the temperature and add it to the moving average
    getNewTemperatureSample();
    // five second block counter
  }
}
