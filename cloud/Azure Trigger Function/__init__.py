# Escape Room Project
#
# Azure Function Trigger to Activate Commands
#
# Author: Marcus Gubanyi
# Last Modified: December 8, 2022
#
# Description: 
#

import logging

import azure.functions as func
import requests

# Linear flow for escape room
#                 1. Players enter escape room and clues lead to shine light on portrait.
# LightTrigger    2. Light sensor triggers, activating switched outlet turning on lamp. 
#                 3. Clues lead to book on shelf.
# ButtonTrigger   4. Removing book triggers button, activating LEDs/Candles.
#                 5. Clues lead to step on floor mat.
# StepTrigger     6. Step triggers, activating the GUI application
#                 7. Clues lead to passcode.
# PasscodeTrigger 8. Entering passcode triggers, activating the GUI to complete escape room.

# Flow configuration is a dictionary with the following structure
#      keys => Trigger Names
#      values => List of Commands

flow_configuration = {
    "LightTrigger":
        [{"NAME":"SetState", "TNID":"4", "AID":"1", "S":"1"}],

    "ButtonTrigger":
        [{"NAME":"SetState", "TNID":"6", "AID":"1", "S":"1"}],

    "StepTrigger":
        [{"NAME":"SetState", "TNID":"8", "AID":"1", "S":{
                "IsComplete": "0",
                "HeaderText": "Escape!",
                "HeaderVisible":"1",
                "InstructionsText": "Enter the passcode",
                "InstructionsVisible":"1",
                "ButtonText": "Submit",
                "ButtonVisible":"1",
                "Passcode": "123",
                "PasscodeVisible":"1"
            }}],

    "PasscodeTrigger":
        [{"NAME":"SetState", "TNID":"8", "AID":"1", "S":{
                "IsComplete": "1",
                "HeaderText": "Complete!",
                "HeaderVisible":"1",
                "InstructionsText": "",
                "InstructionsVisible":"0",
                "ButtonText": "",
                "ButtonVisible":"0",
                "Passcode": "",
                "PasscodeVisible":"0"
            }}]
}


def send_command(command):
    """Run command on IoT Central device.
    
    command : dictionary with keys 
        "NAME" - name of trigger 
        "TNID" - target node ID (end-device)
        "AID" - activation ID (lives on end-device)
        "S" - state to be set on activation

    returns response to command
    """
    # defining the api-endpoint and body based on the command
    API_ENDPOINT = f"https://csce838escaperoom.azureiotcentral.com/api/devices/EscapeRoomApp/commands/{command['NAME']}?api-version=2022-07-31"

    body = {'request':command}

    # set api authorization
    headers = {'Authorization': "SharedAccessSignature sr=025a11fa-3ac4-4179-a64e-7a017ccd622a&sig=F3RrMY3kpN8WsNuKke3ScTzyDWXp%2BFudq3noTeoFHz4%3D&skn=CommandDevices&se=1700782146808"}

    # sending post request and saving response as response object
    r = requests.post(url = API_ENDPOINT, headers=headers, json=body)
    
    # extracting response text 
    return r.text


def main(req: func.HttpRequest) -> func.HttpResponse:
    """Activate devices with commands based on which device triggers the function."""
    trigger = req.get_json()

    # Flow configuration connects a trigger with a list of commands to activate
    commands = flow_configuration[trigger['NAME']]

    responses = []
    for command in commands:
        responses.append(send_command(command))

    logging.info(f"Python HTTP trigger function processed a request. {trigger['NAME']}")

    responses = '\n'.join(responses)

    return func.HttpResponse(f"Trigger {trigger['NAME']} executed successfully.\nCommand responses:\n{responses}", status_code=200)
