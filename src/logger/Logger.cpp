// SPDX-License-Identifier: MIT
#include "Logger.h"

ILogger* LoggerFactory::createLogger(const std::string& type) {
    if (type == "console") {
        return new ConsoleLogger();
    } else if (type == "file") {
        return new FileLogger();
    }
    return nullptr;
}
