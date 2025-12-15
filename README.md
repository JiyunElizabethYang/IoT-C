# 🌦 Weather Tracker IoT

Weather Tracker IoT is a software-based Internet of Things (IoT) project that simulates an ESP32 environment using **VS Code** and **Wokwi**.  
The system retrieves real-time weather data from the **Korea Meteorological Administration (KMA)** through its API Hub, determines location information based on IP-derived coordinates, and visualizes weather conditions using OLED displays and LED indicators.

🔗 **Project Website:**  
https://jiyunelizabethyang.github.io/IoT-C/

---

## 📌 Project Overview

This project aims to demonstrate how a complete **IoT system** can be developed and tested.  
By integrating real-time weather data, location detection, and visualization techniques, the system provides an intuitive and scalable approach to location-based weather monitoring.

---

## 🧪 System Architecture

The system follows a modular workflow:
- Location detection based on IP-derived coordinates
- Real-time weather data acquisition from the KMA API
- XML data parsing and processing
- Weather visualization using:
  - Text-based information on one OLED
  - Graph-based trends (temperature, humidity, rainfall, wind) on a second OLED
- LED indicators for quick weather status recognition

---

## ✨ Key Features

- Real-time weather data retrieval and XML parsing from the KMA API
- Location-based weather tracking using simulated GPS and IP information
- Dual OLED visualization:
  - Text display for current weather information
  - Graph display for weather trends
- LED indicators for weather conditions:
  - 🔴 Red: Sunny
  - 🔵 Blue: Rainy
  - 🟡 Yellow: Windy
- Fully simulated IoT system using VS Code and Wokwi

---

## 👥 Team Members & Roles

- **Jiho Park (Leader)**  
  - GPS and location handling  
  - Coordinate-to-address conversion and grid mapping logic  

- **Jeongmin Park**  
  - Weather API integration  
  - XML data parsing and graph visualization logic

- **Jiyun Yang**  
  - OLED and LED display implementation  
  - Presentation materials preparation (webpage, GitHub, PPT, etc.)

---

## 📅 Project Timeline

- **Weeks 9–10:** Weather API integration, OLED text display, and location conversion setup  
- **Weeks 11–12:** Project refinement, LED behavior implementation, and graph visualization planning  
- **Weeks 13–14:** GPS-based location detection, OLED graph visualization, and system completion  
- **Weeks 15–16:** Webpage deployment, GitHub Pages setup, and final presentation  

---

## 🛠 Technologies Used

- VS Code  
- Wokwi (ESP32 Simulator)  
- C / C++ (Arduino Framework)  
- HTTPClient  
- KMA Weather API (XML)  
- OLED Display (Text & Graph Visualization)
