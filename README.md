\# IoT Autonomous Exploration Robot

# 

# A 4-wheel autonomous exploration robot built with an ESP32 microcontroller, capable of real-time sensor telemetry and live video streaming over WiFi. Developed as part of the Sensors \& Signal Conditioning course at UPAEP (2025).

# 

# !\[Robot photo](photos/robot.jpg)

# 

# \---

# 

# \## Demo

# 

# 🎥 \[Watch demo video](https://youtu.be/grLDTA6npIw)

# 

# \---

# 

# \## Features

# 

# \- 📡 Real-time telemetry over WiFi using \*\*MQTT protocol\*\*

# \- 📷 Live video streaming via \*\*ESP32CAM\*\*

# \- 🌡️ Multi-sensor environmental monitoring (gas, temperature, humidity, light, distance, inclination)

# \- 🔁 Remote monitoring from any networked device

# \- ⚡ Sub-200ms latency on telemetry data

# 

# \---

# 

# \## Hardware

# 

# | Component | Purpose |

# |---|---|

# | ESP32 | Main microcontroller — WiFi, MQTT, sensor processing |

# | ESP32CAM | Live video streaming |

# | MQ2 Gas Sensor | Gas / smoke detection |

# | SHARP Infrared Sensor | Distance measurement / obstacle detection |

# | DHT11 | Ambient temperature and humidity |

# | MPU6050 | Robot inclination / orientation (gyroscope + accelerometer) |

# | LDR | Ambient light level measurement |

# 

# \---

# 

# \## Architecture

# 

# ```

# Sensors → ESP32 → MQTT Broker (Adafruit IO) → Any device on the network

# &#x20;               ↓

# &#x20;          ESP32CAM → WiFi video stream

# ```

# 

# \---

# 

# \## Getting Started

# 

# \### Requirements

# 

# \- Arduino IDE with ESP32 board support installed

# \- \[Adafruit IO](https://io.adafruit.com) account (free)

# \- Libraries: `Adafruit MQTT`, `DHT sensor library`, `MPU6050`, `WiFi`

# 

# \### Setup

# 

# 1\. Clone this repo:

# &#x20;  ```bash

# &#x20;  git clone https://github.com/AlonsoLohe/iot-exploration-robot.git

# &#x20;  ```

# 

# 2\. Create a `secrets.h` file inside the `IET400\_V3/` folder:

# &#x20;  ```cpp

# &#x20;  #define AIO\_KEY "your\_adafruit\_io\_key\_here"

# &#x20;  ```

# 

# 3\. Open `IET400\_V3/IET400\_V3.ino` in Arduino IDE

# 

# 4\. Update your WiFi credentials in the code:

# &#x20;  ```cpp

# &#x20;  #define WIFI\_SSID "your\_network\_name"

# &#x20;  #define WIFI\_PASS "your\_password"

# &#x20;  ```

# 

# 5\. Select \*\*ESP32\*\* as your board and upload

# 

# \---

# 

# \## MQTT Topics

# 

# The robot publishes sensor data to the following Adafruit IO feeds:

# 

# | Feed | Data |

# |---|---|

# | `gas` | MQ2 gas sensor reading |

# | `distance` | SHARP IR distance (cm) |

# | `temperature` | DHT11 temperature (°C) |

# | `humidity` | DHT11 humidity (%) |

# | `light` | LDR light percentage |

# | `inclination` | MPU6050 tilt angle |

# 

# \---

# 

# \## Photos

# 

# | | |

# |---|---|

# | !\[](photos/photo1.jpg) | !\[](photos/photo2.jpg) |

# 

# \---

# 

# \## Author

# 

# \*\*Alonso Lopez Hernandez\*\* — Mechatronics Engineer  

# \[LinkedIn](https://www.linkedin.com/in/alonso-lópez-hernández-10335716a) · \[GitHub](https://github.com/AlonsoLohe)

