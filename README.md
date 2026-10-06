# sproac
# SPROAC — Smart Produce Rotting & Oxidation Alert Controller

<p align="center">
  <b>Smart IoT-Based Post-Harvest Produce Monitoring & Preservation System</b>
</p>

<p align="center">
  Detect spoilage early • Monitor storage conditions • Control preservation • Connect to the cloud
</p>

---

## 📌 Overview
## 🎥 SPROAC — Working Demonstration

Watch the complete demonstration of the SPROAC prototype, including sensor monitoring, spoilage detection, IoT connectivity, and system operation.

<p align="center">
  <a href="https://youtu.be/27sTv4I3YBQ?si=bwbs9ZbUWlaqHpma">
    <img src="https://img.youtube.com/vi/YOUR_VIDEO_ID/maxresdefault.jpg" width="800">
  </a>
</p>

<p align="center">
  <b>▶ Watch SPROAC Demo on YouTube</b>
</p>

**SPROAC (Smart Produce Rotting & Oxidation Alert Controller)** is an IoT-based smart preservation and monitoring system designed to reduce post-harvest losses in agricultural produce.

The system continuously monitors environmental and spoilage-related parameters and provides early warnings before produce deteriorates significantly.

SPROAC is designed for produce such as:

- 🧅 Onion
- 🧄 Garlic
- 🥔 Potato
- 🍅 Tomato
- 🍌 Banana

The system combines **temperature and humidity monitoring**, **gas-based spoilage detection**, **local alerts**, **automated preservation mechanisms**, and **cloud-based monitoring** into a single portable platform.

---

## 🎯 Problem Statement

A significant percentage of agricultural produce is lost after harvesting due to:

- Poor storage conditions
- Excess temperature and humidity
- Inadequate ventilation
- Microbial activity
- Oxidation and decomposition
- Delayed detection of spoilage

Traditional storage methods generally depend on manual inspection, which can detect deterioration only after visible or measurable damage has already occurred.

### SPROAC Approach

SPROAC continuously monitors storage conditions and spoilage indicators to enable **early detection and preventive action**.

> **Sense → Analyze → Alert → Act → Monitor**

---

## 💡 Key Features

### 🌡️ Environmental Monitoring

Real-time monitoring of:

- Temperature
- Relative humidity
- Storage conditions

The system uses an environmental sensor to determine whether the produce is being stored under suitable conditions.

### 🧪 Spoilage Gas Detection

SPROAC uses gas sensors to identify chemical indicators associated with produce degradation.

The sensing system includes:

- **H₂S sensing**
- **MQ-135 air-quality/gas sensing**
- **Alcohol/VOC sensing**

These sensor readings can be combined to estimate the spoilage condition of the stored produce.

### ⚠️ Early Spoilage Detection

Instead of relying only on visible damage, SPROAC analyzes changes in sensor readings to identify abnormal conditions and provide an early warning.

A future AI/ML layer can further improve produce-specific spoilage classification.

### 💨 Automated Preservation

The system can control preservation hardware such as:

- Mini vacuum pump
- Solenoid valve
- Ventilation/control mechanism

This enables the system to move beyond monitoring into **active preservation**.

### ☁️ Cloud Monitoring

Sensor data can be transmitted to a cloud-connected monitoring platform for:

- Remote monitoring
- Historical data analysis
- Condition tracking
- Alerts
- Data visualization

### 📱 Web Dashboard

The monitoring interface provides a centralized view of:

- Temperature
- Humidity
- Gas readings
- Produce condition
- System status
- Historical sensor trends

The dashboard can be extended for use by farmers, wholesalers, and retailers.

---

# 🏗️ System Architecture

```text
                    ┌───────────────────────────────┐
                    │        AGRICULTURAL           │
                    │          PRODUCE              │
                    │  Onion / Garlic / Potato ...  │
                    └───────────────┬───────────────┘
                                    │
                                    ▼
                    ┌───────────────────────────────┐
                    │        SENSOR LAYER           │
                    │                               │
                    │  Temperature / Humidity       │
                    │  H₂S Sensor                   │
                    │  MQ-135                       │
                    │  Alcohol / VOC Sensor         │
                    └───────────────┬───────────────┘
                                    │
                                    ▼
                    ┌───────────────────────────────┐
                    │       XIAO ESP32-C3            │
                    │                               │
                    │  Sensor Acquisition            │
                    │  Data Processing               │
                    │  Threshold Analysis            │
                    │  Wi-Fi Communication            │
                    │  Local Control                  │
                    └───────────────┬───────────────┘
                                    │
                    ┌───────────────┴───────────────┐
                    │                               │
                    ▼                               ▼
        ┌────────────────────┐          ┌─────────────────────┐
        │  LOCAL INTERFACE    │          │   PRESERVATION      │
        │                    │          │      CONTROL        │
        │ OLED Display       │          │                     │
        │ Buttons            │          │ Vacuum Pump         │
        │ Alerts             │          │ Solenoid Valve      │
        └────────────────────┘          └─────────────────────┘
                    │
                    ▼
        ┌────────────────────────────┐
        │      CLOUD PLATFORM        │
        │                            │
        │ Google Sheets / Web Server │
        │ Data Storage               │
        │ Analytics                  │
        └────────────┬───────────────┘
                     │
                     ▼
        ┌────────────────────────────┐
        │       WEB DASHBOARD        │
        │                            │
        │ Live Monitoring            │
        │ Graphs                     │
        │ Alerts                     │
        │ Historical Data            │
        └────────────────────────────┘
```

---

# 🔧 Hardware

| Component | Purpose |
|---|---|
| **Seeed Studio XIAO ESP32-C3** | Main controller and Wi-Fi connectivity |
| **DHT11 / Environmental Sensor** | Temperature and humidity monitoring |
| **Fermion MEMS H₂S Sensor (DFRobot SEN0568)** | Hydrogen sulfide detection |
| **MQ-135** | Air-quality / gas monitoring |
| **Alcohol / VOC Sensor (SEN0376)** | Alcohol/VOC detection |
| **OLED SSD1306** | Local display |
| **Push Buttons** | User navigation and control |
| **Mini Vacuum Pump (5V)** | Preservation / air extraction |
| **Solenoid Valve (5V)** | Air-flow control |
| **MOSFET Driver** | Switching high-current loads |
| **3.7V Li-ion Battery** | Portable power source |
| **Boost Converter** | Voltage regulation |
| **BMS / Charging Circuit** | Battery management and protection |

---

# 📌 XIAO ESP32-C3 Pin Mapping

The current SPROAC hardware configuration uses the following GPIO assignments:

| Function | GPIO | XIAO Pin |
|---|---:|---|
| DHT Sensor | GPIO 2 | D0 |
| H₂S Sensor | GPIO 3 | D1 |
| MQ-135 | GPIO 4 | D2 |
| Alcohol Sensor | GPIO 5 | D3 |
| I²C SDA | GPIO 6 | D4 |
| I²C SCL | GPIO 7 | D5 |
| Back Button | GPIO 20 | D6 |
| Up Button | GPIO 8 | D8 |
| Down Button | GPIO 9 | D9 |
| Enter Button | GPIO 10 | D10 |

I²C communication is initialized using:

```cpp
Wire.begin(6, 7);
```

---

# 🧠 Software Architecture

SPROAC uses an embedded-to-cloud architecture.

```text
Sensors
   │
   ▼
ESP32-C3
   │
   ├── Sensor Processing
   ├── Threshold Evaluation
   ├── OLED Interface
   ├── Local Control
   │
   ▼
Wi-Fi
   │
   ▼
HTTP / JSON
   │
   ▼
Cloud / Google Apps Script
   │
   ▼
Google Sheets / Data Storage
   │
   ▼
Web Dashboard
```

---

# 💻 Technology Stack

### Embedded

- C/C++
- Arduino Framework
- ESP32-C3
- I²C
- ADC
- GPIO
- Wi-Fi
- NTP time synchronization

### Embedded Libraries

```text
Adafruit_SSD1306
DHT
ArduinoJson
WiFi
HTTPClient
SPIFFS
NTP
Wire
```

### Cloud & Backend

- Google Apps Script
- Google Sheets
- HTTP/JSON communication

### Frontend

- HTML
- CSS
- JavaScript
- Chart.js

---

# 📊 Data Flow

The system periodically reads the sensor values and transmits them to the monitoring platform.

```text
Sensor Acquisition
        ↓
Data Filtering / Processing
        ↓
Threshold Evaluation
        ↓
Spoilage Condition Estimation
        ↓
Local Display + Alert
        ↓
Cloud Data Transmission
        ↓
Dashboard Visualization
```

The firmware is designed around periodic acquisition and communication intervals.

Example configuration:

```cpp
#define SENSOR_READ_INTERVAL 2000
#define DATA_SEND_INTERVAL   5000
```

This allows the device to sample sensor data more frequently than it transmits complete datasets to the cloud.

---

# 🚦 Spoilage Detection Concept

SPROAC can use multiple sensor parameters rather than relying on a single gas sensor.

For example:

```text
Temperature
     +
Humidity
     +
H₂S
     +
MQ-135
     +
Alcohol / VOC
     ↓
Multi-Parameter Analysis
     ↓
Produce Condition
     ↓
Normal / Warning / Spoilage Risk
```

A future version can use machine-learning models trained on sensor data for produce-specific prediction.

### Potential AI Layer

```text
Sensor Data
     ↓
Preprocessing
     ↓
Feature Extraction
     ↓
ML Classification
     ↓
Spoilage Probability
     ↓
Recommended Action
```

---

# 🖥️ User Interface

The device includes a local OLED interface for displaying sensor information and navigating system functions.

Possible screens include:

```text
┌─────────────────────┐
│      SPROAC         │
│                     │
│ Temp : 28.4 °C      │
│ RH   : 72.1 %       │
│ H2S  : 0.18 ppm     │
│ Status: NORMAL      │
└─────────────────────┘
```

The navigation buttons allow the user to access different monitoring and configuration pages.

---

# 🌐 Web Dashboard

The web dashboard is intended to provide remote visibility into the storage environment.

### Dashboard Elements

- Live temperature graph
- Humidity graph
- Gas sensor readings
- Spoilage status
- Historical trends
- Device status
- Alerts
- Produce-specific monitoring

Example conceptual dashboard:

```text
┌──────────────────────────────────────────────────┐
│                 SPROAC DASHBOARD                  │
├──────────────────────────────────────────────────┤
│ Temperature │ Humidity │ H₂S │ Status            │
│    28.4°C    │  72.1%   │ ... │   NORMAL          │
├──────────────────────────────────────────────────┤
│                                                  │
│              SENSOR TREND GRAPH                 │
│                                                  │
├──────────────────────────────────────────────────┤
│ Storage Condition          │ System Status      │
│ ✓ Temperature              │ Online             │
│ ✓ Humidity                 │ Wi-Fi Connected    │
│ ✓ Gas Monitoring           │ Data Sync Active   │
└──────────────────────────────────────────────────┘
```

---

# 🔋 Portable Design

One of the goals of SPROAC is to create a portable monitoring and preservation unit that can be deployed in different agricultural storage environments.

The system is designed to support use at multiple stages of the supply chain:

```text
Farmer
  ↓
Collection / Storage
  ↓
Wholesaler
  ↓
Retailer
  ↓
Consumer
```

This enables the same monitoring concept to be adapted for different storage environments.

---

# 💰 Estimated Bill of Materials

The prototype BOM was estimated in the range of approximately:

**₹2,800 – ₹3,200**

The final cost depends on sensor selection, enclosure, battery capacity, control hardware, and preservation mechanism.

Typical cost contributors include:

- ESP32-C3 controller
- Environmental sensor
- Gas sensors
- OLED display
- Pump
- Solenoid valve
- Battery
- Power electronics
- Mechanical enclosure

---

# 🔬 Research & Development Direction

SPROAC can be expanded from a rule-based IoT monitoring device into an intelligent post-harvest preservation platform.

### Phase 1 — Monitoring

```text
Temperature + Humidity + Gas Sensors
                    ↓
               ESP32-C3
                    ↓
             Local Dashboard
```

### Phase 2 — Automated Preservation

```text
Environmental Monitoring
          ↓
Condition Detection
          ↓
Automatic Control
          ↓
Pump / Valve / Ventilation
```

### Phase 3 — AI-Based Prediction

```text
Historical Sensor Data
          ↓
Feature Engineering
          ↓
Machine Learning
          ↓
Produce-Specific Model
          ↓
Spoilage Prediction
```

### Phase 4 — Connected Supply Chain

```text
Farmer
   │
   ▼
Storage Unit
   │
   ▼
Cloud Platform
   │
   ├── Wholesaler
   ├── Retailer
   └── Analytics
```

---

# ✨ What Makes SPROAC Different?

Conventional storage monitoring generally focuses on environmental parameters such as temperature and humidity.

SPROAC attempts to combine:

**Environmental sensing + spoilage-gas monitoring + automated preservation + cloud monitoring**

in a compact and portable system.

The important design philosophy is:

> **Don't just detect spoilage. Detect it early and respond to it.**

---

# 📈 Future Improvements

Future versions of SPROAC can include:

- Produce-specific AI models
- Multi-class spoilage classification
- Better calibrated gas sensors
- Sensor fusion
- Predictive spoilage analytics
- GSM/LoRa communication for remote areas
- Mobile application
- Automated ventilation
- Automatic storage-condition optimization
- Edge AI processing
- Long-term cloud analytics
- Multi-device farm/warehouse monitoring

---

# 🧪 Testing Plan

The system can be validated using controlled experiments in which produce is monitored over different storage conditions.

### Suggested Experiment

```text
Fresh Produce
     ↓
Controlled Storage
     ↓
Record Sensor Data
     ↓
Periodic Visual Inspection
     ↓
Record Spoilage Stage
     ↓
Compare Sensor Data
     ↓
Develop Detection Threshold / ML Model
```

The collected dataset can later be used to train and evaluate an AI-based spoilage prediction model.

---

# 📂 Project Structure

A suggested repository structure is:

```text
SPROAC/
│
├── firmware/
│   ├── src/
│   ├── include/
│   └── libraries/
│
├── dashboard/
│   ├── index.html
│   ├── style.css
│   └── script.js
│
├── cloud/
│   └── google_apps_script/
│
├── hardware/
│   ├── circuit/
│   ├── pcb/
│   └── schematics/
│
├── docs/
│   ├── architecture/
│   ├── testing/
│   └── images/
│
├── data/
│   └── sensor_samples/
│
└── README.md
```

---

# 🚀 Getting Started

## 1. Hardware Setup

Connect the sensors and peripherals according to the SPROAC pin mapping.

Ensure:

- Correct sensor voltage levels
- Proper common ground
- Stable power supply
- MOSFET protection for inductive loads
- Battery protection through a suitable BMS

## 2. Firmware

Open the firmware using Arduino IDE or a compatible ESP32 development environment.

Install the required libraries:

```text
Adafruit SSD1306
DHT
ArduinoJson
SPIFFS
```

Configure:

```cpp
WiFi SSID
WiFi Password
Cloud Endpoint
NTP Settings
Sensor Pins
```

Then upload the firmware to the XIAO ESP32-C3.

## 3. Dashboard

Place the dashboard files on your preferred web server or deploy them as required by your architecture.

## 4. Cloud Integration

Configure the Google Apps Script endpoint and connect the ESP32-C3 using HTTP/JSON requests.

---

# 🛠️ Current Development Status

**Project:** SPROAC  
**Version:** 4.0 development  
**Platform:** XIAO ESP32-C3  
**Domain:** IoT / Embedded Systems / Agriculture  
**Primary Focus:** Post-harvest spoilage detection and preservation

### Current System Capabilities

- [x] Temperature monitoring
- [x] Humidity monitoring
- [x] H₂S sensing
- [x] MQ-135 sensing
- [x] Alcohol/VOC sensing
- [x] OLED interface
- [x] Button-based navigation
- [x] Wi-Fi connectivity
- [x] JSON-based data transmission
- [x] Cloud data logging
- [x] Web dashboard concept
- [x] Automated preservation hardware integration
- [ ] Produce-specific ML model
- [ ] Large-scale validation dataset
- [ ] Predictive spoilage model

---

# 🤝 Applications

SPROAC can be adapted for:

- Agricultural warehouses
- Onion storage facilities
- Vegetable markets
- Cold/storage rooms
- Wholesale distribution centers
- Retail storage
- Farm-level post-harvest monitoring
- Smart agriculture research

---

# 📜 License

This project is intended for research, prototyping, and educational development.

Add an appropriate open-source license such as **MIT License** when publishing the repository.

---

# 👨‍💻 Author

**Ashwin K**

Embedded Systems • IoT • Electronics • AI

Interested in building practical hardware systems that combine embedded intelligence, sensing, automation, and real-world applications.

---

# ⭐ Project Vision

> **SPROAC aims to turn post-harvest storage from a passive process into an intelligent, data-driven preservation system.**

By continuously sensing the storage environment, detecting spoilage indicators, and enabling automated intervention, SPROAC can help reduce avoidable post-harvest losses and improve the reliability of agricultural storage.
