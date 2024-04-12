/** Includes **/
#include <Arduino.h>

#include "EscapeRoomTriggerDevice.hpp"



/** Constructors **/
EscapeRoomTriggerDevice::EscapeRoomTriggerDevice(String type, String (*setConfigFunc)(String), String (*getStateFunc)(void))
{
	/*
	Constructor
	Arguments:
		@type (String): A short string describing the type of trigger device. (e.g. for a button, this could be "BUT", or for a string of candles could be "LEDs"). All devices with the same type should behave the same way programmatically.
		@setConfigFunc (function pointer with args (String) and return type String): A function pointer that sets the config of the trigger device using a string as the input. The function pointer must be able to parse the string to determine what config to use. If the config setup is successful, the function pointer must return "". If there was an error while setting the config of the activation device, the return string must be prefixed by "ER:" followed by an error message, and the config must not change.
		@getStateFunc (function pointer with args (void) and return type String): A function pointer that returns the current state of the trigger device as a string. If there was an error while reading the state of the trigger device, the return string must be prefixed by "ER:" followed by an error message.
	Returns: (no return for a constructor)
	*/
	Type = type;
	ID = -1;
	SendStateOnNextTransfer();	// Set this to true to send the initial state of the trigger device when a connection is established 
	_setConfigFunc = setConfigFunc;
	_getStateFunc = getStateFunc;
}



/** Class Methods **/
bool EscapeRoomTriggerDevice::SetID(int id)
{
	if (ID >= 0 || id < 1)
	{
		return false;
	}
	
	ID = id;
	return true;
}

int EscapeRoomTriggerDevice::GetID()
{
	return ID;
}

String EscapeRoomTriggerDevice::GetType()
{
	return Type;
}

String EscapeRoomTriggerDevice::GetErrorString()
{
	return ErrorString;
}

String EscapeRoomTriggerDevice::SetConfig(String config)
{
	if (_setConfigFunc == nullptr)
	{
		// Generate the error string
		ErrorString = "null SetConfigFunc";

		// Queue up a reply to the cloud server to indicate an error occurred while setting the state
		SendStateOnNextTransfer();

		return "ER: " + ErrorString;
	}

	String er = _setConfigFunc(config);
	if (er.startsWith("ER:"))
	{
		// Generate the error string
		ErrorString = er.substring(4);
		
		// Queue up a reply to the cloud server to indicate an error occurred while setting the state
		SendStateOnNextTransfer();
		
		return er;
	}

	return "";
}

String EscapeRoomTriggerDevice::GetState()
{
	if (_getStateFunc == nullptr)
	{
		// Generate the error string
		ErrorString = "null GetStateFunc";
		
		// Set the state as unknown
		MostRecentState = "?";

		// Queue up a reply to the cloud server to indicate an error occurred while setting the state
		SendStateOnNextTransfer();

		return "ER: " + ErrorString;
	}

	String newState = _getStateFunc();
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

String EscapeRoomTriggerDevice::GetStateAndClearRequest()
{
	String ret = GetState();
	SendStateIsRequested = false;
	return ret;
}

void EscapeRoomTriggerDevice::SendStateOnNextTransfer()
{
	SendStateIsRequested = true;
}

bool EscapeRoomTriggerDevice::GetSendStateIsRequested()
{
	return SendStateIsRequested;
}
