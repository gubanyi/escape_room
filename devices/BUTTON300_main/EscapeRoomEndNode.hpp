#pragma once

/** Includes **/
#include <Arduino.h>
#include <ArduinoJson.h>

#include "EscapeRoomHeader.h"
#include "EscapeRoomActivationDevice.hpp"
#include "EscapeRoomTriggerDevice.hpp"



/** Classes **/
class EscapeRoomEndNode
{
public:
	EscapeRoomEndNode(int id, unsigned int numActivationDevices, const EscapeRoomActivationDevice** activationDevices, unsigned int numTriggerDevices, const EscapeRoomTriggerDevice** triggerDevices);
	int GetID();
	unsigned int GetUptimeMillis();
	unsigned int GetUptimeSeconds();
	unsigned int GetPreviousTxTimestamp();
	String ParseRxJsonStr(String rxStr);
	String CreateTxJsonStr(bool sendStateOfAll=false);
	bool GetAnySendStateIsRequested();
	bool NewCommandFromCloudAvailable();
	String GetNewCommandFromCloud();
	
	String ErrorString;


	unsigned int NumActivationDevices;
	EscapeRoomActivationDevice** ActivationDevices;

	unsigned int NumTriggerDevices;
	EscapeRoomTriggerDevice** TriggerDevices;
	
private:
	unsigned int ID;
	unsigned int PacketID;
	unsigned int PreviousTxTimestamp;
	String CommandFromCloud;
};
