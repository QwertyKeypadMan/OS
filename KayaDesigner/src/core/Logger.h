#pragma once

#include <string>
#include <vector>

namespace kaya::designer {

class Logger {
public:
    void info(std::string message);
    void warn(std::string message);
    void error(std::string message);
    void clear();

    [[nodiscard]] const std::vector<std::string> &messages() const { return messages_; }

private:
    std::vector<std::string> messages_;
};

}

