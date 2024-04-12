#include <ArduinoJson.h>

#define SELF_NODE_ID			1234
#define SELF_NUM_TRIGGERS		2
#define SELF_TRIGGER_TYPES		{ "SWO", "LED" }
#define SELF_TRIGGER_IDS		{ 1, 2 }
#define SELF_NUM_ACTIVATIONS	1
#define SELF_ACTIVATION_TYPES	{ "LS" }
#define SELF_ACTIVATION_IDS		{ 2 }

String triggerTypes[] = SELF_TRIGGER_TYPES;
int triggerIDs[] = SELF_TRIGGER_IDS;
String activationTypes[] = SELF_ACTIVATION_TYPES;
int activationIDs[] = SELF_ACTIVATION_IDS;


void setup()
{
	SerialUSB.begin(115200);
	while (!SerialUSB) {}
	SerialUSB.println("Start JSON program");
	
	DynamicJsonDocument doc(1024);
	
	//char jsonStr[] = "{\"NID\":1234,\"PID\":2,\"UT\":500,\"As\":[{\"AT\":\"SWO\",\"ID\":1,\"State\":\"1\"},{\"AT\":\"LED\",\"ID\":2,\"State\":\"0\"}],\"Ts\":[{\"TT\":\"BUT\",\"ID\":1,\"State\":\"1\"},{\"TT\":\"LS\",\"ID\":2,\"State\":\"243\"}]}";
	char jsonStr[] = "{\"NID\":1234,\"PID\":2,\"UT\":500,\"As\":[{\"AT\":\"SWO\",\"ID\":1,\"State\":\"1\"},{\"AT\":\"LED\",\"ID\":2,\"State\":\"0\"},{\"AT\":\"SWO\",\"ID\":3,\"State\":\"1\"}],\"Ts\":[{\"TT\":\"BUT\",\"ID\":1,\"State\":\"1\"},{\"TT\":\"LS\",\"ID\":2,\"State\":\"243\"}]}";
	
	DeserializationError error = deserializeJson(doc, jsonStr);
	if (error)
	{
		SerialUSB.print("deserializeJson() failed: ");
		SerialUSB.println(error.f_str());
		return;
	}
	
	
	
	// Print out the json document
	JsonObject obj = doc.as<JsonObject>();	// convert to json object. The JsonObject class is only a pointer to the JsonDocument, with some additional functions to help with parsing arbitrary stuff inside. The memory is not freed when the JsonObject goes out of scope; that only happens when the JsonDocument goes out of scope.
	
	//printJsonObject(obj);
	
	
	// Parse the JSON object similar to how we will do it in our project for RX packets 
	parseRXJsonPacket(obj);
}

void loop()
{
	
}

void printJsonObject(JsonObject obj)
{
	for (JsonPair p: obj)
	{
		SerialUSB.print(p.key().c_str());
		SerialUSB.print(": ");
		if (p.value().is<const char*>())
		{
			SerialUSB.println(p.value().as<const char*>());
		}
		else if (p.value().is<int>())
		{
			SerialUSB.println(p.value().as<int>());
		}
		else if (p.value().is<JsonArray>())
		{
			SerialUSB.println("list [");
			for (JsonObject o: p.value().as<JsonArray>())	// do NOT use an indexed for loop (e.g. p.value()[i]) to get an element of the list, as it is much slower than this method
			{
				SerialUSB.println("{");
				printJsonObject(o);
				SerialUSB.println("}");
			}
			SerialUSB.println("]");
		}
	}
}

void parseRXJsonPacket(JsonObject obj)
{
	// Get the node ID, see if it matches this node ID
	if (!obj.containsKey("NID"))
	{
		SerialUSB.println("No node ID in JSON");
		return;
	}
	int NID = obj["NID"];
	if (NID != SELF_NODE_ID)
	{
		SerialUSB.println("Packet not for this node ID");
		return;
	}
	SerialUSB.print("NID = ");
	SerialUSB.println(NID);
	
	// Get the packet ID
	if (!obj.containsKey("PID"))
	{
		SerialUSB.println("No packet ID in JSON");
		return;
	}
	
	int PID = obj["PID"];
	SerialUSB.print("PID = ");
	SerialUSB.println(PID);
	
	// Get the uptime
	if (!obj.containsKey("UT"))
	{
		SerialUSB.println("No uptime in JSON");
		return;
	}
	
	int UT = obj["UT"];
	SerialUSB.print("UT = ");
	SerialUSB.println(UT);
	
	// Are there any activation devices in the JSON dictionary?
	if (obj.containsKey("As"))
	{
		// Make sure the value is of type JsonArray
		if (!obj["As"].is<JsonArray>())
		{
			SerialUSB.println("As key is not an array!");
			return;
		}
		
		// Cycle through each activation device
		for (JsonObject o: obj["As"].as<JsonArray>())
		{
			// Get the type
			if (!o.containsKey("AT"))
			{
				SerialUSB.println("No type in activation device");
				return;
			}
			String AT = o["AT"];
			SerialUSB.println("Type = " + AT);
			
			// Get the activation device ID
			if (!o.containsKey("ID"))
			{
				SerialUSB.println("No ID in activation device");
				return;
			}
			int AID = o["ID"];
			
			// Get the state string to set the activation device to
			if (o.containsKey("State"))
			{
				if (!o["State"].is<char*>())
				{
					SerialUSB.println("State must be of type String in activation device");
					return;
				}
				
				String state = o["State"].as<char*>();
				SerialUSB.println("State = " + state);
			}
		}
	}
	else
	{
		SerialUSB.println("No activation devices in JSON");
	}
}
