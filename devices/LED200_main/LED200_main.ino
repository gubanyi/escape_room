/** Includes **/
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#include "EscapeRoomEndNode.hpp"
#include "EscapeRoomActivationDevice.hpp"
#include "EscapeRoomTriggerDevice.hpp"



/** Defines **/
#define THIS_END_NODE_ID  6
#define NETWORK_FREQUENCY 903900000
#define HEARTBEAT_DELAY_MS  6000



/** Function Declarations **/
String LED1_setConfigFunc(String config);
String LED1_setStateFunc(String state);
String LED1_getStateFunc();

String LED2_setConfigFunc(String config);
String LED2_setStateFunc(String state);
String LED2_getStateFunc();

String LED3_setConfigFunc(String config);
String LED3_setStateFunc(String state);
String LED3_getStateFunc();

String LED4_setConfigFunc(String config);
String LED4_setStateFunc(String state);
String LED4_getStateFunc();

// These callbacks are only used in over-the-air activation, so they are left empty here (we cannot leave them out completely unless DISABLE_JOIN is set in arduino-lmic/project_config/lmic_project_config.h, otherwise the linker will complain). Do not change these.
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }
void onEvent (ev_t ev) { }

static void tx_func(osjob_t* job);



/** Global Variables **/
// Activation Devices
EscapeRoomActivationDevice LED1_activation("SO", &LED1_setConfigFunc, &LED1_getStateFunc, &LED1_setStateFunc);
EscapeRoomActivationDevice LED2_activation("SO", &LED2_setConfigFunc, &LED2_getStateFunc, &LED2_setStateFunc);
EscapeRoomActivationDevice LED3_activation("SO", &LED3_setConfigFunc, &LED3_getStateFunc, &LED3_setStateFunc);
EscapeRoomActivationDevice LED4_activation("SO", &LED4_setConfigFunc, &LED4_getStateFunc, &LED4_setStateFunc);

// End node class setup
const EscapeRoomActivationDevice* activationDevices[] = { &LED1_activation, &LED2_activation, &LED3_activation, &LED4_activation };
const int numActivationDevices = 4;
const EscapeRoomTriggerDevice* triggerDevices[] = {};
const int numTriggerDevices = 0;
EscapeRoomEndNode node(THIS_END_NODE_ID, numActivationDevices, activationDevices, numTriggerDevices, triggerDevices);

// Pin mapping for SAMD21 to LoRa modem. Do not change these
const lmic_pinmap lmic_pins = {
  .nss = 12,//RFM Chip Select
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 7,//RFM Reset
  .dio = {6, 10, 11}, //RFM Interrupt, RFM LoRa pin, RFM LoRa pin
};

osjob_t txjob;
osjob_t timeoutjob;
unsigned int lastTxMillis = 0;

// Test strings
// {"SNID":1,"TNID":4,"PID":2,"UT":1000,"AL":[{"ID":1,"T":"SO","S":"1"}]} // Turns on LED 1
// {"SNID":1,"TNID":4,"PID":3,"UT":1000,"AL":[{"ID":1,"T":"SO","S":"0"}]} // Turns off LED 1
// {"SNID":1,"TNID":4,"PID":4,"UT":1000,"AL":[{"ID":2,"T":"SO","S":"1"}]} // Turns on LED 2
// {"SNID":1,"TNID":4,"PID":5,"UT":1000,"AL":[{"ID":2,"T":"SO","S":"0"}]} // Turns off LED 2
// {"SNID":1,"TNID":4,"PID":6,"UT":1000,"AL":[{"ID":3,"T":"SO","S":"1"}]} // Turns on LED 3
// {"SNID":1,"TNID":4,"PID":7,"UT":1000,"AL":[{"ID":3,"T":"SO","S":"0"}]} // Turns off LED 3
// {"SNID":1,"TNID":4,"PID":8,"UT":1000,"AL":[{"ID":4,"T":"SO","S":"0"}]} // Turns off LED 4



/** Main Functions **/
void setup()
{
  // Set up switched LED defaults
  digitalWrite(2, HIGH);  // LED1
  pinMode(2, OUTPUT); // LED1
  
  digitalWrite(3, HIGH);  // LED2
  pinMode(3, OUTPUT); // LED2
  
  digitalWrite(4, HIGH);  // LED3
  pinMode(4, OUTPUT); // LED3
  
  digitalWrite(5, HIGH);  // LED4
  pinMode(5, OUTPUT); // LED4

  
  SerialUSB.begin(115200);
  //while (!SerialUSB) {}
  SerialUSB.println("**********Switched LED**********");
  SerialUSB.println();

  // Initialize runtime env
  os_init();

  // Set up LMIC wireless channel Do not change this
  uint32_t uBandwidth;
  LMIC.freq = NETWORK_FREQUENCY; // originally was 903900000 in the example
  uBandwidth = 125;
  LMIC.datarate = US915_DR_SF9; // originally US915_DR_SF7
  LMIC.txpow = 20;  // originally was 21 in the example

  // disable RX IQ inversion
  LMIC.noRXIQinversion = true;

  // This sets CR 4/5, BW125 (except for EU/AS923 DR_SF7B, which uses BW250)
  LMIC.rps = updr2rps(LMIC.datarate);

  // disable RX IQ inversion
  LMIC.noRXIQinversion = true;

  // Set up initial transmission of the state
  os_setCallback(&txjob, tx_func);
}

void loop()
{
  // No need to change this

  // Do we need to send anything? Could be requests to send state of triggers or activation devices
  if (node.GetAnySendStateIsRequested())
  {
    // Trigger a TX for the OS to handle
    os_setCallback(&txjob, tx_func);
  }

  // Run the OS callbacks, including scheduled jobs and events
  os_runloop_once();
}



/** Function Definitions **/
static void tx_func(osjob_t* job)
{
  // No need to change this
  
  // Construct JSON packet
  String json = node.CreateTxJsonStr();

  // Send the packet
  tx(json.c_str(), json.length(), txdone_func);
  DEBUG_NOTE("Sent packet\"" + json + "\"");

  // Reschedule job every HEARTBEAT_DELAY_MS (plus a bit of random to prevent systematic collisions), which will be overridden if another packet must be transmitted before that.
  os_setTimedCallback(job, os_getTime() + ms2osticks(HEARTBEAT_DELAY_MS + random(500)), tx_func);
}

static void rx_func(osjob_t* job)
{ 
  // No need to change this

  // Copy the raw received data and convert it to a JSON string
  String json = "";
  for (int i = 0; i < LMIC.dataLen; i++)
  {
    json += (char)LMIC.frame[i];
  }

  // Process the received data
  DEBUG_NOTE("Received packet \"" + json + "\"");
  node.ParseRxJsonStr(json);
  
  // Restart the RX portion of the radio, using this function as the callback
  rx(rx_func);
}

void tx(const void* msg, uint8_t len, osjobcb_t func)
{
  // No need to change this

  os_radio(RADIO_RST); // Stop RX before transmitting
  delay(1); // Wait a bit, without this os_radio below asserts, apparently because the state hasn't changed yet
  LMIC.dataLen = len;
  uint16_t i;
  for (i = 0; i < len; i++)
  {
    LMIC.frame[i] = *(((uint8_t*)msg) + i);
  }
  LMIC.osjob.func = func;
  os_radio(RADIO_TX);
}

static void txdone_func(osjob_t* job)
{
  // No need to change this

  // Clear errors after they have been transmitted
  node.ErrorString = "";

  // Set up for RX
  rx(rx_func);

  DEBUG_NOTE("Finished sending packet");
}

static void rxtimeout_func(osjob_t* job)
{
  // No need to change this

  // Not sure what this really is supposed to do...
  return;
}

void rx(osjobcb_t func)
{
  // No need to change this

  // This resets the radio to begin listening again. This function does not need to be modified
  LMIC.osjob.func = func; // Set the callback for the next reception
  LMIC.rxtime = os_getTime(); // RX _now_
  // Enable "continuous" RX (e.g. without a timeout, still stops after receiving a packet)
  os_radio(RADIO_RXON);
}



/** Driver Functions for trigger and activation devices **/
// You need to put your own driver functions here!
String LED1_setConfigFunc(String config)
{
  // The LED has no configuration
  return "";
}

String LED1_setStateFunc(String state)
{
  if (state == "0")
  {
    digitalWrite(2, HIGH);
    DEBUG_NOTE("Set LED 1 state to \"0\"");
    return "0";
  }
  else if (state == "1")
  {
    digitalWrite(2, LOW);
    DEBUG_NOTE("Set LED 1 state to \"1\"");
    return "1";
  }
  else
  {
    DEBUG_ERROR("Set LED 1 state: Invalid state \"" + state + "\"");
    return "ER: invalid state";
  }
}

String LED1_getStateFunc()
{
  if (!digitalRead(2))
  {
    DEBUG_NOTE("Get LED 1 state = \"1\"");
    return "1";
  }
  else
  {
    DEBUG_NOTE("Get LED 1 state = \"0\"");
    return "0";
  }
}

String LED2_setConfigFunc(String config)
{
  // The LED has no configuration
  return "";
}

String LED2_setStateFunc(String state)
{
  if (state == "0")
  {
    digitalWrite(3, HIGH);
    DEBUG_NOTE("Set LED 2 state to \"0\"");
    return "0";
  }
  else if (state == "1")
  {
    digitalWrite(3, LOW);
    DEBUG_NOTE("Set LED 2 state to \"1\"");
    return "1";
  }
  else
  {
    DEBUG_ERROR("Set LED 2 state: Invalid state \"" + state + "\"");
    return "ER: invalid state";
  }
}

String LED2_getStateFunc()
{
  if (!digitalRead(3))
  {
    DEBUG_NOTE("Get LED 2 state = \"1\"");
    return "1";
  }
  else
  {
    DEBUG_NOTE("Get LED 2 state = \"0\"");
    return "0";
  }
}

String LED3_setConfigFunc(String config)
{
  // The LED has no configuration
  return "";
}

String LED3_setStateFunc(String state)
{
  if (state == "0")
  {
    digitalWrite(4, HIGH);
    DEBUG_NOTE("Set LED 3 state to \"0\"");
    return "0";
  }
  else if (state == "1")
  {
    digitalWrite(4, LOW);
    DEBUG_NOTE("Set LED 3 state to \"1\"");
    return "1";
  }
  else
  {
    DEBUG_ERROR("Set LED 3 state: Invalid state \"" + state + "\"");
    return "ER: invalid state";
  }
}

String LED3_getStateFunc()
{
  if (!digitalRead(4))
  {
    DEBUG_NOTE("Get LED 3 state = \"1\"");
    return "1";
  }
  else
  {
    DEBUG_NOTE("Get LED 3 state = \"0\"");
    return "0";
  }
}

String LED4_setConfigFunc(String config)
{
  // The LED has no configuration
  return "";
}

String LED4_setStateFunc(String state)
{
  if (state == "0")
  {
    digitalWrite(5, LOW);
    DEBUG_NOTE("Set LED 4 state to \"0\"");
    return "0";
  }
  else if (state == "1")
  {
    digitalWrite(2, HIGH);
    DEBUG_NOTE("Set LED 4 state to \"1\"");
    return "1";
  }
  else
  {
    DEBUG_ERROR("Set LED 4 state: Invalid state \"" + state + "\"");
    return "ER: invalid state";
  }
}

String LED4_getStateFunc()
{
  if (digitalRead(5))
  {
    DEBUG_NOTE("Get LED 1 state = \"1\"");
    return "1";
  }
  else
  {
    DEBUG_NOTE("Get LED 1 state = \"0\"");
    return "0";
  }
}
