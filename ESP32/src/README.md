# UDP - UART Bridge

This is arduino firmware to create a UDP to UART bridge device on an ESP32.

The ESP32 is configured as in _STA mode, i.e. must be connected to a router. The settings are managed over Serial, via a command-line interface.

## Configuration Instructions

To begin using the Serial command line, you must 

When setting up the device for the first time, you must configure the following settings at minimum:

1. Set the ssid and password of your target wifi network:

```
setssid example-2.4Hgz
```
Make sure that the network is 2.4Ghz - this is required for ESP32 devices.

2. Set password:

```
setpwd example-password
```

Note: passwords will be stored in plaintext on the device and can be read back over serial. Physical access is required. Do not use on a secure network. 

At this point, the device should be connected to your network. You can type:

```
ipconfig
```

into the command line to get 