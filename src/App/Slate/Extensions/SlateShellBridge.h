#pragma once

#include <deque>
#include <functional>
#include <string>

namespace Software::Slate
{
    // One request a service or plugin wants the active shell to perform on the next frame
    struct ShellRequest
    {
        // Lists the request kinds the shell understands
        enum class Kind
        {
            Command,
            Status,
            Error,
            Prompt,
            Confirm
        };

        // Stores the request kind
        Kind kind = Kind::Command;
        // Stores the command line, status text, prompt title, or confirm message
        std::string text;
        // Stores the initial prompt value
        std::string value;
        // Receives the submitted prompt value
        std::function<void(const std::string&)> onPrompt;
        // Receives the confirm answer
        std::function<void(bool)> onConfirm;
    };

    // Queues shell actions so code outside the route adapters never calls into a mode directly
    class SlateShellBridge
    {
    public:
        // Queues a command line such as "today" or "slate.search.show notes"
        void RunCommand(std::string commandLine);
        // Queues a status line message
        void Notify(std::string message, bool isError = false);
        // Queues a single line prompt
        void Prompt(std::string title, std::string initialValue, std::function<void(const std::string&)> onSubmit);
        // Queues a yes/no confirm
        void Confirm(std::string message, std::function<void(bool)> onAnswer);
        // Pops the oldest request
        bool Pop(ShellRequest* request);
        // Puts a request back at the front when the shell cannot take it this frame
        void Defer(ShellRequest request);

    private:
        std::deque<ShellRequest> m_requests;
    };
}
