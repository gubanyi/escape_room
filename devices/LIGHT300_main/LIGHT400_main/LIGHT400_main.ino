// name will be LIGHT400_main.ino
// THE DRIVER FUNCTION BOTTOM FOR TRIGGER AND ACTIVATION:
/** Includes **/
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#include <Wire.h>
#include <BH1750.h>

#include "EscapeRoomEndNode.hpp"
#include "EscapeRoomActivationDevice.hpp"
#include "EscapeRoomTriggerDevice.hpp"



/** Defines **/
#define THIS_END_NODE_ID	3
#define NETWORK_FREQUENCY	903900000
#define HEARTBEAT_DELAY_MS	60000
BH1750 lightMeter;


/** Function Declarations **/
String Outlet1_setConfigFunc(String config);
String Outlet1_getStateFunc();

/** lightSensor state tracking **/
float prev_val = 0;
float current_val = 0;
float thres_val = 10;
int mode_state = 1;
int isTriggered = 0;
float lux = 0;
bool txing = false;

// These callbacks are only used in over-the-air activation, so they are left empty here (we cannot leave them out completely unless DISABLE_JOIN is set in arduino-lmic/project_config/lmic_project_config.h, otherwise the linker will complain). Do not change these.
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }
void onEvent (ev_t ev) { }

static void tx_func(osjob_t* job);



/** Global Variables **/
// Activation Devices
//EscapeRoomActivationDevice Outlet1_activation("SO", &Outlet1_setConfigFunc, &Outlet1_getStateFunc, &Outlet1_setStateFunc);

// need to change activation --> Trigger Devices, type 
EscapeRoomTriggerDevice light1_trigger("light", &LightS_setConfigFunc, &LightS_getStateFunc);


// End node class setup
const EscapeRoomActivationDevice* activationDevices[] = {};
const int numActivationDevices = 0;
// Set the pointer to the array here
const EscapeRoomTriggerDevice* triggerDevices[] = {&light1_trigger};
const int numTriggerDevices = 1;
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
// {"SNID":1,"TNID":4,"PID":2,"UT":1000,"AL":[{"ID":1,"T":"SO","S":"1"}]}	// Turns on outlet 1
// {"SNID":1,"TNID":4,"PID":3,"UT":1000,"AL":[{"ID":1,"T":"SO","S":"0"}]}	// Turns off outlet 1
// {"SNID":1,"TNID":4,"PID":4,"UT":1000,"AL":[{"ID":2,"T":"SO","S":"1"}]}	// Turns on outlet 2
// {"SNID":1,"TNID":4,"PID":5,"UT":1000,"AL":[{"ID":2,"T":"SO","S":"0"}]}	// Turns off outlet 2
// {"SNID":1,"TNID":4,"PID":6,"UT":1000,"AL":[{"ID":3,"T":"SO","S":"1"}]}	// Turns on outlet 3
// {"SNID":1,"TNID":4,"PID":7,"UT":1000,"AL":[{"ID":3,"T":"SO","S":"0"}]}	// Turns off outlet 3
// {"SNID":1,"TNID":4,"PID":8,"UT":1000,"AL":[{"ID":4,"T":"SO","S":"0"}]}	// Turns off outlet 4



/** Main Functions **/
void setup()
{
  // Initialize the I2C bus (BH1750 library doesn't do this automatically)
  Wire.begin();
  // On esp8266 you can select SCL and SDA pins using Wire.begin(D4, D3);
  // For Wemos / Lolin D1 Mini Pro and the Ambient Light shield use Wire.begin(D2, D1);

  lightMeter.begin();

	
	SerialUSB.begin(115200);
	//while (!SerialUSB) {}
	SerialUSB.println("**********Light Sensor**********");
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
  
  
	// write the sensor reading func here -- NAZIB

  lux = lightMeter.readLightLevel();
  float luxDiff = abs(abs(lux) - abs(prev_val));
  if (luxDiff > thres_val && (mode_state == 1)){
    isTriggered = 1;
    light1_trigger.SendStateOnNextTransfer();
    mode_state == 2;
   }
  else if(luxDiff > thres_val && (mode_state == 2)){
    mode_state = 1;
   }
   prev_val = lux;

	// Do we need to send anything? Could be requests to send state of triggers or activation devices
	if (node.GetAnySendStateIsRequested())
	{
		// Trigger a TX for the OS to handle
		os_setCallback(&txjob, tx_func);
	}

	// Run the OS callbacks, including scheduled jobs and events
	os_runloop_once();
  delay(100);
}



/** Function Definitions **/
static void tx_func(osjob_t* job)
{
  if (txing == true){
    return;
    }
    txing = true;
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
  txing = false;
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
	LMIC.osjob.func = func;	// Set the callback for the next reception
	LMIC.rxtime = os_getTime(); // RX _now_
	// Enable "continuous" RX (e.g. without a timeout, still stops after receiving a packet)
	os_radio(RADIO_RXON);
}



/** Driver Functions for trigger and activation devices **/
// You need to put your own driver functions here!
//Nazib : interrupt system to check the threshold of the light sensor. 
String LightS_setConfigFunc(String config)
{
	// The outlet has no configuration
	return "";
}


//REFLECT THE NAME OF THE TRIGGER DEVICES. MAYBE Light_getStateFunc()
String LightS_getStateFunc()
{
    String returnString = "Tel:";
    returnString += lux;
    returnString += ",";
    returnString += "Trig:";
    returnString += isTriggered;
    DEBUG_NOTE(returnString); 
    //chaning triggering functionality
    if (isTriggered == 1){
      isTriggered = 0;
      }
    return returnString;
//	if (!digitalRead(2))
//	{
//		DEBUG_NOTE("Get Outlet 1 state = \"1\"");
//		return "1";
//	}
//	else
//	{
//		DEBUG_NOTE("Get Outlet 1 state = \"0\"");
//		return "0";
//	}
}
