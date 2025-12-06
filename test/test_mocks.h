#ifndef TEST_MOCKS_H
#define TEST_MOCKS_H

// Prevent the real headers from being included in tests.
#define LOGGER_H
#define SETTINGS_DATA_H

class Logger {
public:
    static Logger& getInstance() { static Logger inst; return inst; }
    void log(const char* msg, int level = 0) { (void)msg; (void)level; }
    void log(const void* msg, int level = 0) { (void)msg; (void)level; }
    void logf(const char* fmt, ...) { (void)fmt; }
    void logf(int level, const char* fmt, ...) { (void)level; (void)fmt; }
    void logVerbose(const char* fmt, ...) { (void)fmt; }
    void logNormal(const char* fmt, ...) { (void)fmt; }
    void logPinValues(const char* fmt, ...) { (void)fmt; }
    int getLogLevel() const { return 0; }
    void setLogLevel(int level) { (void)level; }
};

class SettingsManager {
public:
    static SettingsManager& getInstance() { static SettingsManager inst; return inst; }
    bool getVerboseLogging() const { return false; }
    template<typename T>
    T getSetting(int offset) const { (void)offset; return T(); }
    template<typename T>
    void setSetting(int offset, T value) { (void)offset; (void)value; }
};

#define logger Logger::getInstance()
#define settingsManager SettingsManager::getInstance()

#endif  // TEST_MOCKS_H
