#pragma once

#include "Services/ILogger.h"

#include <mutex>
#include <deque>

namespace Software::Services
{
    /** Thread-safe in-memory logger that also writes to stdout/stderr. */
    class ConsoleLogger final : public ILogger
    {
    public:
        explicit ConsoleLogger(std::size_t maxMessages = 256);

        void Log(LogLevel level, std::string_view message) override;
        std::vector<LogMessage> Snapshot() const override;

    private:
        std::size_t m_maxMessages = 256;
        mutable std::mutex m_mutex;
        std::deque<LogMessage> m_messages;
    };
}
