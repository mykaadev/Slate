#include "Services/ConsoleLogger.h"

#include <cstdio>

namespace Software::Services
{
    ConsoleLogger::ConsoleLogger(std::size_t maxMessages)
        : m_maxMessages(maxMessages)
    {
    }

    static const char* LevelPrefix(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Trace: return "[trace]";
        case LogLevel::Info:  return "[info ]";
        case LogLevel::Warn:  return "[warn ]";
        case LogLevel::Error: return "[error]";
        default:              return "[info ]";
        }
    }

    void ConsoleLogger::Log(LogLevel level, std::string_view message)
    {
        {
            std::scoped_lock lock(m_mutex);
            if (m_messages.size() >= m_maxMessages)
            {
                m_messages.pop_front();
            }
            m_messages.push_back(LogMessage{level, std::string(message)});
        }

        // Keep it dead-simple and dependency-free.
        const char* prefix = LevelPrefix(level);
        if (level == LogLevel::Error)
        {
            std::fprintf(stderr, "%s %.*s\n", prefix, static_cast<int>(message.size()), message.data());
        }
        else
        {
            std::fprintf(stdout, "%s %.*s\n", prefix, static_cast<int>(message.size()), message.data());
        }
    }

    std::vector<LogMessage> ConsoleLogger::Snapshot() const
    {
        std::scoped_lock lock(m_mutex);
        return std::vector<LogMessage>(m_messages.begin(), m_messages.end());
    }
}
