// Gateway USB to LoRa code
// Written by Sam Murray 12/14/2022

/** Includes **/
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>



/** Defines **/
#define NETWORK_FREQUENCY	903900000



/** Function Declarations **/

// These callbacks are only used in over-the-air activation, so they are left empty here (we cannot leave them out completely unless DISABLE_JOIN is set in arduino-lmic/project_config/lmic_project_config.h, otherwise the linker will complain). Do not change these.
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }
void onEvent (ev_t ev) { }



/** Global Variables **/
// Pin mapping for SAMD21 to LoRa modem. Do not change these
const lmic_pinmap lmic_pins = {
  .nss = 12,//RFM Chip Select
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 7,//RFM Reset
  .dio = {6, 10, 11}, //RFM Interrupt, RFM LoRa pin, RFM LoRa pin
};

osjob_t txjob;
osjob_t timeoutjob;


/** Main Functions **/
void setup()
{
	SerialUSB.begin(115200);

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
	
	// Start the RX portion of the radio, using this function as the callback
	rx(rx_func);
}

String serial_rx_str = "";

void loop()
{
	while (SerialUSB.available() > 0)
	{
		// Read the next received character
		char c = SerialUSB.read();
		
		// Is it the end of a line?
		if (c == '\n')
		{
			// Queue the full string received for sending string via LoRa
			tx(serial_rx_str.c_str(), serial_rx_str.length(), txdone_func);
			serial_rx_str = "";
			break;
		}
		else
		{
			// Append the character to the end of the string
			serial_rx_str += c;
		}
	}
	
	// Run the OS callbacks, including scheduled jobs and events
	os_runloop_once();
}



/** Function Definitions **/
static void rx_func(osjob_t* job)
{	
	// Copy the raw received data and convert it into a string
	String s = "";
	for (int i = 0; i < LMIC.dataLen; i++)
	{
		s += (char)LMIC.frame[i];
	}
	
	// Send the string via serial port
	SerialUSB.println(s);
	
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
	// Set up for RX
	rx(rx_func);
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
