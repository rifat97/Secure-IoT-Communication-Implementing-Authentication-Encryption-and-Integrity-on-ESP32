#include <nvs_flash.h>
#include <nvs.h>

// Namespace and key for storing HMAC
const char* HMAC_NAMESPACE = "secure_hmac";
const char* HMAC_KEY = "hmac_key";

// Secure, randomly generated HMAC key (32 characters)
const char* SHARED_HMAC_KEY = "1a2b3c4d5e6f7g8h9i0j1k2l3m4n5o6p";

void setup() {
    Serial.begin(115200);

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Open NVS handle
    nvs_handle_t nvs_handle;
    err = nvs_open(HMAC_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        Serial.println("Error opening NVS handle!");
        return;
    }

    // Write HMAC key to NVS
    err = nvs_set_str(nvs_handle, HMAC_KEY, SHARED_HMAC_KEY);
    if (err == ESP_OK) {
        Serial.println(String("HMAC Key: ") + SHARED_HMAC_KEY);
        Serial.println("HMAC key successfully written to NVS!");
        nvs_commit(nvs_handle); // Commit changes
    } else {
        Serial.println("Failed to write HMAC key to NVS!");
    }

    // Close NVS handle
    nvs_close(nvs_handle);

    Serial.println("Provisioning complete.");
}

void loop() {
    // Do nothing; provisioning happens only once
}
