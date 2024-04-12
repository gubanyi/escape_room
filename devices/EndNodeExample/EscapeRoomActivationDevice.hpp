#pragma once

/** Includes **/
#include <Arduino.h>
#include "EscapeRoomHeader.h"
#include "EscapeRoomTriggerDevice.hpp"



/** Classes **/
class EscapeRoomActivationDevice : public EscapeRoomTriggerDevice
{
public:
	EscapeRoomActivationDevice(String type, String (*setConfigFunc)(String), String (*getStateFunc)(void), String (*setStateFunc)(String state));

	String SetState(String state);

private:
	String (*_setStateFunc)(String state);	// Function pointer to a function that sets the state of the activation device by parsing the string "state". Must return "" if no error occured; must return "ER: " + errorString if an error occurred
};
