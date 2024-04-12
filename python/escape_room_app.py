# Escape Room Project
#
# GUI App Device and Gateway to Cloud
#
# Author: Marcus Gubanyi
# Last Modified: December 8, 2022
#
# Description:
# GUI has 4 four components which are configured with the cloud.
#     header, instructions, codebox and submit button
#
# The Gateway portion of the application receives and sends serial
# messages with a microcontroller on a LoRaWAN network.

import random 
import time

# Library for GUI
from guizero import App, Text, TextBox, PushButton

# Library for USB Serial Connection
import serial

# Libraries for Cloud Connection
import threading
import requests
from iotc.models import Command, Property 
from iotc import IoTCClient, IOTCConnectType, IOTCEvents 

is_complete = False

# GUI Setup
app = App(title="Escape Room")

header = Text(app,
                    text="",
                    size=60,
                    font="Times New Roman",
                    color="lightblue",
                    visible = False)

instructions = Text(app,
                    text="",
                    size=40,
                    font="Times New Roman",
                    color="lightblue",
                    visible = False)

code_box = TextBox(app, visible = False)
code = ""
failed_attempts = 0

def submit_code():
    if code_box.value == code:
        device.send_telemetry({ 
            'PasscodeTimeToComplete': time.time() - start_time
        })
        thread = threading.Thread(target=azure_trigger)
        thread.start()
    else:
        failed_attempts += 1

submit_button = PushButton(app, command=submit_code, text="", visible = False)


# USB Serial Communication Setup
# TODO
def serial_listener():
    ser = serial.Serial("portname?", 9600, timeout=0.1)
    while not is_complete:
        time.sleep(0.5)
        line = ser.readline()
        print(line)


# Cloud Setup - Azure Function
#LOCAL_ENDPOINT = "http://localhost:7071/api/Trigger"
CLOUD_ENDPOINT = "https://csce838escaperoomtrigger.azurewebsites.net/api/trigger"

def azure_trigger():
    # The request will timeout (regardless of the timeout duration)
    # because calling the azure function sends a command to this
    # device. If this device is waiting on a response then it can't
    # receive the command.
    # An alternative (better) implementation would be to use a separate
    # thread for the HTTP Triggered Azure Function call and response.
    try:
        r = requests.post(url = CLOUD_ENDPOINT, json = {"NAME":"PasscodeTrigger"})
    except:
        pass

# Cloud Setup - IoT Central
scopeId = '0ne0086041B'
device_id = 'EscapeRoomApp' 
device_key = 'bvs67YYVkDh7NZDuTv5fiXL9BCi6NbxO5SPV3ujxAZA='

def on_commands(command: Command):
    print(f"{command.name} command was received")
    if not command.name == "SetState":
        return

    state = command.value["S"]
    print(command.value)
    
    if command.value["TNID"] == '8': # The GUI application
        is_complete = state["IsComplete"] == "1"
        header.value = state["HeaderText"]
        header.visible = state["HeaderVisible"] == "1"
        instructions.value = state["InstructionsText"]
        instructions.visible = state["InstructionsVisible"] == "1"
        global code
        code = state["Passcode"]
        code_box.visible = state["PasscodeVisible"] == "1"
        submit_button.text = state["ButtonText"]
        submit_button.visible = state["ButtonVisible"] == "1"
        
    else:
        # TODO: pass the command through USB serial.
        # SERIAL WRITE
        pass
    
    command.reply() 


device = IoTCClient(device_id,  
                    scopeId, 
                    IOTCConnectType.IOTC_CONNECT_DEVICE_KEY, 
                    device_key) 
 
device.connect() 
 
device.on(IOTCEvents.IOTC_COMMAND, on_commands) 
 
device.send_property({ 
    "LastPowerOn8": time.time() 
}) 


def heartbeat():
    device.send_telemetry({ 
        'PasscodeFailedAttempts': code_box.value,
        'AverageLightLevel': 0,
        'StepState': 0,
        'ButtonState': 0
    })
    
    print("Heartbeat: telemtry sent")


if device.is_connected(): 
    code_box.repeat(60000, heartbeat)
    start_time = time.time()
    app.display()
    

 
    
