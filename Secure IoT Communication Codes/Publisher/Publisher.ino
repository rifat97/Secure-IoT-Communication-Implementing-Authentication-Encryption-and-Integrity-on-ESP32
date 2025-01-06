#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include "mbedtls/md.h"
#include <nvs_flash.h>
#include <nvs.h>

// DHT11 setup
#define DHTPIN 26
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// WiFi and MQTT credentials
const char* ssid = "your_ssid";
const char* password = "your_password";
const char* mqtt_server = "ip_address";

// NVS namespace and key for HMAC
const char* HMAC_NAMESPACE = "secure_hmac";
const char* HMAC_KEY = "hmac_key";

String hmacKey = ""; // Key will be loaded from NVS

WiFiClient espClient;
PubSubClient client(espClient);

// Function to calculate HMAC
String calculateHMAC(String message, String key) {
    unsigned char hash[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1); // HMAC mode
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key.c_str(), key.length());
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)message.c_str(), message.length());
    mbedtls_md_hmac_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    // Convert hash to HEX
    String hmac = "";
    for (int i = 0; i < 32; i++) {
        if (hash[i] < 16) hmac += "0"; // Add leading zero
        hmac += String(hash[i], HEX);
    }
    return hmac;
}

void loadHMACKey() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    nvs_handle_t nvs_handle;
    err = nvs_open(HMAC_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t required_size;
        nvs_get_str(nvs_handle, HMAC_KEY, NULL, &required_size);
        char* keyBuffer = new char[required_size];
        nvs_get_str(nvs_handle, HMAC_KEY, keyBuffer, &required_size);
        hmacKey = String(keyBuffer);
        delete[] keyBuffer;
        nvs_close(nvs_handle);
        Serial.println("HMAC Key loaded from NVS: " + hmacKey);
    } else {
        Serial.println("Failed to load HMAC key from NVS!");
    }
}

void WiFiSetup() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    client.setServer(mqtt_server, 1883);
    reconnect();
}

void reconnect() {
    while (!client.connected()) {
        if (client.connect("Publisher")) {
            // Connected to MQTT broker
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" trying again in 5 seconds");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    dht.begin();
    WiFiSetup();

    // Load HMAC key from NVS
    loadHMACKey();
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    // Create clean payload for Node-RED (temperature and humidity only)
    String cleanPayload = "{\"temperature\": " + String(temperature) + ", \"humidity\": " + String(humidity) + "}";

    // Calculate HMAC for the sensor data
    String hmac = calculateHMAC(cleanPayload, hmacKey);

    // Debugging Output
    Serial.println("======================================");
    Serial.println("[Publisher]");
    Serial.println("Sensor Data: " + cleanPayload);
    Serial.println("Encrypted Data (HMAC): " + hmac);
    Serial.println("======================================");

    // Publish clean data to Node-RED
    client.publish("sensor/clean_data", cleanPayload.c_str());

    // Publish encrypted HMAC to MQTTX
    client.publish("sensor/encrypted_data", hmac.c_str());

    delay(2000);
}
