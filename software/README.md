
Raspberry Pi OS Bookworm

## C

gcc -Wall -pthread -o pwmtest main.c -lpigpio


# IMU Setup

Raspi-config -> Interface Options ->**Disable** I2C Kernel module.

```bash
sudo bash -c "echo 'dtoverlay=i2c-gpio,i2c_gpio_sda=2,i2c_gpio_scl=3,bus=3' >> /boot/firmware/config.txt"
```
# GPS Setup

Raspi-config -> Interface Options -> Disable Serial Port Console, Enable Serial Port.

( /dev/ttyS0 )

```bash
sudo apt install gpsd pps-tools chrony libgps-dev

sudo bash -c "echo 'dtoverlay=pps-gpio,gpiopin=18' >> /boot/firmware/config.txt"
sudo bash -c "echo 'enable_uart=1' >> /boot/firmware/config.txt"
sudo bash -c "echo 'init_uart_baud=9600' >> /boot/firmware/config.txt"
sudo bash -c "echo 'pps-gpio' >> /etc/modules"
```
Reboot.

Check PPS: `sudo ppstest /dev/pps0`

`sudo nano /etc/default/gpsd`
```
START_DAEMON="true"
USBAUTO="false"
DEVICES="/dev/ttyS0 /dev/pps0"
GPSD_OPTIONS="-n"
```

`sudo systemctl start gpsd`


sudo nano /etc/chrony/chrony.conf

Under pool entry, add:
```
refclock SHM 0 refid NMEA offset 0.000 precision 1e-3 poll 3 noselect
refclock PPS /dev/pps0 refid PPS lock NMEA poll 3
```
sudo systemctl restart chrony

watch -n 1 chronyc sources