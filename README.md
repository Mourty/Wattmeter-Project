Wattmeter Readme
========================

# The Wattmeter Project

This project was created for a senior design course as part of my degree path. I wanted to create a wattmeter similar to the "Kill A Watt" meters typically available, but with an important change. I wanted the ability to log the energy consumption data over time. I created a functional prototype well before I even began the class, but decided to iterate on the project and improve it. This is my first project of this scale, and I am sure it isn't perfect. In fact I'm sure it can be improved upon, but it works well enough for me.  
  
### Features
* Single Phase voltage, current, active power, reactive power, apparent power, frequency, and power factor measurement.
* LCD Display for user readable measurements.
* WiFi connectivity with RESTful API
* Configurable via SD card or web interface
* Dedicated Real time clock with automatic NTP syncing (WiFi required)
* Logs data to an SD card for optional independent operation
* Integrated UPS to prevent data loss due to power outage (100ish seconds plenty to automatically save data buffer)
* Monitoring server for remote data collection, storage, and presentation using React and python.


### Hardware
The hardware is a custom designed PCB with mostly through hole components. I chose through hole because it is what I wanted to use, more on this later.
The meter uses the Atmel M90E32 Poly-phase energy monitoring IC to do the heavy lifting of accurate power measurement. The voltage input uses a stepdown transformer and voltage divider to bring the 120VAC input voltage down around a manageable 0.6vrms. The transformer is somewhat unnecessary as a series of resistors could do the same, but this also provides some transient protection and isolation. The current measurement utilizes a current transformer with a carefully sizes burden resistor to bring the voltage down to around 0.6vrms when 20 amps is flowing though the primary of the CT. 
The power supply for the whole board is an off the shelf AC to DC power supply. This provides stable "safe" power without needed to roll my own PSU. This is fed to a super capacitor, then to the LM2621 based boost converted. The super capacitor provided an energy buffer to allow for controlled shutdown of the microcontroller in the event of a power outage. This allows time to write the current buffer of power statistics to the SD card. without this feature the SD card would either need to be written to more often with small amounts of data or the buffer would be lost during a power outage. Depending on the setting this could be multiple minutes or hours of data lost. The size of the super cap was chosen due to part availability at the time; It could easily be a quarter or even an eighth as large and still serve its purpose.
None of the components have been price optimized to a good degree. For example the diodes that are apart of the super capacitor/boost converter are rated for 2 amps, but don't ever see near that amount. These are simply what I had on hand during the design and didn't see a need to order different ones. So If you make this project too, you might want to consider components carefully, you might save some money.

I figured I could improve upon the wattmeter with the wattmeterJR. This version has fewer features, but is smaller and uses SMD components. It lacks an LCD display or the whole integrated UPS part. It is mainly to measure the same power statistics, but really requires the remote server to log the data. it is much less user friendly to build and to debug, but is cheaper and smaller. You can find this version at https://github.com/Mourty/WattmeterJR

### Firmware
I wrote the firmware with assistance from Claude.ai. I'm not the sort to go around proclaiming to be a coder, so you will find issues here. I used the Arduino IDE to develop, compile, and upload the code to the ESP32. The code is semi-organized into different files and imported where needed. The meter itself can be configured using the SD card. You will need to write the settings.ini to the SD card. In the settings you will find a place to configure your WiFi credentials as well as other settings. Many of these will seem cryptic, but most of them won't need to be modified at all. Most are actually configuration registers for the ATM90E32. You can find more on what the do in the IC's datasheet.  
  
### Service
The service is a web server that uses React for the front end and python for the backend. I again used Claude.ai to write much of the code here. The service is containerized using docker, so you should be able to get it up and running fairly easily. I used a Raspberry PI 3+, but in theory any linux computer can run this server. You will need to install docker to compose the containers and to run them. Docker *should* take care of installing other things for the containers like python and such.



I don't plan to maintain this or improve further at this time. If someone wants to improve this feel free, it won't hurt my feelings if you fork this project and do cool things with it, just give me credit for what you use. I have no idea what license this project should have given it uses a few external libraries, so before you go attempting to monetize this project you might need to check on that.

