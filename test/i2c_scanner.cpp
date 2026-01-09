/**
 * I2C Scanner for ESP32-C3 SuperMini with OLED
 *
 * This program scans common I2C pin combinations to find the OLED display.
 * It will test multiple pin pairs and report which ones have devices connected.
 *
 * Upload this to your ESP32-C3 and open the serial monitor at 115200 baud.
 */

#include <Arduino.h>
#include <Wire.h>

// Common I2C pin combinations for ESP32-C3 SuperMini boards
const struct {
    int sda;
    int scl;
    const char* description;
} pinCombos[] = {
    {5, 6, "GPIO5(SDA)/GPIO6(SCL) - Common config 1"},
    {8, 9, "GPIO8(SDA)/GPIO9(SCL) - Common config 2"},
    {0, 1, "GPIO0(SDA)/GPIO1(SCL) - Common config 3"},
    {4, 5, "GPIO4(SDA)/GPIO5(SCL) - Alternative config"},
    {6, 7, "GPIO6(SDA)/GPIO7(SCL) - Alternative config"},
    {10, 8, "GPIO10(SDA)/GPIO8(SCL) - Alternative config"},
};

void scanI2C(int sda, int scl) {
    Serial.println("========================================");
    Serial.printf("Testing SDA=%d, SCL=%d\n", sda, scl);
    Serial.println("========================================");

    // Initialize I2C with these pins
    Wire.end(); // End any previous I2C
    delay(100);

    Wire.begin(sda, scl);
    delay(100);

    bool found = false;
    int deviceCount = 0;

    Serial.println("Scanning I2C addresses 0x00-0x7F...");

    for (byte address = 0x00; address <= 0x7F; address++) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();

        if (error == 0) {
            deviceCount++;
            found = true;
            Serial.printf("  ✓ Device found at address 0x%02X", address);

            // Identify common devices
            if (address == 0x3C || address == 0x3D) {
                Serial.print(" <-- LIKELY OLED DISPLAY!");
            } else if (address == 0x27 || address == 0x3F) {
                Serial.print(" (LCD display)");
            } else if (address == 0x68) {
                Serial.print(" (RTC or IMU)");
            }
            Serial.println();
        }
    }

    if (!found) {
        Serial.println("  No I2C devices found on these pins.");
    } else {
        Serial.printf("\n  Total devices found: %d\n", deviceCount);
    }

    Serial.println();
    delay(500);
}

void setup() {
    Serial.begin(115200);

    // Wait for serial port to connect (ESP32-C3 USB CDC)
    unsigned long startTime = millis();
    while (!Serial && (millis() - startTime < 5000)) {
        delay(10);
    }
    delay(500); // Additional stability delay

    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║   ESP32-C3 I2C Scanner for OLED       ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println();
    Serial.println("Scanning common I2C pin combinations...");
    Serial.println();

    // Test each pin combination
    for (size_t i = 0; i < sizeof(pinCombos) / sizeof(pinCombos[0]); i++) {
        Serial.printf("Test %d/%d: %s\n",
                     i + 1,
                     sizeof(pinCombos) / sizeof(pinCombos[0]),
                     pinCombos[i].description);
        scanI2C(pinCombos[i].sda, pinCombos[i].scl);
        delay(500);
    }

    Serial.println("========================================");
    Serial.println("Scan complete!");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Look for addresses marked with '<-- LIKELY OLED DISPLAY!'");
    Serial.println("Common OLED addresses: 0x3C or 0x3D");
    Serial.println();
    Serial.println("Update your platformio.ini with the correct pins:");
    Serial.println("  -D OLED_SDA_PIN=X");
    Serial.println("  -D OLED_SCL_PIN=Y");
    Serial.println();
}

void loop() {
    // Just blink LED to show it's running
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 1000) {
        lastBlink = millis();
        Serial.println("Scanner idle. Reset to scan again.");
    }
    delay(1000);
}
