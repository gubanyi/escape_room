/** Includes **/
#include <TemperatureZero.h>

#include "EscapeRoomEndNode.hpp"
#include "EscapeRoomActivationDevice.hpp"
#include "EscapeRoomTriggerDevice.hpp"



/** Includes **/
#define THIS_END_NODE_ID	1234



/** Function Declarations **/
String LED_setConfigFunc(String config);
String LED_setStateFunc(String state);
String LED_getStateFunc();

String Thermometer_setConfigFunc(String config);
String Thermometer_getStateFunc();



/** Global Variables **/
// Activation Devices
EscapeRoomActivationDevice LED1_activation("LED", &LED_setConfigFunc, &LED_getStateFunc, &LED_setStateFunc);

// Trigger Devices
EscapeRoomTriggerDevice Temp1_trigger("TEMP", &Thermometer_setConfigFunc, &Thermometer_getStateFunc);

// End node class setup
const EscapeRoomActivationDevice* activationDevices[] = { &LED1_activation };
const int numActivationDevices = 1;
const EscapeRoomTriggerDevice* triggerDevices[] = { &Temp1_trigger };
const int numTriggerDevices = 1;
EscapeRoomEndNode node(THIS_END_NODE_ID, numActivationDevices, activationDevices, numTriggerDevices, triggerDevices);

String jsons[] = {
	"{\"SNID\":1,\"TNID\":1234,\"PID\":3,\"UT\":1000,\"AL\":[{\"ID\":1,\"T\":\"LED\",\"S\":\"1\"}]}",	// 1 Turns LED on
	"{\"SNID\":1,\"TNID\":1234,\"PID\":4,\"UT\":1000,\"AL\":[{\"ID\":1,\"T\":\"LED\",\"S\":\"0\"}]}",	// 2 Turns LED off
	"{\"SNID\":1,\"TNID\":20,\"PID\":1,\"UT\":1000,\"AL\":[{\"ID\":1,\"T\":\"LED\",\"S\":\"0\"}]}",	// 3 Not this target node ID
	"{\"SNID\":1,\"TNID\":1234,\"PID\":4,\"UT\":1000,\"AL\":[{\"ID\":1,\"T\":\"LED\",\"S\":\"0\"}]}",	// 4 Repeat of packet ID
	"{\"SNID\":1,\"TNID\":1234,\"PID\":5,\"UT\":1000,\"AL\":[{\"ID\":1,\"T\":\"LED\"}]}",	// 5 No state data (request to send state)
	"{\"SNID\":1,\"TNID\":1234,\"PID\":7,\"UT\":1000,\"AL\":[{\"ID\":2,\"T\":\"LED\",\"S\":\"0\"}]}",	// 6 Not this activator device ID
	"{\"SNID\":1,\"TNID\":1234,\"PID\":8,\"UT\":1000,\"AL\":[{\"ID\":1,\"T\":\"SO\",\"S\":\"0\"}]}",	// 7 Not this activator device type
	"{\"SNID\":1,\"TNID\":1234,\"PID\":10,\"UT\":1000,\"AL\":[{\"ID\":1,\"T\":\"LED\",\"S\":\"0\"}]}",	// 8 Skip packet ID
	"{\"SNID\":1,\"TNID\":1234,\"PID\":11,\"UT\":1000,\"AL\":[{\"ID\":1,\"T\":\"LED\",\"S\":\"1\"}]}",	// 9 Turns LED on
	"{\"SNID\":1,\"TNID\":1234,\"PID\":12,\"UT\":1000,\"AL\":[{\"ID\":1,\"T\":\"LED\",\"S\":\"1\"}]",	// 10 Improperly formatted JSON
	"{\"SNID\":1,\"TNID\":1234,\"PID\":12,\"UT:1000,\"AL\":[{\"ID\":1,\"T\":\"LED\",\"S\":\"1\"}]}",	// 11 Improperly formatted JSON
	"{\"SNID\":1,\"TNID\":1234,\"PID\":12,\"UT\":1000,\"AL\":[{\"ID\":1,\"T\":\"LED\",\"S\":\"1\"},]}",	// 12 Improperly formatted JSON
	"{\"SNID\":1,\"TNID\":1234,\"PID\":12,\"UT\":1000,\"TL\":[{\"ID\":1,\"T\":\"TEMP\"}]}",	// 13 Request to send TEMP data
	"{\"SNID\":1,\"TNID\":1234,\"PID\":14,\"UT\":1000,\"AL\":[{\"ID\":1,\"T\":\"LED\",\"S\":\"2\"}]}",	// 14 Try to set invalid state to LED
	"{\"SNID\":1,\"TNID\":1234,\"PID\":16,\"UT\":1000, \"CMD\":\"START\"}",	// 15 Send a command
	"{\"SNID\":1,\"TNID\":1234,\"PID\":17,\"UT\":1000,\"TL\":[{\"ID\":1,\"T\":\"TEMP\",\"C\":\">38.2\"}]}",	// 16 Set TEMP config as a temperature threshold
};
#define NUM_JSONS	16

TemperatureZero TempZero = TemperatureZero();
float tempThreshold = 100;




/** Main Functions **/
void setup()
{
	// Note: the heartbeat can be implemented by waiting for the heartbeat timeout to expire after the previous TX timestamp, using the EscapeRoomEndNode::GetPreviousTxTimestamp() method. If that timeout elapses, the heartbeat is sent by simply calling EscapeRoomEndNode::CreateTxJsonStr(false) and sending the string. It will try to send the smallest possible packet, but will include any new & unsent sensor data as well.
	
	SerialUSB.begin(115200);
	while (!SerialUSB) {}
	SerialUSB.println("**********Begin test**********");
	SerialUSB.println();

	// Send the state of all devices (this will include an error in the temperature reading, since the TempZero object hasn't been initialized yet!)
	SerialUSB.println("Test sending the initial TX JSON...");
	SerialUSB.println("Initial TX JSON:" + node.CreateTxJsonStr(true));
	SerialUSB.println();

	// Wait until temperature is not nan, then send the state of all devices again
	TempZero.init();
	float temperature;
	do
	{
		temperature = TempZero.readInternalTemperature();
	} while (temperature != temperature);
	SerialUSB.println("Second TX JSON:" + node.CreateTxJsonStr(true));
	SerialUSB.println();

	// Cycle through all the RX strings to receive
	for (int i = 0; i < NUM_JSONS; i++)
	{
		SerialUSB.println("Prossessing JSON string #" + String(i + 1));
		SerialUSB.println(jsons[i]);
		node.ParseRxJsonStr(jsons[i]);
		SerialUSB.println();

		// Was a command received?
		if (node.NewCommandFromCloudAvailable())
		{
			SerialUSB.println("Received command: " + node.GetNewCommandFromCloud());
			SerialUSB.println();
		}

		// Were any of the activation devices or trigger devices requested for an update? If so, create a responding TX JSON and print it
		if (node.GetAnySendStateIsRequested())
		{
			SerialUSB.println("Requested TX JSON: " + node.CreateTxJsonStr());
			SerialUSB.println();
		}
	}

	// Send heartbeat after timeout
	while (millis() < node.GetPreviousTxTimestamp() + 5000) {}

	SerialUSB.println("Heartbeat timeout occurred. Last TX was at T=" + String(node.GetPreviousTxTimestamp()) + ", current T=" + String(millis()) + ". Sending heartbeat TX JSON packet...");
	SerialUSB.println("Heartbeat TX JSON: " + node.CreateTxJsonStr());
	SerialUSB.println();

	// Wait for the thermometer to exceed its threshold
	while (TempZero.readInternalTemperature() < tempThreshold) {}
	
	// Queue the temperature for sending
	Temp1_trigger.SendStateOnNextTransfer();
	tempThreshold = 100; // Disable the threshold, just in case

	SerialUSB.println("Event-driven TX JSON: " + node.CreateTxJsonStr());
	SerialUSB.println();

	// Send heartbeat after timeout
	while (millis() < node.GetPreviousTxTimestamp() + 5000) {}

	SerialUSB.println("Heartbeat timeout occurred. Last TX was at T=" + String(node.GetPreviousTxTimestamp()) + ", current T=" + String(millis()) + ". Sending heartbeat TX JSON packet...");
	SerialUSB.println("Heartbeat TX JSON: " + node.CreateTxJsonStr());
	SerialUSB.println();
}

void loop()
{
	
}



/** Function Definitions **/
String LED_setConfigFunc(String config)
{
	// The LED has no configuration
	return "";
}

String LED_setStateFunc(String state)
{
	if (state == "0")
	{
		pinMode(PIN_LED_13, OUTPUT);
		digitalWrite(PIN_LED_13, LOW);
		DEBUG_NOTE("Set blue STATUS LED state to \"0\"");
		return "0";
	}
	else if (state == "1")
	{
		pinMode(PIN_LED_13, OUTPUT);
		digitalWrite(PIN_LED_13, HIGH);
		DEBUG_NOTE("Set blue STATUS LED state to \"1\"");
		return "1";
	}
	else
	{
		DEBUG_ERROR("Set blue STATUS LED state: Invalid state \"" + state + "\"");
		return "ER: invalid state";
	}
}

String LED_getStateFunc()
{
	if (digitalRead(PIN_LED_13))
	{
		DEBUG_NOTE("Get blue STATUS LED state = \"1\"");
		return "1";
	}
	else
	{
		DEBUG_NOTE("Get blue STATUS LED state = \"0\"");
		return "0";
	}
}

String Thermometer_setConfigFunc(String config)
{
	if (config.startsWith(">"))
	{
		tempThreshold = config.substring(1).toFloat();
	}
}

String Thermometer_getStateFunc()
{
	float temperature = TempZero.readInternalTemperature();

	if (temperature != temperature)
	{
		// Result is NAN, send error
		DEBUG_ERROR("Get temperature state: \"nan\"");
		return "ER: nan";
	}

	String temperatureString = String(temperature, 2);
	DEBUG_NOTE("Get temperature state = \"" + temperatureString + "\"");
	return temperatureString;
}