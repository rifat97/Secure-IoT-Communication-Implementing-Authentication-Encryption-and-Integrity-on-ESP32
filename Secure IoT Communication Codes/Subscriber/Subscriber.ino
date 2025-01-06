#include <WiFi.h>
#include <PubSubClient.h>
#include "mbedtls/md.h"
#include <nvs_flash.h>
#include <nvs.h>

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
        Serial.println("HMAC Key from NVS: " + hmacKey);
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
    client.setCallback(callback);
    reconnect();
}

void reconnect() {
    while (!client.connected()) {
        if (client.connect("Subscriber")) {
            client.subscribe("sensor/clean_data");
            client.subscribe("sensor/encrypted_data");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" trying again in 5 seconds");
            delay(5000);
        }
    }
}

void callback(char* topic, byte* message, unsigned int length) {
    static String cleanPayload = "";
    static String encryptedHMAC = "";

    String payload = "";
    for (int i = 0; i < length; i++) {
        payload += (char)message[i];
    }

    if (String(topic) == "sensor/clean_data") {
        cleanPayload = payload;
    } else if (String(topic) == "sensor/encrypted_data") {
        String incomingHMAC = payload;
        encryptedHMAC = calculateHMAC(cleanPayload, hmacKey);

        Serial.println("======================================");
        Serial.println("[Subscriber]");
        Serial.println("Received Encrypted Data (HMAC): " + incomingHMAC);
        Serial.println("Sensor Data: " + cleanPayload);
        Serial.println("Calculated HMAC: " + encryptedHMAC);

        if (incomingHMAC == encryptedHMAC) {
            Serial.println("Integrity check: PASSED");
        } else {
            Serial.println("Integrity check: FAILED");
        }
        Serial.println("======================================");

        cleanPayload = "";
        encryptedHMAC = "";
    }
}

void setup() {
    Serial.begin(115200);
    loadHMACKey();
    WiFiSetup();
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
}
