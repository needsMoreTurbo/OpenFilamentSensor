/**
 * WiFi Network Scanner for ESP32-C3
 *
 * Scans for all available 2.4GHz WiFi networks and displays them.
 * This helps diagnose WiFi connectivity issues.
 */

#include <Arduino.h>
#include <WiFi.h>

void setup() {
    Serial.begin(115200);

    // Wait for serial port to connect (ESP32-C3 USB CDC)
    unsigned long startTime = millis();
    while (!Serial && (millis() - startTime < 5000)) {
        delay(10);
    }
    delay(500);

    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║   ESP32-C3 WiFi Network Scanner       ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println();

    // Set WiFi mode to station
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.println("Scanning for WiFi networks...\n");
}

void loop() {
    Serial.println("========================================");
    Serial.println("Starting new scan...");
    Serial.println("========================================");

    int numNetworks = WiFi.scanNetworks();

    Serial.println();
    if (numNetworks == 0) {
        Serial.println("No networks found!");
    } else {
        Serial.printf("Found %d networks:\n\n", numNetworks);

        // Print header
        Serial.println("  #  | SSID                             | Ch | RSSI | Encryption");
        Serial.println("-----+----------------------------------+----+------+-----------");

        // Print each network
        for (int i = 0; i < numNetworks; i++) {
            Serial.printf(" %2d  | %-32s | %2d | %4d | ",
                         i + 1,
                         WiFi.SSID(i).c_str(),
                         WiFi.channel(i),
                         WiFi.RSSI(i));

            // Print encryption type
            switch (WiFi.encryptionType(i)) {
                case WIFI_AUTH_OPEN:
                    Serial.println("Open");
                    break;
                case WIFI_AUTH_WEP:
                    Serial.println("WEP");
                    break;
                case WIFI_AUTH_WPA_PSK:
                    Serial.println("WPA");
                    break;
                case WIFI_AUTH_WPA2_PSK:
                    Serial.println("WPA2");
                    break;
                case WIFI_AUTH_WPA_WPA2_PSK:
                    Serial.println("WPA/WPA2");
                    break;
                case WIFI_AUTH_WPA2_ENTERPRISE:
                    Serial.println("WPA2-EAP");
                    break;
                default:
                    Serial.println("Unknown");
                    break;
            }
        }
    }

    Serial.println();
    Serial.println("Scan complete. Waiting 10 seconds before next scan...");
    Serial.println();

    // Wait before next scan
    delay(10000);
}
