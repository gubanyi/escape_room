# CSCE 838 Escape Room

## Instructions to Use
 1. Obtain hardware including 5 SAMD21 boards (https://cdn.sparkfun.com/assets/e/0/6/6/5/SamProRF_Graphical_Datasheet_Updated.pdf)
 2. Download this repository.
 3. Upload gateway sketch and 4 enchanted objects sketches to 5 boards.
 4. pip install requirements.txt for python escape_room_app
 5. Execute the python application
 6. Set up cloud: create Azure Account, configure IoT Central, write and deploy Azure Function.

## Project PDF
See EscapeRoom-CSCE838.pdf for a full description of the project, including it's software requirements and testing procedure.

## Authors and Contributions
Marcus Gubanyi 
 * GIU-Gateway Application: `python/escape_room_app.py`
 * Trigger Azure Function: `cloud/trigger/*`
 * Azure IoT Central configuration: `cloud/iotc/*`
 * Technical Writing: primary author of `EscapeRoom-CSCE838.pdf`

Rehenuma Tasnim Rodoshi 
 * Push Button and Bookshelf: `devices/Button_main.ino`

Rezoan Ahmed Nazib 
 * Light Sensor and Portrait: `devices/Light_main.ino`

Samuel Murray 
 * Generic Classes (End-Device, Trigger, Activator): `generic_classes/*`
 * Switched Outlet: `devices/OUTLET_main`
 * Step Device: `devices/STEP_main.ino`
 * Technical Writing: author of "Architecture - Message Dictionary" in `EscapeRoom-CSCE838.pdf`, coauthor of "Customer Requirements" and "Engineering Requirements" in `EscapeRoom-CSCE838.pdf`

Sepehr Tabrizchi 
 * SAMD21 Gateway: `devices/Gateway/Gateway_main.ino`
 * LED Device: `devices/LED200_main/LED200_main.ino`
## License
   Copyright 2022 Marcus Gubanyi

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

## Project Status - Course Complete, Development Halted
This project was a course project in CSCE 838 "Internet of Things" at UNL in Fall 2022. As the semester ends, so does this project. The development team has no intentions to continue this project.

Marcus Gubanyi and students from Concordia University-Nebraska intend to begin a similar escape room project but will not re-use this code, system design or architecture. The new project will use WiFi-connected microcontrollers (such as Raspberry Pi Pico W) instead of LoRaWAN boards.
