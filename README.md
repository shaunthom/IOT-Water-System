# IOT-Water-System

This is an IOT-based Water Management system. It can monitor the water level in sumps and tanks, which is in turn integrated with a fresh water supply system. Human intervention is made minimal and optimal usage of time is ensured. The entire circuits have been voltage regulated as well for longevity of the model. The control panel side acts as the brain of the entire automated operation. Subsequent messages are shown on a screen by the use of control panel.

### Objective:

The objective of the prototype is to monitor water levels in real-time as well as to automate the process of refilling water in tanks and sumps, eliminating human intervention. It also has the provision to alert the required personnel when water touches or goes below a predefined level. 

### Brief Outline:

In our IoT water management system, water level sensors are installed in the tanks and sumps to continuously monitor the water level. These sensors send the collected data to an ESP32 microcontroller-based control panel via WiFi. The control panel processes this data and can autonomously control the water refilling system based on pre-defined conditions. A React Native mobile application serves as the user interface, displaying real-time water level statistics and enabling users to remotely manage features like automatic refilling, tracking time to refill water and so on.

### Working:

##### Control Panel.ino : 

The code manages a water pump control panel for building sumps, offering both manual and automatic modes. It switches between two sumps based on preset conditions and uses current and voltage sensors for monitoring. Communication is done via Wi-Fi and ESP-NOW, with user interface through an LCD screen and remote access via Blynk. The system also includes notifications for specific conditions like an empty sump and safety features such as overload detection. All operations are executed in the Arduino's loop() function.

##### Sump Protection.ino :
 
The code controls a sump pump system, utilizing the ESP8266 WiFi module and ESP-NOW for communication. Various GPIO pins are configured for monitoring water levels, pump status, and error conditions. The code runs both setup and loop functions to initialize components and execute logic in real-time. Timed triggers manage periodic updates and error displays. The system integrates manual controls through a toggle switch and sends status updates via ESP-NOW. Additionally, safety features are included, such as disabling the pump when water is not available.

##### Voltage Regulation.cpp :

The code uses a NodeMCU board to manage a pump system with safety features and manual controls. It initializes pin configurations for the pump, manual switches, and voltage sensors. The main loop reads current values from an ACS sensor, averages them, and displays them on an LCD. Safety checks for overcurrent, under-voltage, and over-voltage are included to automatically turn the pump off under hazardous conditions. Manual ON and OFF switches allow for user control, with the LCD providing real-time status updates.

### Results:

The successful implementation of this pump control system had several significant impacts:

###### Operational Efficiency:
The system automated routine tasks, minimizing human intervention and thereby increasing efficiency. The real-time data display helped in proactive decision-making, further enhancing operational productivity.

###### Energy Savings:
Through its precise control mechanisms, the system contributed to energy efficiency. Overcurrent and voltage-based shutdowns not only protected the equipment but also helped in reducing energy wastage.

###### Safety:
The safety features, including overcurrent and abnormal voltage detection, significantly reduced the risks associated with pump operation, such as electrical fires or equipment damage.

###### Scalability:
Given its successful implementation, the system serves as a blueprint for further advancements. It demonstrates the feasibility of expanding this model to a more extensive network of pumps, potentially integrating it with IoT technologies for remote monitoring and control.
