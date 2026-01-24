#pragma once

#include <string>
#include <iostream>
#include <memory>
#include <map>


struct ILogger {
    virtual ~ILogger() = default;
    virtual void log(const std::string& message) = 0;
};

struct ConsoleLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << "ConsoleLogger: " << message << std::endl;
    }
};

struct FileLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << "FileLogger: " << message << std::endl;
    }
};

struct LoggerFactory {
    static ILogger* createLogger(const std::string& type);
};

ILogger* LoggerFactory::createLogger(const std::string& type) {
    if (type == "console") {
        return new ConsoleLogger();
    } else if (type == "file") {
        return new FileLogger();
    }
    return nullptr;
}




 struct BugLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << "Bug: " << message << std::endl;
    }
};

struct WarningLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << "Warning: " << message << std::endl;
    }
};

struct InfoLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << "Info: " << message << std::endl;
    }
};

struct DebugLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << "Debug: " << message << std::endl;
    }
};

struct TraceLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << "Trace: " << message << std::endl;
    }
};

struct FatalLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << "Fatal: " << message << std::endl;
    }
};

struct ErrorLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << "Error: " << message << std::endl;
    }
};

struct CriticalLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << "Critical: " << message << std::endl;
    }
};

struct Log{
    ILogger* logger;
    std::map<std::string, std::unique_ptr<ILogger>> loggerDispatch;
    Log() {
        loggerDispatch["bug"] = std::make_unique<BugLogger>();
        loggerDispatch["warning"] = std::make_unique<WarningLogger>();
        loggerDispatch["info"] = std::make_unique<InfoLogger>();
        loggerDispatch["debug"] = std::make_unique<DebugLogger>();
        loggerDispatch["trace"] = std::make_unique<TraceLogger>();
        loggerDispatch["fatal"] = std::make_unique<FatalLogger>();
        loggerDispatch["error"] = std::make_unique<ErrorLogger>();
        loggerDispatch["critical"] = std::make_unique<CriticalLogger>();
    }
    void log(const std::string& type, const std::string& message) {
        try {
            loggerDispatch[type]->log(message);
        } catch (const std::exception& e) {
            std::cout << "Logger not found" << std::endl;
        }
        
    }
};

