# Smart Temperature-Regulated Jacket 🌡️🧥

An ESP32-based smart wearable system that automatically regulates body temperature using intelligent heating and cooling mechanisms, with manual control, real-time display, GPS tracking, and web-based monitoring.

---

## 📌 Project Overview

The **Smart Temperature-Regulated Jacket** is designed to maintain thermal comfort by continuously monitoring ambient temperature and automatically activating heating or cooling systems based on predefined thresholds. The system also supports manual override controls and provides real-time system status through an OLED display and a web interface hosted by the ESP32.

---

## ✨ Key Features

### 🔹 Automatic Temperature Control
- Heating pad turns **ON below 21°C**
- Cooling system turns **ON above 30°C**
- System remains **IDLE between 21°C and 30°C**

### 🔹 Manual Override
- Physical switches allow the user to manually activate heating or cooling at any time

### 🔹 Thermal Balancing System
- **Carbon Heating Pad** controlled using **IRFZ44N MOSFET**
- **Peltier Module (TEC1-12706, 12V 92W)** triggered via **5V single-channel relay**
- Maintains balance between heating and cooling

### 🔹 Cooling & Airflow Control
- **5V DC Air Blower** for air circulation
- **5V DC CPU Fan** for exhaust airflow
- Both controlled using **L298N Motor Driver**

### 🔹 Real-Time Monitoring
- Live temperature and system status displayed on **1.96” OLED Display**

### 🔹 GPS Tracking
- **NEO-6M GPS Module** provides real-time location via satellite

### 🔹 Web-Based Configuration
- ESP32 hosts a **local web interface**
- System status and controls accessible using the device IP address on the same network

---

## 🧰 Hardware Components Used

- ESP32 Development Board  
- DS18B20 Digital Temperature Sensor  
- Carbon Heating Pad  
- IRFZ44N MOSFET  
- Peltier Module (TEC1-12706, 12V 92W)  
- 5V Single-Channel Relay Module  
- L298N Motor Driver  
- 5V DC Air Blower  
- 5V DC CPU Fan  
- 1.96” OLED Display  
- NEO-6M GPS Module  
- Manual Control Switches  
- External Power Supply (as required)

---

## ⚙️ System Working Logic

1. DS18B20 continuously reads ambient temperature.
2. ESP32 processes the temperature data.
3. Control logic:
   - **Temperature < 21°C → Heating ON**
   - **Temperature > 30°C → Cooling ON**
   - **21°C–30°C → Idle**
4. Peltier module helps balance heating and cooling.
5. Blower and exhaust fan manage airflow.
6. OLED displays real-time system status.
7. Web interface allows monitoring and control via IP address.
8. GPS module provides real-time location data.

---

## 🚀 Applications

- Smart wearable technology  
- Extreme cold or hot weather protection  
- Defense and industrial safety wearables  
- Medical and personal comfort systems  

---

