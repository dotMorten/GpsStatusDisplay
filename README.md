# High Accuracy GPS Device Build

Build your own survey-grade super-accurate GPS Device at a fraction of the cost.
The below list will run you around $300 (mainly for the advanced GPS chip), but will rival high-end GPS receivers that'll often cost you up towards $10000, not to mention you get a built-in interface for configuring the device, instead of relying on external desktop or mobile software.

### Features:
 - Up to 10mm accuracy with RTCM Correction Source
 - 128x128 RGB OLED Display for status and configuration
 - Bluetooth and Serial Port connections
 - 5-way button to control display and configure device
 
 ### Parts List:
 1. High-accuracy GPS Chip: [SparkFun GPS-RTK2 Board - ZED-F9P (Qwiic) High-Precision Breakout](https://amzn.to/2HEfFoN)
 2. GNSS Multi-Band Antenna : [GNSS Multi-Band Magnetic Mount Antenna - 5m (SMA)](https://www.sparkfun.com/products/15192)
 3. Antenna Ground Plate: [GPS Antenna Ground Plate](https://www.sparkfun.com/products/15004)
 4. UFL-to-SMA antenna connector: [Karcy Mini PCI UFL to SMA Female Connector](https://amzn.to/34DS7c4)
 5. Bluetooth chip: [SparkFun Bluetooth Silvermate](https://amzn.to/35LofKn)
 6. Arduino: [SparkFun Thing Plus - SAMD51](https://amzn.to/3kK7pSq)
 7. Display: [Waveshare 1.5inch RGB OLED Display Module](https://amzn.to/3oGIIsc)
 8. Buttons: [NOYITO 5-Channel Tactile Switch Breakout](https://amzn.to/31V8IGq)
 9. Qwiic connector: [SparkFun Qwiic Cable Kit Hook Up](https://amzn.to/2TBvj6r)
 10. Various cables/connectors for soldering it all together
 
You really only need 1-4, if you don't need a display. Add 5 if you want bluetooth. However without the Arduino, display and buttons, all configuration has to be done via the u-blox desktop software.
For antenna you can pretty much use any GPS antenna rated for multi-band L1 and L2, but the above combined with the ground plate is likely the cheapest option while supporting full 10mm RTK.

### Resources

- [SparkFun GPS-RTK2 Hookup Guide](https://learn.sparkfun.com/tutorials/gps-rtk2-hookup-guide)
- [NmeaParser](https://github.com/dotMorten/NmeaParser) - My library to process NMEA data from the device, as well as sending it NTRIP Correction Data for full RTK 10mm accuracy
- [SparkFun u-blox Arduino Library](https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library)
- [UcgLib](https://github.com/olikraus/ucglib) - Display library used for rendering to the OLED display

### Display example:
![gpsdisplay_4](https://user-images.githubusercontent.com/1378165/97663689-7a299100-1a37-11eb-9d00-08eab29f479d.gif)
 
### Connection Diagram

![image](https://user-images.githubusercontent.com/1378165/97655357-23fe2300-1a22-11eb-9e4f-2a2a4c6cb93b.png)

### Accuracy

With an RTCM Correction source such as an NTRIP service, you can get down to around 10mm accuracy. As an example here's the point plot of 1000 measurements in RTK mode with an RTCM base station ~7.7 km away.

![image](https://user-images.githubusercontent.com/1378165/97666691-68e28380-1a3b-11eb-9e9c-0af0f7178090.png)

