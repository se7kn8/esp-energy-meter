#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RingBufCPP.h>
#include "config.h"

WiFiClient espClient;
PubSubClient client(espClient);

// Width and height of the display in pixel

Adafruit_SSD1306 display(WIDTH, HEIGHT, &Wire, -1);

// Width for a single sample line in pixels

RingBufCPP<int, WIDTH / SAMPLE_WIDTH> dataPoints;

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (!error) {
        int power = doc["ENERGY"]["Power"];
        Serial.printf("Power %dW\n", power);
        display.printf("Power %dW\n", power);
        display.clearDisplay();
        if (dataPoints.isFull()) {
            int value;
            dataPoints.pull(&value);
        }
        dataPoints.add(power);

        int min = 0xFFFFFFF;
        int max = 0;
        for (size_t i = 0; i < dataPoints.numElements(); i++) {
            int value = *dataPoints.peek(i);
            if (value < min) {
                min = value;
            }
            if (value > max) {
                max = value;
            }
        }
        float range = max - min;
        display.setCursor(0, 0);
        display.printf("Min: %dW   Max: %dW ", min, max);
        display.display();
        for (int i = dataPoints.numElements() - 1; i > 0; i--) {
            int x0 = (WIDTH - (dataPoints.numElements() - i) * SAMPLE_WIDTH) - 1;
            int x1 = x0 + SAMPLE_WIDTH;

            float currentValue = (float)(*dataPoints.peek(i) - min);
            float lastValue = (float)(*dataPoints.peek(i - 1) - min);
            currentValue = (currentValue / range) * DATA_HEIGHT;
            lastValue = (lastValue / range) * DATA_HEIGHT;

            display.drawLine(x0, (HEIGHT - ((int)lastValue)) - (HEIGHT - DATA_HEIGHT - 10), x1, (HEIGHT - ((int)currentValue)) - (HEIGHT - DATA_HEIGHT - 10), SSD1306_WHITE);
        }
        display.setCursor(46, DATA_HEIGHT + 15);
        display.printf("Cur: %dW\n", *dataPoints.peek(dataPoints.numElements() - 1));
        display.display();
    }
    else {
        Serial.println(error.c_str());
    }

    Serial.println();
}

void setup() {
    delay(10);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        for (;;);
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("EnergyMeter by se7kn8");
    display.display();

    Serial.begin(115200);
    Serial.print("\n\n\n");
    WiFi.mode(WIFI_STA);
    display.println("Connecting to wifi");
    display.display();
    WiFi.begin(WIFI_NAME, WIFI_PASSWD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
    }

    randomSeed(micros());

    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    display.println(WiFi.localIP());
    display.display();

    client.setServer(MQTT_SERVER, 1883);
    client.setBufferSize(512);
    client.setCallback(mqtt_callback);
}

void reconnect() {
    while (!client.connected()) {
        Serial.println("Try to connect mqtt...");

        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWD)) {
            Serial.println("Connected");
            display.println("Connected to MQTT");
            display.display();
            client.subscribe(MQTT_TOPIC);
        }
        else {
            Serial.println("Failed");
            display.println("Error while connecting to mqtt");
            display.display();
        }
    }
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
}