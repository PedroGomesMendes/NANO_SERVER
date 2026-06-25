# NANEYE2 ESP32 Image Capture Server

## Overview

This project implements a high-speed image acquisition system for the **NANEYE2 CMOS image sensor** using an **ESP32**. The firmware configures the image sensor over SPI, continuously captures 12-bit RAW images using DMA, stores frames in PSRAM with double buffering, and streams the captured images through either an embedded HTTP server or UDP.

The project demonstrates how the ESP32 can be used as a compact and low-cost image acquisition platform without requiring external FPGA or image processing hardware.

---

# Features

* High-speed SPI communication (31 MHz)
* Continuous DMA image acquisition
* Double-buffered image capture using PSRAM
* Embedded Wi-Fi Access Point
* Embedded HTTP server for image streaming
* Optional UDP image transmission
* Runtime exposure control
* Runtime analog gain control
* Multi-core processing
* Low memory overhead
* RAW 12-bit image acquisition

---

# Hardware Requirements

* ESP32 (with PSRAM)
* NANEYE2 image sensor
* SPI connection
* Power supply for the sensor
* Wi-Fi capable device (PC, tablet or smartphone)

---

# Image Specifications

| Parameter        | Value            |
| ---------------- | ---------------- |
| Resolution       | 328 × 320 pixels |
| Pixel Format     | RAW 12-bit       |
| Bytes per Line   | 492 bytes        |
| Total Image Size | 157,440 bytes    |

---

# Software Architecture

The application is divided into two independent tasks running on separate ESP32 cores.

## Core 0

Responsible for:

* Initializing the SPI peripheral
* Configuring the image sensor
* Capturing image frames using DMA
* Swapping frame buffers

## Core 1

Responsible for:

* Starting the Wi-Fi Access Point
* Running the HTTP server
* Serving captured images
* Handling exposure and gain requests

When UDP mode is enabled, Core 1 continuously transmits the image over UDP instead of serving it through HTTP.

---

# Memory Architecture

Two frame buffers are allocated in PSRAM:

```
Buffer A  <-- SPI DMA writes here
Buffer B  <-- HTTP server reads here
```

After every frame acquisition:

```
write_buffer <--> read_buffer
```

This prevents tearing while allowing continuous acquisition.

---

# Wi-Fi Configuration

The ESP32 creates its own wireless network.

Default configuration:

```
SSID: NANEYE_CAM
Password: 12345678
```

The firmware operates in Access Point mode.

---

# HTTP Endpoints

## Root Page

```
GET /
```

Serves the embedded HTML interface.

---

## Image Endpoint

```
GET /image
```

Returns the latest captured RAW frame.

Content-Type:

```
application/octet-stream
```

---

## Exposure Control

```
GET /set_exposure?value=<0-255>
```

Updates the sensor exposure.

Example:

```
/set_exposure?value=150
```

---

## Gain Control

```
GET /set_gain?value=<0-3>
```

Updates the sensor analog gain.

Example:

```
/set_gain?value=2
```

---

# UDP Mode

If enabled:

```c
#define WEBSERVER 0
#define UPD_SENDER 1
```

Frames are transmitted line-by-line using UDP packets.

Packet format:

```
492 bytes  RAW image data
2 bytes    Line number
```

Packet size:

```
494 bytes
```

Destination:

```
IP: 192.168.4.2
Port: 5001
```

---

# SPI Configuration

| Parameter | Value   |
| --------- | ------- |
| SPI Host  | SPI3    |
| Clock     | 31 MHz  |
| DMA       | Enabled |
| SPI Mode  | Mode 0  |

---

# Sensor Configuration

The firmware dynamically generates the NANEYE configuration registers.

Supported runtime parameters include:

* Exposure
* Ramp Gain
* Analog Gain
* Offset Ramp
* Output Current
* Bias Current
* VREF
* High Speed Mode
* Idle Mode

Configuration frames are generated before every image acquisition.

---

# Performance

The project is optimized for continuous image acquisition using:

* DMA transfers
* Double buffering
* Multi-core execution
* PSRAM storage
* Minimal CPU overhead

The CPU performs almost no processing on image data.

---

# Project Structure

```
.
├── main
│   ├── main.c
│   ├── index.html
│   └── ...
├── CMakeLists.txt
├── sdkconfig
└── README.md
```

---

# Build

Using ESP-IDF:

```bash
idf.py set-target esp32

idf.py build

idf.py flash monitor
```

---

# Future Improvements

* JPEG compression
* MJPEG streaming
* WebSocket streaming
* Live image preview
* Auto Exposure
* Auto Gain Control
* Image processing
* Frame rate statistics
* Sensor register GUI
* OTA firmware updates

---

# License

This project is released under the MIT License.

---

# Author

Pedro Mendes

pedro.mendes@ams-osram.com
