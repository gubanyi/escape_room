/** Includes **/
#include <Arduino.h>

#include "EscapeRoomActivationDevice.hpp"



/** Constructors **/
EscapeRoomActivationDevice::EscapeRoomActivationDevice(String type, String (*setConfigFunc)(String), String (*getStateFunc)(void), String (*setStateFunc)(String state)) : EscapeRoomTriggerDevice(type, setConfigFunc, getStateFunc)
{
	/*
	Constructor
	Arguments:
		@type (String): A short string describing the type of activation device. (e.g. for a button, this could be "BUT", or for a string of candles could be "LEDs"). All devices with the same type should behave the same way programmatically.
		@getStateFunc (function pointer with args (void) and return type String): A function pointer that returns the current state of the activation device as a string. If there was an error while reading the state of the activation device, the return string must be prefixed by "ER:" followed by an error message.
		@setStateFunc (function pointer with args (String state) and return type String): A function pointer that sets the current state of the activation device using a string as the input. The function pointer must be able to parse the string to determine what state to set the activation device to. If the state is set successfully, the function pointer must return "". If there was an error while setting the state of the activation device, the return string must be prefixed by "ER:" followed by an error message, and the function must make every effort to return the state of the activation device to how it was before the function was called.
	Returns: (no return for a constructor)
	*/
	_setStateFunc = setStateFunc;
}



/** Class Methods **/
String EscapeRoomActivationDevice::SetState(String state)
{
	if (_setStateFunc == nullptr)
	{
		// Generate the error string
		ErrorString = "null SetStateFunc";

		// Set the state as unknown
		MostRecentState = "?";

		// Queue up a reply to the cloud server to indicate an error occurred while setting the state
		SendStateOnNextTransfer();

		return "ER: " + ErrorString;
	}

	String newState = _setStateFunc(state);
	if (newState.startsWith("ER:"))
	{
		// Generate the error string
		ErrorString = newState.substring(4);
		
		// Set the state as unknown
		MostRecentState = "?";
		
		// Queue up a reply to the cloud server to indicate an error occurred while setting the state
		SendStateOnNextTransfer();
		
		return newState;
	}

	ErrorString = "";
	MostRecentState = newState;
	return MostRecentState;
}
