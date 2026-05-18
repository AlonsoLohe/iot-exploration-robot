#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ESP32Servo.h>
#include "driver/ledc.h"

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

#define AIO_SERVER     "io.adafruit.com"
#define AIO_SERVERPORT 1883
#define AIO_USERNAME   "AlonsoLohe"
#define AIO_KEY        "ADAFRUIT_IO_KEY_HERE"

#define TOPIC_TEMP "AlonsoLohe/feeds/temperatura"
#define TOPIC_HUM  "AlonsoLohe/feeds/humedad"
#define TOPIC_MQ2  "AlonsoLohe/feeds/gas"

const char* ssid = "upaep wifi";
const char* password = "";

WiFiClient espClient;
PubSubClient client(espClient);

const int PIN_FOTORESISTENCIA = 34;
const int PIN_PROXIMIDAD = 35;
const int PIN_MQ2 = 32;

const int PIN_DHT11 = 23;
#define DHTTYPE DHT11
DHT dht(PIN_DHT11, DHTTYPE);

const int PIN_LED_FOCUS = 2;

const int PIN_BUZZER = 27; 

const int PIN_M1A = 16;
const int PIN_M1B = 17;
const int PIN_M2A = 18;
const int PIN_M2B = 19;

const int PIN_MOTOR_ENABLE = 5;

const int PIN_SERVO = 4;
Servo miServo;

Adafruit_MPU6050 mpu;

const int PWM_FREQUENCY = 5000;
const ledc_timer_bit_t PWM_RESOLUTION = LEDC_TIMER_10_BIT;
const int MAX_DUTY = 1023;

const ledc_timer_t LEDC_TIMER = LEDC_TIMER_0;
const ledc_mode_t LEDC_MODE = LEDC_LOW_SPEED_MODE;
#define MOTOR_ENABLE_CHANNEL LEDC_CHANNEL_0

const int VELOCIDAD_DUTY_CYCLE_MOTORES = 600;

volatile int distanciaCM = 0;
volatile int valorFotorresistencia = 0;
volatile int umbralLuzBajo = 700;

volatile float dhtTemp = 0.0;
volatile float dhtHum = 0.0;
volatile int mq2Value = 0;

volatile float anguloInclinacion = 0.0;

enum MotorState { FORWARD, STOP_INIT, STOPPED, TURNING };
MotorState currentState = FORWARD;
long stopTime = 0;

void recuperarI2C() {
    Serial.println(">>> INICIANDO RECUPERACION I2C (Bit-Bang) <<<");
    
    Wire.end();
    
    pinMode(22, OUTPUT); // SCL
    pinMode(21, INPUT);  // SDA
    
    for(int i = 0; i < 9; i++) {
        digitalWrite(22, HIGH);
        delayMicroseconds(10);
        digitalWrite(22, LOW);
        delayMicroseconds(10);
    }
    
    digitalWrite(22, HIGH);
    delayMicroseconds(10);
    
    Wire.begin(21, 22);
    delay(50);
    
    if (mpu.begin(0x68)) {
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        Serial.println(">>> SENSOR RECUPERADO EXITOSAMENTE <<<");
    } else {
        Serial.println(">>> FALLO RECUPERACION - CHECK CABLES <<<");
    }
}

void wifiConnect() {
    Serial.print("\nConectando a WiFi...");
    WiFi.begin(ssid, password);
    // Timeout para no bloquear el arranque si no hay wifi
    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
        intentos++;
    }
    if(WiFi.status() == WL_CONNECTED){
        Serial.println("\nWiFi Conectado!");
        Serial.print("Direccion IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFallo conexion WiFi (Continuando Offline)");
    }
}

void mqttReconnect() {
    // Solo intentar si hay WiFi
    if(WiFi.status() != WL_CONNECTED) return;
    
    // Un solo intento para no bloquear las tareas RTOS
    if (!client.connected()) {
        Serial.print("MQTT...");
        if (client.connect("ESP32Client", AIO_USERNAME, AIO_KEY)) {
            Serial.println("OK");
        } else {
            Serial.print("Fail rc=");
            Serial.println(client.state());
        }
    }
}

static inline void setMotorSpeed(int duty) {
    duty = constrain(duty, 0, MAX_DUTY);
    ledc_set_duty(LEDC_MODE, MOTOR_ENABLE_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, MOTOR_ENABLE_CHANNEL);
}

static void setupPWM() {
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_MODE;
    timer.timer_num = LEDC_TIMER;
    timer.duty_resolution = PWM_RESOLUTION;
    timer.freq_hz = PWM_FREQUENCY;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer);

    ledc_channel_config_t c = {};
    c.gpio_num = (gpio_num_t)PIN_MOTOR_ENABLE;
    c.speed_mode = LEDC_MODE;
    c.channel = MOTOR_ENABLE_CHANNEL;
    c.intr_type = LEDC_INTR_DISABLE;
    c.timer_sel = LEDC_TIMER;
    c.duty = 0;
    c.hpoint = 0;
    ledc_channel_config(&c);
}

void setMotorAction(int m1a, int m1b, int m2a, int m2b, int enable_duty_cycle, const char* action) {
    digitalWrite(PIN_M1A, m1a);
    digitalWrite(PIN_M1B, m1b);
    digitalWrite(PIN_M2A, m2a);
    digitalWrite(PIN_M2B, m2b);
    setMotorSpeed(enable_duty_cycle);
}

void moveForward() {
    setMotorAction(0, 1, 0, 1, VELOCIDAD_DUTY_CYCLE_MOTORES, "ADELANTE");
}
void stopMotors() {
    setMotorAction(0, 0, 0, 0, 0, "DETENIDO");
}
void turnAround() {
    setMotorAction(1, 0, 0, 1, VELOCIDAD_DUTY_CYCLE_MOTORES, "VUELTA");
}

int adcToDistance(int adcValue) {
    if (adcValue < 100) return 80;
    float voltage = (float)adcValue * (3.3 / 4095.0);
    if (voltage < 0.3) return 80;
    if (voltage > 3.0) return 5;
    return (int)(20.76 / (voltage - 0.11));
}

void TaskMotorControl(void *parameter) {
    const int UMBRAL_DISTANCIA_CM = 50;
    const TickType_t DELAY_MOTOR_MS = pdMS_TO_TICKS(100);
    const int TIEMPO_ESPERA_STOP_MS = 1000;
    const int TIEMPO_GIRO_MS = 1000;
    for (;;) {
        int adcProximidad = analogRead(PIN_PROXIMIDAD);
        distanciaCM = adcToDistance(adcProximidad);
        switch (currentState) {
            case FORWARD:
                if (distanciaCM < UMBRAL_DISTANCIA_CM) {
                    stopMotors();
                    currentState = STOP_INIT;
                    stopTime = millis();
                } else {
                    moveForward();
                }
                break;
            case STOP_INIT:
                if (millis() - stopTime >= TIEMPO_ESPERA_STOP_MS)
                    currentState = STOPPED;
                if (distanciaCM >= UMBRAL_DISTANCIA_CM)
                    currentState = FORWARD;
                break;
            case STOPPED:
                stopMotors();
                if (distanciaCM < UMBRAL_DISTANCIA_CM) {
                    turnAround();
                    currentState = TURNING;
                    stopTime = millis();
                } else {
                    currentState = FORWARD;
                }
                break;
            case TURNING:
                if (millis() - stopTime >= TIEMPO_GIRO_MS) {
                    stopMotors();
                    currentState = STOPPED;
                }
                break;
        }
        vTaskDelay(DELAY_MOTOR_MS);
    }
}

void TaskLightSensor(void *parameter) {
    const TickType_t DELAY_LIGHT_MS = pdMS_TO_TICKS(50);
    for (;;) {
        valorFotorresistencia = analogRead(PIN_FOTORESISTENCIA);
        digitalWrite(PIN_LED_FOCUS, valorFotorresistencia > umbralLuzBajo ? LOW : HIGH);
        vTaskDelay(DELAY_LIGHT_MS);
    }
}

void TaskRoutineSensors(void *parameter) {
    const TickType_t DELAY_ROUTINE_SEC = pdMS_TO_TICKS(2000);
    dht.begin();
    for (;;) {
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        int mq2 = analogRead(PIN_MQ2);

        if (!isnan(h) && !isnan(t)) {
            dhtHum = h;
            dhtTemp = t;
        }
        mq2Value = mq2;
        Serial.printf("Lectura: Temp: %.1fC | Hum: %.1f%% | Gas: %d | Dist: %dcm | Inclinacion: %.2f\n", 
                      dhtTemp, dhtHum, mq2Value, distanciaCM, anguloInclinacion);
                      
        vTaskDelay(DELAY_ROUTINE_SEC);
    }
}

void TaskAlarmControl(void *parameter) {
    const TickType_t DELAY_ALARM_MS = pdMS_TO_TICKS(500);
    for (;;) {
        bool alarmOn =
            (dhtTemp > 40.0) ||
            (dhtHum > 80.0) ||
            (mq2Value > 3000);

        if (alarmOn) {
            digitalWrite(PIN_BUZZER, HIGH);
        } else {
            digitalWrite(PIN_BUZZER, LOW);
        }
        vTaskDelay(DELAY_ALARM_MS);
    }
}

void TaskMqttPublisher(void *parameter) {
    const TickType_t DELAY_PUBLISH_SEC = pdMS_TO_TICKS(30000);

    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            if (!client.connected()) mqttReconnect();
            if (client.connected()) {
                client.loop();
                client.publish(TOPIC_TEMP, String(dhtTemp, 1).c_str());
                client.publish(TOPIC_HUM, String(dhtHum, 1).c_str());
                client.publish(TOPIC_MQ2, String(mq2Value).c_str());
            }
        }
        vTaskDelay(DELAY_PUBLISH_SEC);
    }
}
void TaskServoControl(void *parameter) {
    const TickType_t DELAY_SERVO_MS = pdMS_TO_TICKS(40); 
    float lastAnguloX = 0;
    int frozenCounter = 0; 
    const int MAX_FROZEN_LOOPS = 15; 
    for (;;) {
        sensors_event_t a, g, temp;
        if (mpu.getEvent(&a, &g, &temp)) {
            float accX = a.acceleration.x;
            float accY = a.acceleration.y;
            float accZ = a.acceleration.z;
            float anguloX = atan(accX / sqrt(pow(accY, 2) + pow(accZ, 2))) * 180 / PI;
            anguloInclinacion = anguloX;
            if (abs(anguloX - lastAnguloX) < 0.01) { 
                frozenCounter++;
            } else {
                frozenCounter = 0; 
                lastAnguloX = anguloX;
            }
            if (frozenCounter > MAX_FROZEN_LOOPS) {
                Serial.printf("!!! DETECTADO CONGELAMIENTO (%.2f) - RESETEANDO I2C !!!\n", anguloX);
                recuperarI2C(); 
                frozenCounter = 0;
                lastAnguloX = 0; 
            }
            int servoPos = map((int)anguloX, -90, 90, 180, 0);
            servoPos = constrain(servoPos, 0, 180);
            miServo.write(servoPos);
        } else {
            if(!mpu.begin(0x68)) {
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        
        vTaskDelay(DELAY_SERVO_MS);
    }
}
void setup() {
    Serial.begin(115200);
    recuperarI2C();
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    pinMode(PIN_M1A, OUTPUT);
    pinMode(PIN_M1B, OUTPUT);
    pinMode(PIN_M2A, OUTPUT);
    pinMode(PIN_M2B, OUTPUT);
    stopMotors();
    pinMode(PIN_LED_FOCUS, OUTPUT);
    digitalWrite(PIN_LED_FOCUS, HIGH);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    miServo.setPeriodHertz(50);
    miServo.attach(PIN_SERVO, 500, 2400);
    setupPWM();
    wifiConnect();
    client.setServer(AIO_SERVER, AIO_SERVERPORT);
    xTaskCreate(TaskAlarmControl,     "AlarmControl",     2048, NULL, 5, NULL);
    xTaskCreate(TaskMqttPublisher,    "MqttPublisher",    8192, NULL, 4, NULL);
    xTaskCreate(TaskMotorControl,     "MotorControl",     4096, NULL, 3, NULL);
    xTaskCreate(TaskLightSensor,      "LightSensor",      2048, NULL, 2, NULL);
    xTaskCreate(TaskServoControl,     "ServoControl",     4096, NULL, 2, NULL); 
    xTaskCreate(TaskRoutineSensors,   "RoutineSensors",   4096, NULL, 1, NULL);
    moveForward();
    currentState = FORWARD;
}

void loop() {
    vTaskDelete(NULL);
}