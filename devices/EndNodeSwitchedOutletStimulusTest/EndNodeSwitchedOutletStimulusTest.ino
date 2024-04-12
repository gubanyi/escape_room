/** Includes **/
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#include "EscapeRoomEndNode.hpp"
#include "EscapeRoomActivationDevice.hpp"
#include "EscapeRoomTriggerDevice.hpp"



/** Defines **/
#define THIS_END_NODE_ID	100
#define NETWORK_FREQUENCY	903900000
#define HEARTBEAT_DELAY_MS	60000



/** Function Declarations **/
// These callbacks are only used in over-the-air activation, so they are left empty here (we cannot leave them out completely unless DISABLE_JOIN is set in arduino-lmoc/project_config/lmic_project_config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }
void onEvent (ev_t ev) { }

static void tx_func (osjob_t* job);



/** Global Variables **/
// Activation Devices

// End node class setup
const EscapeRoomActivationDevice* activationDevices[] = {};
const int numActivationDevices = 0;
const EscapeRoomTriggerDevice* triggerDevices[] = {};
const int numTriggerDevices = 0;
EscapeRoomEndNode node(THIS_END_NODE_ID, numActivationDevices, activationDevices, numTriggerDevices, triggerDevices);

// Pin mapping for SAMD21
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
	SerialUSB.begin(115200);
	//while (!SerialUSB) {}
	SerialUSB.println("**********Switched Outlet Stimulus Test**********");
	SerialUSB.println();

	// Send the state of all devices (this will include an error in the temperature reading, since the TempZero object hasn't been initialized yet!)
	SerialUSB.println("Test sending the initial TX JSON...");
	SerialUSB.println("Initial TX JSON:" + node.CreateTxJsonStr(true));
	SerialUSB.println();

	// Initialize runtime env
	os_init();

	// Set up LMIC wireless channel
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
int packetID = 2;
static void tx_func(osjob_t* job)
{
	static int outlet1on = true;

	// Construct JSON packet
	String json = "{\"SNID\":1,\"TNID\":4,\"PID\":" + String(packetID) + ",\"UT\":1,\"AL\":[{\"ID\":1,\"T\":\"SO\",\"S\":\"" + String(outlet1on) + "\"}]}";

	// Send the packet
	tx(json.c_str(), json.length(), txdone_func);
	DEBUG_NOTE("Sending packet \"" + json + "\"...");
	
	outlet1on = !outlet1on;
	packetID++;

	// Reschedule job every HEARTBEAT_DELAY_MS (plus a bit of random to prevent systematic collisions), which will be overridden if another packet must be transmitted before that.
	os_setTimedCallback(job, os_getTime() + ms2osticks(5000), tx_func);
}

static void rx_func(osjob_t* job)
{
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
	// Clear errors after they have been transmitted
	node.ErrorString = "";

	DEBUG_NOTE("Finished sending packet");
}

static void rxtimeout_func(osjob_t* job)
{
	// Not sure what this really is supposed to do...
	return;
}

void rx(osjobcb_t func)
{
	// This resets the radio to begin listening again. This function does not need to be modified
	LMIC.osjob.func = func;	// Set the callback for the next reception
	LMIC.rxtime = os_getTime(); // RX _now_
	// Enable "continuous" RX (e.g. without a timeout, still stops after receiving a packet)
	os_radio(RADIO_RXON);
}
