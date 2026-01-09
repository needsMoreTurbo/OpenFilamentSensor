#include "SystemServices.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <time.h>

#include "ElegooCC.h"
#include "Logger.h"
#include "SettingsManager.h"

namespace
{
constexpr unsigned long WIFI_CHECK_INTERVAL_MS    = 30000;
constexpr unsigned long WIFI_RECONNECT_TIMEOUT_MS = 10000;
constexpr unsigned long NTP_SYNC_INTERVAL_MS      = 3600000;
const char*             NTP_SERVER                = "pool.ntp.org";
}  // namespace

SystemServices systemServices;

void SystemServices::begin()
{
    wifiSetupAttempted         = false;
    wifiSetupAttemptedThisLoop = false;
    stationConnected           = false;
    isReconnecting             = false;
    ntpConfigured              = false;
    lastWifiCheck              = 0;
    wifiReconnectStart         = 0;
    lastNTPSyncAttempt         = 0;
    lastHeapCheck              = 0;
}

void SystemServices::loop()
{
    unsigned long currentTime = millis();
    wifiSetupAttemptedThisLoop = false;

    if (!wifiSetupAttempted)
    {
        wifiSetupAttempted         = true;
        wifiSetupAttemptedThisLoop = true;
        bool success = wifiSetup();

        if (success)
        {
            logger.log(F("WiFi connected successfully"));
            logger.logf("IP Address: %s", WiFi.localIP().toString().c_str());
        }
        else
        {
            if (settingsManager.isAPMode())
            {
                logger.log(F("WiFi setup complete - running in AP mode"));
                logger.logf("AP IP Address: %s", WiFi.softAPIP().toString().c_str());
            }
            else
            {
                logger.log(F("WiFi setup attempted - will retry connection in background"));
            }
        }
        return;
    }

    handleWifiReconnectRequest();

    stationConnected = (!settingsManager.isAPMode() && WiFi.status() == WL_CONNECTED);

    if (stationConnected)
    {
        // Calculate GMT offset in seconds:
        // Browser returns minutes difference between UTC and local (positive if west of UTC)
        // e.g., NY (UTC-5) -> 300.
        // POSIX/configTime wants offset from UTC in seconds (negative if west of UTC?)
        // Wait, standard configTime: gmtOffset_sec.
        // If I want UTC+1, I pass 3600.
        // If I want UTC-5, I pass -18000.
        // Browser 300 -> We want -18000.
        // (-1) * 300 * 60 = -18000. Correct.
        long gmtOffset_sec = -1 * settingsManager.getTimezoneOffsetMinutes() * 60;

        if (!ntpConfigured)
        {
            configTime(gmtOffset_sec, 0, NTP_SERVER);
            syncTimeWithNTP(currentTime);
            logger.log("NTP setup complete");
            ntpConfigured = true;
        }
        else if (currentTime - lastNTPSyncAttempt >= NTP_SYNC_INTERVAL_MS)
        {
            syncTimeWithNTP(currentTime);
        }
    }
    else if (!settingsManager.isAPMode() && currentTime - lastWifiCheck >= WIFI_CHECK_INTERVAL_MS)
    {
        lastWifiCheck = currentTime;
        checkWifiConnection();
    }

    monitorHeap(currentTime);
}

bool SystemServices::wifiReady() const
{
    return stationConnected;
}

bool SystemServices::runningInAPMode() const
{
    return settingsManager.isAPMode();
}

bool SystemServices::hasAttemptedWifiSetup() const
{
    return wifiSetupAttempted;
}

bool SystemServices::shouldYieldForSetup() const
{
    return wifiSetupAttemptedThisLoop;
}

unsigned long SystemServices::currentEpoch() const
{
    time_t now;
    time(&now);
    return now;
}

void SystemServices::failWifi()
{
    stationConnected = false;

    if (!settingsManager.getHasConnected())
    {
        settingsManager.setAPMode(true);
        bool saved = settingsManager.save();
        elegooCC.refreshCaches();
        if (saved)
        {
            logger.log("Failed to connect to wifi, reverted to AP mode (first connection attempt)");
            logger.log("Restarting to enter AP mode...");
            delay(1000);  // Give time for serial output
            ESP.restart();
        }
        else
        {
            logger.log("Failed to update settings");
        }
    }
    else
    {
        logger.log("WiFi connection failed, retrying in 30 seconds");
    }
}

void SystemServices::startAPMode()
{
    stationConnected = false;
    logger.log("Starting AP mode");

    // Ensure WiFi is properly cleaned up before starting AP
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    delay(200);

    // Set WiFi country code (helps with regulatory compliance and visibility)
    wifi_country_t country = {
        .cc = "US",
        .schan = 1,
        .nchan = 11,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    esp_wifi_set_country(&country);

    // Explicitly set WiFi to AP mode only
    WiFi.mode(WIFI_AP);
    delay(200);

    // Use channel 11 (very common) and explicit parameters
    const char* apSSID = "OFS";
    bool apStarted = WiFi.softAP(apSSID, "", 11, false, 4);

    if (apStarted) {
        logger.log("AP started successfully");
        logger.logf("AP SSID: %s", apSSID);
        logger.logf("AP IP Address: %s", WiFi.softAPIP().toString().c_str());
        logger.logf("AP MAC Address: %s", WiFi.softAPmacAddress().c_str());
        logger.logf("AP Station Count: %d", WiFi.softAPgetStationNum());

        // Check if AP is actually broadcasting
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        logger.logf("WiFi Mode: %d (1=STA, 2=AP, 3=STA+AP)", mode);

        // Get and log the actual channel being used
        uint8_t primary;
        wifi_second_chan_t second;
        esp_wifi_get_channel(&primary, &second);
        logger.logf("AP Channel: %d", primary);
    } else {
        logger.log("ERROR: Failed to start AP!");
    }

    if (!MDNS.begin("OFS"))
    {
        logger.log("Error setting up MDNS responder in AP mode!");
    }
}

void SystemServices::handleSuccessfulWifiConnection()
{
    stationConnected = true;
    logger.log("WiFi Connected");
    logger.logf("IP Address: %s", WiFi.localIP().toString().c_str());

    if (!settingsManager.getHasConnected())
    {
        settingsManager.setHasConnected(true);
        settingsManager.save();
        elegooCC.refreshCaches();
        logger.log("First successful WiFi connection recorded");
    }

    isReconnecting = false;

    MDNS.end();
    if (!MDNS.begin("OFS"))
    {
        logger.log("Error setting up MDNS responder!");
    }
}

bool SystemServices::connectToWifiStation(bool isReconnect)
{
    // Fully disconnect and clean up before attempting connection
    WiFi.disconnect(true);
    delay(100);

    WiFi.mode(WIFI_STA);
    delay(100);

    const char* action = isReconnect ? "Reconnecting to" : "Connecting to";
    logger.logf("%s WiFi: %s", action, settingsManager.getSSID().c_str());
    logger.logf("WiFi password length: %d", settingsManager.getPassword().length());

    // Explicitly set channel to 0 (auto-detect) to help with connection
    WiFi.begin(settingsManager.getSSID().c_str(), settingsManager.getPassword().c_str(), 0);

    const unsigned long CONNECT_TIMEOUT_MS = 20000;  // Increased to 20 seconds
    unsigned long       startTime          = millis();

    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < CONNECT_TIMEOUT_MS)
    {
        wl_status_t currentStatus = WiFi.status();

        // Log status changes for debugging
        static wl_status_t lastStatus = WL_IDLE_STATUS;
        if (currentStatus != lastStatus) {
            logger.logf("WiFi status changed: %d", currentStatus);
            lastStatus = currentStatus;
        }

        Serial.print('.');
        vTaskDelay(pdMS_TO_TICKS(500));
        yield();
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        handleSuccessfulWifiConnection();
        return true;
    }

    // Log detailed failure reason
    wl_status_t status = WiFi.status();
    logger.logf("WiFi connection failed with status: %d", status);
    switch(status) {
        case WL_NO_SSID_AVAIL:
            logger.log("Error: SSID not found");
            break;
        case WL_CONNECT_FAILED:
            logger.log("Error: Connection failed (wrong password?)");
            break;
        case WL_CONNECTION_LOST:
            logger.log("Error: Connection lost");
            break;
        case WL_DISCONNECTED:
            logger.log("Error: Disconnected");
            break;
        default:
            logger.logf("Error: Unknown status %d", status);
            break;
    }

    if (isReconnect)
    {
        logger.log("Failed to connect with new WiFi credentials");
    }
    else
    {
        failWifi();
    }

    stationConnected = false;
    return false;
}

void SystemServices::cleanupWifiConnections()
{
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    delay(1000);
}

bool SystemServices::wifiSetup()
{
    if (settingsManager.isAPMode())
    {
        startAPMode();
        return false;
    }

    return connectToWifiStation(false);
}

bool SystemServices::reconnectWifiWithNewCredentials()
{
    logger.log("Applying new WiFi credentials...");
    cleanupWifiConnections();

    if (settingsManager.isAPMode())
    {
        logger.log("Switching to AP mode");
        startAPMode();
        return false;
    }

    logger.log("Connecting to WiFi station mode with new credentials...");
    return connectToWifiStation(true);
}

void SystemServices::checkWifiConnection()
{
    if (settingsManager.isAPMode())
    {
        stationConnected = false;
        return;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        stationConnected = false;

        if (!isReconnecting)
        {
            logger.log("WiFi disconnected, attempting to reconnect...");
            WiFi.begin(settingsManager.getSSID().c_str(), settingsManager.getPassword().c_str());
            wifiReconnectStart = millis();
            isReconnecting     = true;
        }
        else if (millis() - wifiReconnectStart >= WIFI_RECONNECT_TIMEOUT_MS)
        {
            failWifi();
        }
    }
    else if (isReconnecting)
    {
        logger.log("WiFi reconnected successfully");
        isReconnecting = false;

        if (!settingsManager.getHasConnected())
        {
            settingsManager.setHasConnected(true);
            settingsManager.save();
            elegooCC.refreshCaches();
        }
    }
}

void SystemServices::syncTimeWithNTP(unsigned long currentTime)
{
    struct tm timeinfo;
    lastNTPSyncAttempt = currentTime;
    if (getLocalTime(&timeinfo))
    {
        logger.log("NTP time synchronization successful");
    }
    else
    {
        logger.log("NTP time synchronization failed");
    }
}

void SystemServices::monitorHeap(unsigned long currentTime)
{
    if (currentTime - lastHeapCheck <= 300000)
    {
        return;
    }

    lastHeapCheck = currentTime;

    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minHeap  = ESP.getMinFreeHeap();
    uint32_t maxAlloc = ESP.getMaxAllocHeap();

    float fragmentation = 100.0f * (1.0f - ((float)maxAlloc / (float)freeHeap));

    logger.logf(LOG_VERBOSE, "Heap: free=%lu min=%lu maxAlloc=%lu frag=%.1f%%",
                freeHeap, minHeap, maxAlloc, fragmentation);

    if (fragmentation > 30.0f)
    {
        logger.log(F("WARNING: Heap fragmentation high!"), LOG_NORMAL);
    }

    if (minHeap < 2000)
    {
        logger.logf(LOG_NORMAL, "CRITICAL: Low memory! Min heap: %lu", minHeap);
    }
}

void SystemServices::handleWifiReconnectRequest()
{
    if (!settingsManager.requestWifiReconnect)
    {
        return;
    }

    settingsManager.requestWifiReconnect = false;
    reconnectWifiWithNewCredentials();
}

unsigned long getTime()
{
    // Return epoch time for now - Logger will format it as local time
    return systemServices.currentEpoch();
}
