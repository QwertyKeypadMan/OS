#include "core/Logger.h"

namespace kaya::designer {

void Logger::info(std::string message)
{
    messages_.push_back("[info] " + std::move(message));
}

void Logger::warn(std::string message)
{
    messages_.push_back("[warn] " + std::move(message));
}

void Logger::error(std::string message)
{
    messages_.push_back("[error] " + std::move(message));
}

void Logger::clear()
{
    messages_.clear();
}

}

