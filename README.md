# 🌦 Weather Tracker IoT (Group C)

Weather Tracker IoT is a software-based Internet of Things (IoT) project that simulates an ESP32 environment using **VS Code** and **Wokwi**.  
The system retrieves real-time weather data from the **Korea Meteorological Administration (KMA)** through its API Hub, determines location information based on IP-derived coordinates, and visualizes weather conditions using OLED displays and LED indicators.

🔗 **Project Website:**  
https://jiyunelizabethyang.github.io/IoT-C/

---

## 📌 Project Overview

This project focuses on building a **fully virtual IoT system** without physical hardware.  
By integrating external weather data, simulated location detection, and virtual sensor visualization, the system demonstrates how real-world IoT services can be prototyped and tested entirely in software.

Key objectives of this project include:
- Retrieving and processing real-time weather data
- Simulating location-based services
- Visualizing environmental data in an intuitive way
- Gaining hands-on experience with IoT system architecture and data flow

---

## 🧪 System Architecture

The Weather Tracker IoT system follows a modular architecture:

1. **Location Detection**
   - Determines geographic coordinates based on the device’s internet IP address
2. **Weather Data Acquisition**
   - Retrieves XML-formatted weather data from the KMA API
3. **Data Processing**
   - Parses and extracts key parameters such as temperature, humidity, rainfall, and wind
4. **Visualization**
   - Displays weather information as text on one OLED
   - Shows four graphs (temperature, humidity, rainfall, wind) on a second OLED
   - Uses LED indicators to represent weather conditions

---

## ✨ Key Features

- Fetches and parses real-time weather data from the KMA API
- Location-based weather tracking using IP-derived coordinates
- Dual OLED visualization:
  - Text-based weather information
  - Graph-based weather trends
- LED indicators for intuitive weather status
  - 🔴 Red: Sunny
  - 🔵 Blue: Rainy
  - 🟡 Yellow: Windy
- Fully simulated IoT environment using VS Code and Wokwi

---

## 🛠 Technologies Used

- **VS Code**
- **Wokwi (ESP32 Simulator)**
- **C / C++ (Arduino framework)**
- **HTTPClient**
- **KMA Weather API (XML)**
- **OLED Display (Text & Graph Visualization)**

---

## 👥 Team Members & Roles

- **Jeongmin Park**  
  - Weather API integration  
  - XML data parsing and processing  

- **Jiho Park**  
  - GPS and IP-based location simulation  
  - Virtual sensor and LED logic  

- **Jiyun Yang**  
  - OLED visualization (text & graphs)  
  - Serial output design and system integration  

---

## 📅 Project Timeline

- **Weeks 9–10:** Project planning and environment setup  
- **Weeks 11–12:** Weather API integration and data parsing  
- **Weeks 13–14:** OLED visualization and virtual sensor implementation  
- **Weeks 15–16:** Debugging, system refinement, and final presentation  

---

## 🚀 Future Improvements

- Web-based dashboard for weather visualization
- Map-based location display
- Support for real hardware sensors
- Extended historical weather analysis

---

## 📄 License

This project is for educational purposes.
