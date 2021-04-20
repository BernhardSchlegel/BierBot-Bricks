## BierBot Brick



### Setup for easy use

First, we  need to flash the firmware.

1. Order a Sonoff [TH16 + DS18B20 Temperature sensor](https://amzn.to/3uhLiXN).
2. In case you don't have one laying around, you'll also need:
   1. [FT232RL USB to TTL](https://amzn.to/3ujiT3w) for flashing the BierBot Brick firmware onto your TH16.
   2. [the cheapest female rainbow cables you can find on the WWW](https://amzn.to/39IG9A3) to hook the TH16 and the FT232 up.
   3. [a 4x1 pin header](https://amzn.to/3rXGrJT) to solder it onto your TH16. 
   4. **INSTEAD** of the last two, you can also buy an [electronic fun kit](https://amzn.to/3fArDhE), which is most likely cheaper.
3. Hook everything up as shown in the image. 

TODO: Image / Schematic of Rainbow cable connection.

4. Download the [NodeMCU Flasher](https://github.com/nodemcu/nodemcu-flasher).
6. Grab the (current) version from `./BierBot-Brick/ESP8266/releases`.
7. Open the NodeMCU Flasher and hit "Flash".

Second, read [the disclaimer](https://github.com/BernhardSchlegel/BierBot-Brick/blob/master/README.md#development) and cut a extension cable into two pieces and connect as shown in the picture. **ATTENTION** :warning: check if the
labels on YOUR PCB match the colors of the cables: L = Brown, N = Blue, E / Ground symbol = Yellow / Green.

TODO: Image of AC connection.

Your device is now ready to be used.

Go to [bricks.bierbot.com](https://bricks.bierbot.com/#/) > "Brewery" > "Sensors / Actors" and hit "Add device". Copy the API-Key into your clipboard. Power up your BierBot Brick. Scan for the "BierBot Brick 101" WiFi and connect. Enter the credentials to your home WiFi and paste the API-key from your clipboard. Hit "Save". 

**DONE**. The BierBot Brick should show up in your dashboard.

### Developer Setup

1. Install VSCode from [here](https://code.visualstudio.com/).
2. Install PlatformIO as described [here](https://platformio.org/).
3. Install driver for your Serial converter. E.g. from [here](https://ftdichip.com/drivers/vcp-drivers/).
4. connect your TTL Converter to the Sonoff TH 16. If all cables are in a row, you likely did something wrong. 
5. Click the terminal Icon in the footer bar of VSCode saying "PlatformIO: new terminal" & type `pio lib install`.
6. Click the alien on the right side > "Project Tasks" > "sonoff_th" > "Build" and then "Upload".

### Notes for devs

To decode stacktraces use `python3 ./decoder.py -e ./firmware.elf ./trace.txt -s` from [here](https://github.com/janLo/EspArduinoExceptionDecoder).

### Help

If you get a bunch of WiFi Manager related compiling messages, unzip "./backup/2021_01_15_16_39_WiFiManager.zip" to ".pio/libdeps/sonoff_th"

### TODOs

- [ ] Reduce programm size to 50% FLASH so that OTA update works.
- [ ] migrate WiFiManager as soon there is a stable version that works.
- [ ] as soon as ESP8266 supports SSL without hardcoding fingerprints: Enable SSL.

## Disclaimer

:warning: **DANGER OF ELECTROCUTION** :warning:

If your device connects to mains electricity (AC power) there is danger of electrocution if not installed properly. If you don't know how to install it, please call an electrician (***Beware:*** certain countries prohibit installation without a licensed electrician present). Remember: _**SAFETY FIRST**_. It is not worth the risk to yourself, your family and your home if you don't know exactly what you are doing. Never tinker or try to flash a device using the serial programming interface while it is connected to MAINS ELECTRICITY (AC power).

We don't take any responsibility nor liability for using this software nor for the installation or any tips, advice, videos, etc. given by any member of this site or any related site.

[disclaimer source](https://github.com/arendst/Tasmota/edit/development/README.md)

## Help

Q: My COM-Port is not recognized!
A: Install the FT232RL Virtual COM-Port Driver from [here](https://ftdichip.com/drivers/vcp-drivers/).
