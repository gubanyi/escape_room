# Written by Sam Murray

from UART import UART
import json
from time import sleep


# Global variables
EndNodeIDs = {
	'server': 1,
	'light': 3,
	'SO': 4,
	'BUTN': 5,
	'LED': 6,
	'STEP': 7
}

nodes = [
	{
		'ID': EndNodeIDs['light'],
		'PID': 2,
		'T': 'light'
	},
	{
		'ID': EndNodeIDs['SO'],
		'PID': 200,
		'T': 'SO'
	},
	{
		'ID': EndNodeIDs['BUTN'],
		'PID': 2,
		'T': 'BUTN'
	},
	{
		'ID': EndNodeIDs['LED'],
		'PID': 200,
		'T': 'LED'
	},
	{
		'ID': EndNodeIDs['STEP'],
		'PID': 2,
		'T': 'STEP'
	}
]	# {"ID": int, "PID": int, "T": str}

def set_outlet_state(outletNum:int, state:bool):
	# Get the switched outlet node
	n = None
	for _n in nodes:
		if _n['ID'] == EndNodeIDs['SO'] and _n['T'] == 'SO':
			n = _n
			break
		
	n['PID'] += 100
	
	d = {
		'SNID': 1,
		'TNID': EndNodeIDs['SO'],
		'PID': n['PID'],
		'UT': 1,
		'AL': [
			{
				'ID': outletNum,
				'T': 'SO',
				'S': str(int(state))
			}
		]
	}
	
	s = json.dumps(d, separators=(',', ':'))
	
	sleep(0.5)
	uart.WriteLine(s)
	print('Sending:', s)
	sleep(0.5)
	
def set_led_state(state:bool):
	# Get the LED node
	n = None
	for _n in nodes:
		if _n['ID'] == EndNodeIDs['LED'] and _n['T'] == 'LED':
			n = _n
			break
		
	n['PID'] += 100
	
	d = {
		'SNID': 1,
		'TNID': EndNodeIDs['LED'],
		'PID': n['PID'],
		'UT': 1,
		'AL': [
			{
				'ID': 1,
				'T': 'SO',
				'S': str(int(state))
			}
		]
	}
	
	s = json.dumps(d, separators=(',', ':'))
	
	sleep(0.5)
	uart.WriteLine(s)
	print('Sending:', s)
	sleep(0.5)

# Get UART port
uart = UART()
port = uart.InteractivePortChooser()

if uart.Open(port, baudrate=115200) is not True:
	print('ERROR: Failed to open serial port', port)
	exit()


set_outlet_state(1, False)
set_led_state(False)

state = 0
while True:
	# Wait for a line to come in from serial
	s = uart.ReadLine()
	if s is None:
		continue
	
	print('Received:', s.strip())
	
	# Decode JSON string to dictionary
	try:
		d = json.loads(s)
	except:
		print('ERROR: Unable to decode JSON string:', s)
		continue
	
	if 'SNID' not in d:
		print('ERROR: SNID not in JSON string:', s)
		continue
	
	if 'PID' not in d:
		print('ERROR: PID not in JSON string:', s)
		continue
	
	TL = None
	if 'TL' in d:
		TL = d['TL']
	
	# Get the node
	n = None
	for _n in nodes:
		if _n['ID'] == d['SNID']:
			n = _n
			break
		
	n['PID'] = d['PID']
	
	if state == 0:
		# Waiting for step trigger
		if d['SNID'] == EndNodeIDs['STEP']:
			if TL is not None and len(TL) > 0:
				S = TL[0]['S']
				if S == '1':
					state = 1
					set_outlet_state(1, True)
					print('Advancing to state', state)
	elif state == 1:
		# Waiting for light sensor trigger
		if d['SNID'] == EndNodeIDs['light']:
			if TL is not None and len(TL) > 0:
				S = TL[0]['S']
				if 'Trig:1' in S:
					state = 2
					set_led_state(True)
					set_led_state(True)
					set_led_state(True)
					print('Advancing to state', state)
	elif state == 2:
		# Waiting for book to be removed
		if d['SNID'] == EndNodeIDs['BUTN']:
			if TL is not None and len(TL) > 0:
				S = TL[0]['S']
				if S == '0':
					state = 3
					print('ESCAPED!')
					exit(0)
					