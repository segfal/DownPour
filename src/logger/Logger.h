#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <string>

// ANSI color codes
namespace LogColors {
const std::string RESET         = "\033[0m";
const std::string RED           = "\033[31m";
const std::string GREEN         = "\033[32m";
const std::string YELLOW        = "\033[33m";
const std::string BLUE          = "\033[34m";
const std::string MAGENTA       = "\033[35m";
const std::string CYAN          = "\033[36m";
const std::string WHITE         = "\033[37m";
const std::string BOLD          = "\033[1m";
const std::string BRIGHT_RED    = "\033[91m";
const std::string BRIGHT_YELLOW = "\033[93m";
}  // namespace LogColors

struct ILogger {
    virtual ~ILogger()                           = default;
    virtual void log(const std::string& message) = 0;
};

struct ConsoleLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << LogColors::CYAN << "ConsoleLogger: " << LogColors::RESET << message << std::endl;
    }
};

struct FileLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << LogColors::BLUE << "FileLogger: " << LogColors::RESET << message << std::endl;
    }
};

struct LoggerFactory {
    static ILogger* createLogger(const std::string& type);
};

struct BugLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << LogColors::MAGENTA << LogColors::BOLD << "Bug: " << LogColors::RESET << message << std::endl;
    }
};

struct WarningLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << LogColors::BRIGHT_YELLOW << "Warning: " << LogColors::RESET << message << std::endl;
    }
};

struct InfoLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << LogColors::CYAN << "Info: " << LogColors::RESET << message << std::endl;
    }
};

struct DebugLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << LogColors::BLUE << "Debug: " << LogColors::RESET << message << std::endl;
    }
};

struct TraceLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << LogColors::WHITE << "Trace: " << LogColors::RESET << message << std::endl;
    }
};

struct FatalLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << LogColors::BRIGHT_RED << LogColors::BOLD << "Fatal: " << LogColors::RESET << LogColors::RED
                  << message << LogColors::RESET << std::endl;
    }
};

struct ErrorLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << LogColors::RED << "Error: " << LogColors::RESET << message << std::endl;
    }
};

struct CriticalLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << LogColors::BRIGHT_RED << "Critical: " << LogColors::RESET << message << std::endl;
    }
};

struct PositionLogger : public ILogger {
    void log(const std::string& message) override {
        std::cout << LogColors::GREEN << "Position: " << LogColors::RESET << message << std::endl;
    }
};

struct Log {
    ILogger*                                        logger;
    std::map<std::string, std::unique_ptr<ILogger>> loggerDispatch;
    Log() {
        loggerDispatch["bug"]      = std::make_unique<BugLogger>();
        loggerDispatch["warning"]  = std::make_unique<WarningLogger>();
        loggerDispatch["info"]     = std::make_unique<InfoLogger>();
        loggerDispatch["debug"]    = std::make_unique<DebugLogger>();
        loggerDispatch["trace"]    = std::make_unique<TraceLogger>();
        loggerDispatch["fatal"]    = std::make_unique<FatalLogger>();
        loggerDispatch["error"]    = std::make_unique<ErrorLogger>();
        loggerDispatch["critical"] = std::make_unique<CriticalLogger>();
        loggerDispatch["position"] = std::make_unique<PositionLogger>();
    }
    void log(const std::string& type, const std::string& message) {
        try {
            loggerDispatch[type]->log(message);
        } catch (const std::exception& e) {
            std::cout << "Logger not found" << std::endl;
        }
    }
};
