#pragma once

/** Includes **/
#include <Arduino.h>
#include "EscapeRoomHeader.h"



/** Classes **/
class EscapeRoomTriggerDevice
{
public:
	EscapeRoomTriggerDevice(String type, String (*setConfigFunc)(String), String (*getStateFunc)(void));
	String GetType();
	bool SetID(int id);
	int GetID();
	String GetErrorString();

	String SetConfig(String config);
	String GetState();
	String GetStateAndClearRequest();
	
	void SendStateOnNextTransfer(); // This is the function that sends its state
	bool GetSendStateIsRequested();

protected:
	String (*_setConfigFunc)(String);	// Function pointer to a function that gets the state of the activation device by parsing the string "state". Must return stateString if no error occured; must return "ER: " + errorString if an error occurred
	String (*_getStateFunc)(void);	// Function pointer to a function that gets the state of the activation device by parsing the string "state". Must return stateString if no error occured; must return "ER: " + errorString if an error occurred

	String Type;
	int ID;
	bool SendStateIsRequested;
	String MostRecentState;
	String ErrorString;
};
