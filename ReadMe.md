| Supported Targets | ESP32 |
| ----------------- | ----- |

# ESP32 Clock synchronization over Bluetooth

## overview
This is a student project for the PRO-IOT Module at ESIR (2nd year, IoT Diploma).
The goal of this project is to implement a solution to synchronize ESP32's clock in order to synchronize servers (the Microcontroller is acting as a key to transmit its clock). This synchronization needs to be made thanks to the Bluetooth Protocol.

## Technical Aspects
### Microcontroller
This project has been developped for ESP-32 microcontrollers. 

2 differents ESP-32 microchip boards were used to develop and test the code : 
- ESP32-WROOM-32U 
- ESP32-D0WDQ6.

These boards include an Bluetooth stack.

## How to use this project
### Hardware required
This project is designed to run on commonly available ESP32 development board. 2 boards are required, one with the `Master` code running on it and the other with the `Slave` one.
### Clone the project
Run the following command:
```
git clone https://github.com/hbertin1/esp32_sync_timecode
```
### Configure the project

```
idf.py menuconfig
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Project description
After the program started, the `Master` board will connect to the `slave` one. Then you can use the CLI to request/send LTC (Linear TimeCode) and to print some clock values.

## Console
The CLI is accessible through the monitor. Use `help` to display all the commands available.

## Troubleshouting

- If the discovery on the Master side is already over when you boot the slave, reboot the Master. The Slave is expected to run before the Master (or before the Master's end of Bluetooth discovery).

- If you get some "stack Overflow in task pthread" errors, make sure that `PTHreads/'default task stack size'`isn't set to a too small value, it should work to set it to `5120`.
