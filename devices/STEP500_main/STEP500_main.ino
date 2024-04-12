/** Includes **/
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#include "EscapeRoomEndNode.hpp"
#include "EscapeRoomActivationDevice.hpp"
#include "EscapeRoomTriggerDevice.hpp"



/** Defines **/
#define THIS_END_NODE_ID	7
#define NETWORK_FREQUENCY	903900000
#define HEARTBEAT_DELAY_MS	2000
#define DEBOUNCE_PERIOD		5



/** Function Declarations **/
String Step1_setConfigFunc(String config);
String Step1_getStateFunc();

// These callbacks are only used in over-the-air activation, so they are left empty here (we cannot leave them out completely unless DISABLE_JOIN is set in arduino-lmic/project_config/lmic_project_config.h, otherwise the linker will complain). Do not change these.
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }
void onEvent (ev_t ev) { }

static void tx_func(osjob_t* job);



/** Global Variables **/
// Trigger Devices
EscapeRoomTriggerDevice Step1_trigger("STEP", &Step1_setConfigFunc, &Step1_getStateFunc);

// End node class setup
const EscapeRoomActivationDevice* activationDevices[] = { };
const int numActivationDevices = 0;
const EscapeRoomTriggerDevice* triggerDevices[] = { &Step1_trigger };
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
unsigned int nextDebounceMillis = 0;
unsigned int Step1PrevReading = 1;
volatile unsigned int step1State = 1;

bool txing = false;


/** Main Functions **/
void setup()
{
	SerialUSB.begin(115200);
	//while (!SerialUSB) {}
	SerialUSB.println("**********Step Trigger**********");
	SerialUSB.println();
	
	// Did a WDT reset cause the current boot? p.147
	if (PM->RCAUSE.reg & (1 << 5))
	{
		node.ErrorString = "WDT caused reset";
		DEBUG_ERROR(node.ErrorString);
	}
	else
	{
		node.ErrorString = "";
	}
	
	// Set up switched outlet defaults
	pinMode(2, INPUT_PULLUP);	// Step1

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
	
	// Disable WDT, wait for it to actually be disabled
	WDT->CTRL.bit.ENABLE = 0;
	while (WDT->STATUS.bit.SYNCBUSY) {}

	// Configure clock generator 7 to be 2,048 Hz
	GCLK->GENDIV.reg = 0
		| GCLK_GENDIV_ID(7)		// Select clock generator 7 (this register works in a weird way, but whatever)
		| GCLK_GENDIV_DIV(3)	// Division factor is /2^(DIV + 1), where DIV is the variable in this macro: GCLK_GENDIV_DIV(DIV). Hence, since we want the clock rate to be 2,048 Hz, and the clock source is 32,768 Hz, we need DIV to be 3 as in 2,048 = 32,768 / 2^(3 + 1)
	;
	while (GCLK->STATUS.bit.SYNCBUSY) {}

	// Configure clock generator 7 to select the 32.768 kHz clock source, use the division factor, and enable it.
	GCLK->GENCTRL.reg = 0
		| GCLK_GENCTRL_ID(7)		// Select clock generator 7
		| GCLK_GENCTRL_GENEN		// Enable clock generator
		| GCLK_GENCTRL_SRC_OSCULP32K	// Select 32 kHz clock source
		| GCLK_GENCTRL_DIVSEL	// DIV is used as a power of 7 rather than a linear division factor
	;

	// Wait for clock generator 7 settings to be latched. This takes time because it may have been at a low frequency before, and so it may not latch the updated register values for a while.
	while (GCLK->STATUS.bit.SYNCBUSY) {}

	// Attach clock generator 7 to the watchdog timer
	GCLK->CLKCTRL.reg = 0
		| GCLK_CLKCTRL_ID_WDT		// Select the WDT clock
		| GCLK_CLKCTRL_CLKEN		// Enable the WDT clock
		| GCLK_CLKCTRL_GEN_GCLK7	// Attach clock generator 7 as the source of the WDT clock
	;
	while (GCLK->STATUS.bit.SYNCBUSY) {}

	// Configure WDT period to 2 seconds, disable early warning interrupt, disable window mode
	WDT->CONFIG.bit.PER = 0xB;	// Set period to 8 seconds
	WDT->INTENCLR.bit.EW = 1;	// Disable early warning interrupt
	WDT->CTRL.bit.WEN = 0;		// Disable window mode

	// Re-enable WDT
	WDT->CTRL.bit.ENABLE = 1;

	// Get initial state of step trigger
	Step1PrevReading = digitalRead(2);
	step1State = Step1PrevReading;
}

void loop()
{
	// Kick that old fiend, that insideous reaper with fathomless patience, awaiting man's inevitable stumbling with insatiable hunger, the watchdog timer
	WDT->CLEAR.reg = 0xA5;
	
	// Is it time to debounce the input pins?
	if (millis() >= nextDebounceMillis)
	{
		// Debounce the pin
		bool Step1CurrentReading = digitalRead(2);

		nextDebounceMillis = millis() + DEBOUNCE_PERIOD;
		
		if (Step1CurrentReading == Step1PrevReading)
		{
			// Debounced successfully

			// Is the new reading going to change the state of the input?
			if ((Step1CurrentReading == false) && (Step1CurrentReading != step1State))
			{
				Step1_trigger.SendStateOnNextTransfer();
				//nextDebounceMillis = millis() + 1000;
			}
			step1State = Step1CurrentReading;
		}
		else
		{
			// Currently bouncing...
		}

		Step1PrevReading = Step1CurrentReading;
		
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
	if (txing)
	{
		return;
	}
	txing = true;
	
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
	txing = false;
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
String Step1_setConfigFunc(String config)
{
	// The step trigger has no configuration
	return "";
}

String Step1_getStateFunc()
{
	if (!digitalRead(2))
	{
		DEBUG_NOTE("Get Step 1 state = \"1\"");
		return "1";
	}
	else
	{
		DEBUG_NOTE("Get Step 1 state = \"0\"");
		return "0";
	}
}
