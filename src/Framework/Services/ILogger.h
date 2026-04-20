#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Software::Services
{
    /** Logging severity levels supported by the framework. */
    enum class LogLevel
    {
        /** Lowest severity used for verbose traces. */
        Trace,
        /** Informational messages describing normal operation. */
        Info,
        /** Non-fatal issues that may need attention. */
        Warn,
        /** Serious issues that require investigation. */
        Error
    };

    /** Message payload used by logger implementations. */
    struct LogMessage
    {
        /** Severity of the message. */
        LogLevel level = LogLevel::Info;
        /** Message body provided by callers. */
        std::string message;
    };

    /** Minimal logger contract suitable for tooling-style apps. */
    class ILogger
    {
        // Functions
    public:
        /** Virtual destructor for interface safety. */
        virtual ~ILogger() = default;

        /** Writes a log entry at the requested level. */
        virtual void Log(LogLevel level, std::string_view message) = 0;

        /** Returns a snapshot of buffered messages for UI purposes. */
        virtual std::vector<LogMessage> Snapshot() const = 0;

        /** Convenience wrapper for trace messages. */
        void Trace(std::string_view message) { Log(LogLevel::Trace, message); }
        /** Convenience wrapper for info messages. */
        void Info(std::string_view message)  { Log(LogLevel::Info,  message); }
        /** Convenience wrapper for warnings. */
        void Warn(std::string_view message)  { Log(LogLevel::Warn,  message); }
        /** Convenience wrapper for errors. */
        void Error(std::string_view message) { Log(LogLevel::Error, message); }
    };

    using ILoggerPtr = std::shared_ptr<ILogger>;
}
