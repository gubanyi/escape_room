/** Includes **/
#include <Arduino.h>
#include <string.h>

#include "EscapeRoomEndNode.hpp"



/** Constructors **/
EscapeRoomEndNode::EscapeRoomEndNode(int id, unsigned int numActivationDevices, const EscapeRoomActivationDevice** activationDevices, unsigned int numTriggerDevices, const EscapeRoomTriggerDevice** triggerDevices)
{
	ID = id;
	PacketID = 0;
	PreviousTxTimestamp = 0;
	CommandFromCloud = "";

	NumActivationDevices = numActivationDevices;
	ActivationDevices = (EscapeRoomActivationDevice**)activationDevices;

	NumTriggerDevices = numTriggerDevices;
	TriggerDevices = (EscapeRoomTriggerDevice**)triggerDevices;
	
	int i;
	for (i = 0; i < numActivationDevices; i++)
	{
		ActivationDevices[i]->SetID(i + 1);
	}

	int triggerDeviceOffset = i;
	for (i = triggerDeviceOffset; i < (numTriggerDevices + triggerDeviceOffset); i++)
	{
		TriggerDevices[i]->SetID(i + 1);
	}
}



/** Class Methods **/
int EscapeRoomEndNode::GetID()
{
	return ID;
}

unsigned int EscapeRoomEndNode::GetUptimeMillis()
{
	return millis();
}

unsigned int EscapeRoomEndNode::GetUptimeSeconds()
{
	return GetUptimeMillis() / 1000;
}

unsigned int EscapeRoomEndNode::GetPreviousTxTimestamp()
{
	return PreviousTxTimestamp;
}

String EscapeRoomEndNode::ParseRxJsonStr(String rxStr)
{
	DEBUG_NOTE("Beginning to deserialize RX JSON string...");

	// Create the JSON data type and deserialize it
	StaticJsonDocument<1024> doc;
	DeserializationError deserializationError = deserializeJson(doc, rxStr);

	// Was there a deserialization error?
	if (deserializationError)
	{
		ErrorString = deserializationError.c_str();
		DEBUG_ERROR("Deserialization " + ErrorString);
		return "ER: " + ErrorString;
	}

	DEBUG_NOTE("JSON string has been deserialized");

	// Convert to a JSON object to navigate the dictionary
	JsonObject obj = doc.as<JsonObject>();

	// A JSON packet from the server to an end node has the following structure:
	// {
	//	"SNID": int (the packet source node ID)
	//	"TNID": int (the packet target node ID)
	//	"PID": int (the packet ID. Every device hears every packet sent, and each packet has a packet ID +1 of the previous packet)
	//	"UT": unsigned int (the uptime in milliseconds)
	//	"CMD": (optional, only from server to end node) string (a command from the cloud to the end node, such as "START" or "STOP" to begin or end the escape room)
	//	"AL": (optional) list (the list of activation devices that need to be set to a new state)
	//	"TL": (optional) list (the list of trigger devices that are requested to send their current status)
	//	"ER": (optional, only from end node to server) string (contains a general end node error that isn't specific to one of the activation/trigger devices)
	// }

	// An activation device JSON packet, contained within the "AL" list, has the following structure:
	// {
	//	"ID": int (the activation device ID)
	//	"T": string (the activation device type)
	//	"C": (optional, only from server to end node) string (sets up the activation device configuration)
	//	"S": (optional) string (the current state or the desired state to set the activation device to)
	//	"ER": (optional, only from end node to server) string (contains an error specific to the activation device)
	// }

	// A trigger device JSON packet, contained within the "TL" list, has the following structure:
	// {
	//	"ID": int (the trigger device ID)
	//	"T": string (the trigger device type)
	//	"C": (optional, only from server to end node) string (sets up the trigger device configuration)
	//	"S": (optional, only from end node to server) string (the current state of the trigger device)
	//	"ER": (optional, only from end node to server) string (contains an error specific to the activation device)
	// }

	ErrorString = "";

	// Get the packet source node ID, of datatype int
	if (!obj.containsKey("SNID"))
	{
		ErrorString = "No SNID";
		DEBUG_ERROR("Parse RX packet: " + ErrorString);
		return "ER: " + ErrorString;
	}

	if (!obj["SNID"].is<int>())
	{
		ErrorString = "SNID invalid type";
		DEBUG_ERROR("Parse RX packet: " + ErrorString);
		return "ER: " + ErrorString;
	}

	int SNID = obj["SNID"];

	// Get the packet target node ID, of datatype int
	if (!obj.containsKey("TNID"))
	{
		ErrorString = "No TNID";
		DEBUG_ERROR("Parse RX packet: " + ErrorString);
		return "ER: " + ErrorString;
	}

	if (!obj["TNID"].is<int>())
	{
		ErrorString = "TNID invalid type";
		DEBUG_ERROR("Parse RX packet: " + ErrorString);
		return "ER: " + ErrorString;
	}

	int TNID = obj["TNID"];

	// Is this packet addressed to this node?
	if (TNID != ID)
	{
		// Packet not addressed to this node, quit listening
		DEBUG_NOTE("Parse RX packet: packet not addressed to this node (this node ID = " + String(ID) + ", desired ID = " + String(TNID) + ")");
		return "";
	}

	// Get the packet ID
	if (!obj.containsKey("PID"))
	{
		ErrorString = "No PID";
		DEBUG_ERROR("Parse RX packet: " + ErrorString);
		return "ER: " + ErrorString;
	}

	if (!obj["PID"].is<unsigned int>())
	{
		ErrorString = "PID invalid type";
		DEBUG_ERROR("Parse RX packet: " + ErrorString);
		return "ER: " + ErrorString;
	}

	unsigned int PID = obj["PID"];

	// Get the uptime of the packet source
	if (!obj.containsKey("UT"))
	{
		ErrorString = "No UT";
		DEBUG_ERROR("Parse RX packet: " + ErrorString);
		return "ER: " + ErrorString;
	}

	if (!obj["UT"].is<unsigned int>())
	{
		ErrorString = "UT invalid type";
		DEBUG_ERROR("Parse RX packet: " + ErrorString);
		return "ER: " + ErrorString;
	}

	unsigned int UT = obj["UT"];

	// Did we get the expected packet ID?
	if (PID < (PacketID + 1))
	{
		ErrorString = "Received packet ID " + String(PID) + ", which has already been received before";
		DEBUG_ERROR("Parse RX packet: " + ErrorString);
		return "ER: " + ErrorString;
	}
	else if (PID > (PacketID + 1))
	{
		ErrorString = "Missed packets " + String(PacketID + 1) + " to " + String(PID - 1);
		DEBUG_WARNING("Parse RX packet: " + ErrorString);
		// Continue parsing
	}
	
	PacketID = PID;

	// This packet was addressed, begin parsing the message
	DEBUG_NOTE("Parse RX packet: this node (ID = " + String(ID) + ") is addressed by the packet");

	// Is there a command in this message?
	if (obj.containsKey("CMD"))
	{
		// There is a command in this message, get it
		if (!obj["CMD"].is<char*>())
		{
			ErrorString = "CMD invalid type";
			DEBUG_ERROR("Parse RX packet:" + ErrorString);
			return "ER: " + ErrorString;
		}

		// Get the command. The user will have to manually check to see if there is a command or not
		CommandFromCloud = obj["CMD"].as<char*>();
		DEBUG_NOTE("Parse RX Packet: Got command " + CommandFromCloud);
	}

	// Parse the activation devices list, if there is one
	if (obj.containsKey("AL"))
	{
		// Ensure the activation devices list is indeed of datatype list
		if (obj["AL"].is<JsonArray>())
		{
			// Cycle through each item in the activation devices list
			for (JsonObject ad: obj["AL"].as<JsonArray>())
			{
				// Get the ID of the activation device, of datatype int
				if (!ad.containsKey("ID"))
				{
					ErrorString = "No AD ID";
					DEBUG_ERROR("Parse RX packet AD: " + ErrorString);
					return "ER: " + ErrorString;
				}

				if (!ad["ID"].is<int>())
				{
					ErrorString = "AD ID invalid type";
					DEBUG_ERROR("Parse RX packet AD: " + ErrorString);
					return "ER: " + ErrorString;
				}

				int ADID = ad["ID"];
				
				// Get the type of the activation device, of datatype String
				if (!ad.containsKey("T"))
				{
					ErrorString = "No AD T";
					DEBUG_ERROR("Parse RX packet AD: " + ErrorString);
					return "ER: " + ErrorString;
				}

				if (!ad["T"].is<char*>())
				{
					ErrorString = "AD T invalid type";
					DEBUG_ERROR("Parse RX packet AD: " + ErrorString);
					return "ER: " + ErrorString;
				}

				String ADT = ad["T"];

				// Does this data match any of the activation devices in this node?
				for (int i = 0; i < NumActivationDevices; i++)
				{
					if (ActivationDevices[i]->GetID() != ADID)
					{
						continue;
					}
					if (ActivationDevices[i]->GetType() != ADT)
					{
						continue;
					}

					// Matched ID and type of activation device!
					DEBUG_NOTE("Parse RX packet AD: this device's AD (ID " + String(ADID) + ") is addressed");

					// Get the config, if there is one
					if (ad.containsKey("C"))
					{
						// Make sure the desired state is of datatype string
						if (!ad["C"].is<char*>())
						{
							ErrorString = "AD C invalid type";
							DEBUG_ERROR("Parse RX packet AD: " + ErrorString);
							break;	// Continue processing other stuff
						}

						String er = ActivationDevices[i]->SetConfig(ad["C"]);
						if (er.startsWith("ER:"))
						{
							// An error occurred while setting the config of the activation device
							ErrorString = "AD " + String(ADID) + " " + er.substring(4);
							DEBUG_ERROR("Parse RX packet AD: " + ErrorString);
							continue;
						}
						else
						{
							DEBUG_NOTE("Parse RX packet AD: successfully set config of AD (ID = " + String(ADID) + ", Type = " + String(ADT) + ") to config \"" + ad["C"].as<char*>() + "\"");
						}
					}

					// Get the desired state, if there is one
					if (ad.containsKey("S"))
					{
						// Make sure the desired state is of datatype string
						if (!ad["S"].is<char*>())
						{
							ErrorString = "AD S invalid type";
							DEBUG_ERROR("Parse RX packet AD: " + ErrorString);
							break;	// Continue processing other stuff
						}

						// Try to set the state of the activation device
						String adSetRet = ActivationDevices[i]->SetState(ad["S"]);

						if (adSetRet.startsWith("ER:"))
						{
							// An error occurred while setting the state of the activation device
							ErrorString = "AD " + String(ADID) + " " + adSetRet.substring(4);
							DEBUG_ERROR("Parse RX packet AD: " + ErrorString);
							continue;
						}
						else
						{
							DEBUG_NOTE("Parse RX packet AD: successfully set state of AD (ID = " + String(ADID) + ", Type = " + String(ADT) + ") to state \"" + ad["S"].as<char*>() + "\"");
						}
					}
					else
					{
						// There is no desired state in the dictionary, so this is a request to update the server with the current state of the activation device
						ActivationDevices[i]->SendStateOnNextTransfer();
						DEBUG_NOTE("Parse RX packet AD: AD ID " + String(ADID) + " is requested to send its state");
					}
				}
			}
		}
		else
		{
			ErrorString = "AL invalid type";
			DEBUG_ERROR("Parse RX packet: " + ErrorString);
		}
	}

	// Parse the trigger devices list, if there is one
	if (obj.containsKey("TL"))
	{
		// Ensure the trigger devices list is indeed of datatype list
		if (obj["TL"].is<JsonArray>())
		{
			// Cycle through each item in the trigger devices list
			for (JsonObject td: obj["TL"].as<JsonArray>())
			{
				// Get the ID of the trigger device, of datatype int
				if (!td.containsKey("ID"))
				{
					ErrorString = "No TD ID";
					DEBUG_ERROR("Parse RX packet TD: " + ErrorString);
					return "ER: " + ErrorString;
				}

				if (!td["ID"].is<int>())
				{
					ErrorString = "TD ID invalid type";
					DEBUG_ERROR("Parse RX packet TD: " + ErrorString);
					return "ER: " + ErrorString;
				}

				int TDID = td["ID"];
				
				// Get the type of the trigger device, of datatype String
				if (!td.containsKey("T"))
				{
					ErrorString = "No TD T";
					DEBUG_ERROR("Parse RX packet TD: " + ErrorString);
					return "ER: " + ErrorString;
				}

				if (!td["T"].is<char*>())
				{
					ErrorString = "TD T invalid type";
					DEBUG_ERROR("Parse RX packet TD: " + ErrorString);
					return "ER: " + ErrorString;
				}

				String TDT = td["T"];

				// Does this data match any of the trigger devices in this node?
				for (int i = 0; i < NumActivationDevices; i++)
				{
					if (TriggerDevices[i]->GetID() != TDID)
					{
						continue;
					}
					if (TriggerDevices[i]->GetType() != TDT)
					{
						continue;
					}

					// Matched ID and type of trigger device!
					DEBUG_NOTE("Parse RX packet TD: this device's TD (ID " + String(TDID) + ") is addressed and is requested for an update");

					// Get the config, if there is one
					if (td.containsKey("C"))
					{
						// Make sure the desired state is of datatype string
						if (!td["C"].is<char*>())
						{
							ErrorString = "TD C invalid type";
							DEBUG_ERROR("Parse RX packet TD: " + ErrorString);
							break;	// Continue processing other stuff
						}

						String er = TriggerDevices[i]->SetConfig(td["C"]);
						if (er.startsWith("ER:"))
						{
							// An error occurred while setting the config of the activation device
							ErrorString = "TD " + String(TDID) + " " + er.substring(4);
							DEBUG_ERROR("Parse RX packet TD: " + ErrorString);
							continue;
						}
						else
						{
							DEBUG_NOTE("Parse RX packet TD: successfully set config of TD (ID = " + String(TDID) + ", Type = " + String(TDT) + ") to config \"" + td["C"].as<char*>() + "\"");
						}
					}
					
					// Receiving a trigger device like this means there is a request for the data of this trigger device, so transmit the current state on the next TX packet
					TriggerDevices[i]->SendStateOnNextTransfer();
					
				}
			}
		}
		else
		{
			ErrorString = "AL invalid type";
			DEBUG_ERROR("Parse RX packet: " + ErrorString);
		}
	}

	DEBUG_NOTE("Finished parsing RX packet");

	if (ErrorString.startsWith("ER:"))
	{
		return "ER: " + ErrorString;
	}

	return "";
}
// this is the string that is sent when triggerred (Rodoshi)
String EscapeRoomEndNode::CreateTxJsonStr(bool sendStateOfAll)
{
	DEBUG_NOTE("Beginning to serialize TX JSON string...");

	// Create the JSON data type and deserialize it
	StaticJsonDocument<1024> doc;

	// Add header data
	doc["SNID"] = ID;
	doc["TNID"] = 1;	// Since this device is an end node, the target is always the server
	PacketID++;	// Increment Packet ID
	doc["PID"] = PacketID;
	PreviousTxTimestamp = GetUptimeMillis();
	doc["UT"] = PreviousTxTimestamp;

	// Do any of the activation device states need to be sent?
	bool sendSomeActivationDevices = sendStateOfAll;
	if (!sendSomeActivationDevices)
	{
		for (int i = 0; i < NumActivationDevices; i++)
		{
			if (ActivationDevices[i]->GetSendStateIsRequested())
			{
				sendSomeActivationDevices = true;
				break;
			}
		}
	}

	if (sendSomeActivationDevices)
	{
		// Send the list of activation devices (but only those devices whose states have been requested)
		JsonArray AL = doc.createNestedArray("AL");

		// Add each requested device
		for (int i = 0; i < NumActivationDevices; i++)
		{
			if (sendStateOfAll || ActivationDevices[i]->GetSendStateIsRequested())
			{
				JsonObject ad = AL.createNestedObject();
				ad["ID"] = ActivationDevices[i]->GetID();
				ad["T"] = ActivationDevices[i]->GetType();

				String state = ActivationDevices[i]->GetStateAndClearRequest();
				if (state.startsWith("ER:"))
				{
					ad["S"] = "?";
					ad["ER"] = state.substring(4);
					DEBUG_ERROR("CreateTxJsonStr error getting state of AD ID " + String(ActivationDevices[i]->GetID()));
				}
				else
				{
					ad["S"] = state;
				}
			}
		}
	}

	// Do any of the trigger device states need to be sent?
	bool sendSomeTriggerDevices = sendStateOfAll;
	if (!sendSomeTriggerDevices)
	{
		for (int i = 0; i < NumTriggerDevices; i++)
		{
			if (TriggerDevices[i]->GetSendStateIsRequested())
			{
				sendSomeTriggerDevices = true;
				break;
			}
		}
	}

	if (sendSomeTriggerDevices)
	{
		// Send the list of trigger devices (but only those devices whose states have been requested)
		JsonArray TL = doc.createNestedArray("TL");

		// Add each requested device
		for (int i = 0; i < NumTriggerDevices; i++)
		{
			if (sendStateOfAll || TriggerDevices[i]->GetSendStateIsRequested())
			{
				JsonObject td = TL.createNestedObject();
				td["ID"] = TriggerDevices[i]->GetID();
				td["T"] = TriggerDevices[i]->GetType();

				String state = TriggerDevices[i]->GetStateAndClearRequest();
				if (state.startsWith("ER:"))
				{
					td["S"] = "?";
					td["ER"] = state.substring(4);
					DEBUG_ERROR("CreateTxJsonStr error getting state of TD ID " + String(TriggerDevices[i]->GetID()));
				}
				else
				{
					td["S"] = state;
					DEBUG_NOTE("State added");
				}
			}
		}
	}

	// Is there an error string to send?
	if (ErrorString.length() > 0)
	{
		doc["ER"] = ErrorString;
	}

	String jsonStr = "";
	serializeJson(doc, jsonStr);

	return jsonStr;
}

bool EscapeRoomEndNode::GetAnySendStateIsRequested()
{
	for (int i = 0; i < NumTriggerDevices; i++)
	{
		if (TriggerDevices[i]->GetSendStateIsRequested())
		{
			return true;
		}
	}
	
	for (int i = 0; i < NumActivationDevices; i++)
	{
		if (ActivationDevices[i]->GetSendStateIsRequested())
		{
			return true;
		}
	}

	return false;
}

bool EscapeRoomEndNode::NewCommandFromCloudAvailable()
{
	return (CommandFromCloud.length() > 0);
}

String EscapeRoomEndNode::GetNewCommandFromCloud()
{
	String cmd = CommandFromCloud;
	CommandFromCloud = "";
	return cmd;
}
