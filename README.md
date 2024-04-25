# G-5500 PiDrive

***This project is a work in progress, very little testing has so far been done, I supply no guarantee that the basic concepts work***

Controlling an AC-type Yaesu G-5500 Az+El Rotator with PWM from a Raspberry Pi, allowing a portable and compact drive unit running off a 12V battery with no mains inverters.

Choices have been made to use high-end voltage-isolated parts to allow easy co-existence with multiple power domains, as several RF receivers will also be connected to the Raspberry Pi.

## Physical Form Factor

PWM board intends to mount on the _back_ of an RPI DIN Plate, facing the Host Raspberry Pi on another DIN plate, connected by Adafruit GPIO Ribbon Cable.
* https://thepihut.com/products/dinrplate-2-raspberry-pi-din-rail-mount

PCB Render:

![PCB CAD Render](https://raw.githubusercontent.com/philcrump/g5500-pidrive/master/pcb/render/g5500-pidrive-r1.png)

### Supply Voltage

36-48V Input, Tracpower Isolated TEL 12-4811WI DC-DC converts down to 5.1V @ 2.4A for the Raspberry Pi.

MAX22530 12-bit Isolated ADC cam measure the input voltage, and also exposes a connector to measure 12V battery voltage, and an optional extra channel (charge current?)

## G-5500 Control

### Motor Drive

Yaesu mains control box produces 80V pk-pk AC. Initially I plan to use a cheap 12V -> 36V DC-DC step-up to allow output of 72V pk-pk AC. The design allows for increasing to 48V motor bus voltage.

DRV8251A 4.1A Motor drivers used to achieve 50Hz AC PWM over the relevant winding for each direction. Initial experiments with variable-frequency were not very successful, but there may be +/-10% to play with for accel/decel softening.

E-STOP implemented with hardware logic (high-speed CMOS) holds motors in 'Brake' regardless of software - if switch contact is opened.

Footprints have been included to allow the use of RF chokes on the PWM outputs if RFI problems are encountered.

### Potentiometer sensing

MCP3202 12-bit ADC measures the wiper voltage on each of the Az+El Potentiometers to sense position. Careful decoupling of the supply voltage and filtering of the wiper voltage is implemented to achieve a low-noise result.

## Position / Alignment Sensors

* Uputronics Ublox GPS Breakout as daughterboard for Time + Position https://store.uputronics.com/index.php?route=product/product&path=64&product_id=84
* * Connection for NTPD Timing setup with PPS https://austinsnerdythings.com/2021/04/19/microsecond-accurate-ntp-with-a-raspberry-pi-and-pps-gps/

* BNO055 DOF IMU Fusion for magnetic-north calibration https://thepihut.com/products/adafruit-9-dof-absolute-orientation-imu-fusion-breakout-bno055
* * Mounted at end of aluminium boom
* * Active I2C LTC4311 Terminator to extend the I2C far enough down the boom (screened cable will be used.)

## Software

Motor drive using pigpio's excellent ability to run hardware-timed PWM on every GPIO. https://abyz.me.uk/rpi/pigpio/index.html

## Authors

* Phil Crump M0DNY <phil@philcrump.co.uk>
