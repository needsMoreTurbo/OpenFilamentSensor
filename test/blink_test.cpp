/**
 * Simple blink and serial test for ESP32-C3
 *
 * This minimal program tests:
 * 1. Serial communication
 * 2. Basic boot functionality
 * 3. LED blinking (if you have an LED on GPIO8 or built-in LED)
 *
 * Should see "Hello!" messages every second in serial monitor at 115200 baud
 */

#include <Arduino.h>

// Try GPIO 8 which is commonly used for built-in LED on ESP32-C3
#define LED_PIN 8

void setup() {
    // Initialize serial at 115200 baud
    Serial.begin(115200);

    // ESP32-C3 with USB CDC needs time to establish connection
    // Wait for serial port to connect (or timeout after 5 seconds)
    unsigned long startTime = millis();
    while (!Serial && (millis() - startTime < 5000)) {
        delay(10);
    }

    // Additional delay to ensure connection is stable
    delay(500);

    // Print startup message multiple times to ensure it's visible
    for (int i = 0; i < 10; i++) {
        Serial.println("\n\n========================================");
        Serial.println("ESP32-C3 BOOT TEST - STARTING UP!");
        Serial.println("========================================");
        Serial.printf("Boot iteration: %d\n", i + 1);
        delay(100);
    }

    // Try to configure LED pin (won't crash if pin doesn't have LED)
    pinMode(LED_PIN, OUTPUT);

    Serial.println("\nSetup complete! Starting main loop...");
    Serial.println("You should see 'Hello!' messages every second.");
    Serial.println("If you see this, your ESP32-C3 is working!\n");
}

void loop() {
    static int counter = 0;

    // Print message
    Serial.printf("Hello! Count: %d (uptime: %lu ms)\n", counter++, millis());

    // Toggle LED
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(900);
}
