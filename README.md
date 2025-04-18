# ⛽ Smart Fuel Monitoring System

A simple and effective ESP32-based IoT system to monitor water/fuel levels, usage trends, and send alerts — all accessible through your mobile phone using the Blynk app.

---

## 🛠️ Features

- Real-time level monitoring (in cm and %)
- Fuel/water usage tracking
- Remaining tank capacity
- Usage trends over time
- Low fuel alerts (push notification)
- Battery-powered with TP4056 charging

---

## ⚙️ Hardware Used

- **ESP32**
- **HC-SR04 Ultrasonic Sensor**
- **TP4056 Charging Module**
- **3.7V Li-ion Battery**

---

## 📲 Blynk Dashboard Preview

| App Screenshot | Circuit |
|----------------|---------|
| <img src="gui of fuel monitoring system.jpg" width="200"/> | <img src="circuit.png" width="200"/> |

---

## 📡 Pin Mapping

| Component    | ESP32 Pin |
|--------------|-----------|
| HC-SR04 TRIG | GPIO5     |
| HC-SR04 ECHO | GPIO18    |
| VCC          | 3.3V      |
| GND          | GND       |
| TP4056 OUT+  | VIN       |
| TP4056 OUT−  | GND       |

---

## 🔔 Alert Logic

Low fuel notification is triggered when level drops below 20%.
</br>
<img src="alert message.jpg" width="200"/>

---

## 🚀 How to Use

1. Upload the code to ESP32
2. Connect your components as shown above
3. Link your Blynk template with correct virtual pins
4. Power on and start monitoring!

---

Made with ❤️ by [Revanshu Pusadkar](https://www.linkedin.com/in/revanshu-pusadkar-454082273/)
