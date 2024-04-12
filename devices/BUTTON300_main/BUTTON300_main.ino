// constant variable initialized
const int LED = 13;
const int buttonPin = 3;

// couting initialized for button pushed
int Count = 0;

/** Includes **/
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#include "EscapeRoomEndNode.hpp"
#include "EscapeRoomActivationDevice.hpp"
#include "EscapeRoomTriggerDevice.hpp"

// RENAME OUTLET TO BUTTON IN THE WHOLE CODE

/** Defines **/
#define THIS_END_NODE_ID	5
#define NETWORK_FREQUENCY	903900000
#define HEARTBEAT_DELAY_MS	60000
#define DEBOUNCE_PERIOD    5



/** Function Declarations **/
String Button1_setConfigFunc(String config);
String Button1_getStateFunc();


// These callbacks are only used in over-the-air activation, so they are left empty here (we cannot leave them out completely unless DISABLE_JOIN is set in arduino-lmic/project_config/lmic_project_config.h, otherwise the linker will complain). Do not change these.
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }
void onEvent (ev_t ev) { }

static void tx_func(osjob_t* job);

/** Global Variables **/

bool txIng = false;
// Trigger device (WRITING FOR THE BUTTON TRIGGER DEVICE)
EscapeRoomTriggerDevice Button1_trigger("BUTN", &Button1_setConfigFunc, &Button1_getStateFunc);

// End node class setup // THIS PART NEED TO BE MODIFIED
const EscapeRoomActivationDevice* activationDevices[] = {};
const int numActivationDevices = 0;
const EscapeRoomTriggerDevice* triggerDevices[] = {&Button1_trigger};
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
unsigned int nextDebounceMillis = 0;
unsigned int Button1PrevReading = 1;
volatile unsigned int Button1State = 1;

// Test strings
// {"SNID":1,"TNID":4,"PID":2,"UT":1000,"AL":[{"ID":1,"T":"SO","S":"1"}]}	// Turns on outlet 1
// {"SNID":1,"TNID":4,"PID":3,"UT":1000,"AL":[{"ID":1,"T":"SO","S":"0"}]}	// Turns off outlet 1



/** Main Functions **/
void setup()
{
  SerialUSB.begin(115200);
  //while (!SerialUSB) {}
  SerialUSB.println("**********Push button**********");
  SerialUSB.println();

  // setup the push button default
   pinMode(buttonPin, INPUT_PULLUP);
   pinMode(LED, OUTPUT);


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

  //set the initial state of the pushbutton
  Button1PrevReading = digitalRead(buttonPin);
  Button1State = Button1PrevReading;
}

void loop()
{

  // Is it time to debounce the input pins?
  if (millis() >= nextDebounceMillis)
  {
  // Debounce the pin
  bool Button1CurrentReading = digitalRead(buttonPin);
  
  if(Button1CurrentReading == Button1PrevReading)
  {
    // Debounced successfully

      // Is the new reading going to change the state of the input?
      if (Button1CurrentReading != Button1State)
      {
        Button1_trigger.SendStateOnNextTransfer();
      }
      Button1State = Button1CurrentReading;
  }
  else
  {
    // Currently bouncing...
  }
  Button1PrevReading = Button1CurrentReading;
  nextDebounceMillis = millis() + DEBOUNCE_PERIOD;
  

  }

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
  if( txIng)
  {
    return;
    } 
	txIng = true;
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
  txIng = false;
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
	LMIC.osjob.func = func;	// Set the callback for the next reception
	LMIC.rxtime = os_getTime(); // RX _now_
	// Enable "continuous" RX (e.g. without a timeout, still stops after receiving a packet)
	os_radio(RADIO_RXON);
}



/** Driver Functions for trigger and activation devices **/
// You need to put your own driver functions here!
String Button1_setConfigFunc(String config)
{
	// The outlet has no configuration
	return "";
}

// MODIFY ACCORDINGLY
String Button1_getStateFunc()
{
  /*int buttonValue = digitalRead(buttonPin);
     if (buttonValue == LOW){
       // If button pushed, turn LED on
          digitalWrite(LED,HIGH);
          DEBUG_NOTE("Button is pushed");
          SerialUSB.println("Button is pushed");
          return "0";
        }
        
     else if (buttonValue == HIGH){
       // Otherwise, turn the LED off
       digitalWrite(LED, LOW);
       DEBUG_NOTE("Button is not pushed");
       SerialUSB.println("Button is not pushed");
       return "1";
     }
  */
	if (!digitalRead(buttonPin))
	{
  	DEBUG_NOTE("Get Button 1 state = \"1\""); // button is pushed
	  	return "1";
	}
	else
	{
		DEBUG_NOTE("Get Button 1 state = \"0\""); // button is not pushed
		return "0";
  	}
}
