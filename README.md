# esp32-smart-provisioning

> A robust, modular connectivity framework for the ESP32 using ESP-IDF v5.1. This project provides a "fail-safe" onboarding experience by allowing users to configure Wi-Fi credentials via a Web Portal (SoftAP) or Bluetooth Low Energy (NimBLE).

---

## 🚀 Key Features

- **Dual-Mode Provisioning**: Choose between a mobile-friendly Web Server or a modern BLE GATT interface.
- **Modular Architecture**: Fully decoupled modules for Wi-Fi, BLE, and Web Services using an "Observer Pattern" for event management.
- **Memory Efficient**: Built using the NimBLE stack to minimize flash and RAM footprint compared to the standard Bluedroid stack.
- **Automated State Management**: A centralized Provisioning Manager (FreeRTOS based) handles timeouts and resource cleanup to save power.
- **Non-Volatile Storage (NVS)**: Securely stores and retrieves credentials across power cycles.

## 🛠️ Tech Stack

| Component | Technology |
|-----------|------------|
| **Language** | C |
| **Framework** | ESP-IDF (Espressif IoT Development Framework) v5.1+ |
| **RTOS** | FreeRTOS |
| **Protocols** | BLE (GATT), HTTP, TCP/IP, Wi-Fi (AP/STA) |
| **Build System** | CMake & Ninja |

## 📂 Project Structure

```plaintext
esp32_smart_provisioning/
├── CMakeLists.txt
├── components
│   └── wifi_module
│       ├── ble_provisioning.c
│       ├── CMakeLists.txt
│       ├── include
│       │   ├── ble_provisioning.h
│       │   ├── utilities.h
│       │   ├── web_server.h
│       │   └── wifi_module.h
│       ├── utilities.c
│       ├── web_server.c
│       └── wifi_module.c
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── sdkconfig
```

## 🚦 Getting Started

### Prerequisites

- ESP-IDF v5.1 environment installed and configured.
- An ESP32 or ESP32-S3 development board.

### Installation

1. **Clone the repository:**

   ```bash
   git clone https://github.com/dev-muhamid/esp32-smart-provisioning.git
   cd esp32-smart-provisioning
   ```

2. **Set the target:**

   ```bash
   idf.py set-target esp32
   ```

3. **Build and Flash:**

   ```bash
   idf.py build flash monitor
   ```

### Usage

On first boot, the device will enter **Provisioning Mode**.

#### Option 1: BLE Provisioning

- Open a BLE scanner (like **nRF Connect**) and connect to `ESP32_MY_PROV`.
- Write the **SSID** to characteristic `0xFF01`.
- Write the **Password** to characteristic `0xFF02`.

#### Option 2: Web Provisioning

- Connect to the Wi-Fi AP `ESP32_PROV_AP`.
- Navigate to `http://192.168.4.1` in your browser.
- Enter your Wi-Fi credentials in the web portal.

---

## 📝 License

This project is open source and available under the [MIT License](LICENSE).

## 🤝 Contributing

Contributions, issues, and feature requests are welcome!

## 📧 Contact

For questions or support, please open an issue on GitHub.