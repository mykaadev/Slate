#include "App/Slate/Extensions/SlateShellBridge.h"

#include <utility>

namespace Software::Slate
{
    void SlateShellBridge::RunCommand(std::string commandLine)
    {
        ShellRequest request;
        request.kind = ShellRequest::Kind::Command;
        request.text = std::move(commandLine);
        m_requests.push_back(std::move(request));
    }

    void SlateShellBridge::Notify(std::string message, bool isError)
    {
        ShellRequest request;
        request.kind = isError ? ShellRequest::Kind::Error : ShellRequest::Kind::Status;
        request.text = std::move(message);
        m_requests.push_back(std::move(request));
    }

    void SlateShellBridge::Prompt(std::string title, std::string initialValue,
                                  std::function<void(const std::string&)> onSubmit)
    {
        ShellRequest request;
        request.kind = ShellRequest::Kind::Prompt;
        request.text = std::move(title);
        request.value = std::move(initialValue);
        request.onPrompt = std::move(onSubmit);
        m_requests.push_back(std::move(request));
    }

    void SlateShellBridge::Confirm(std::string message, std::function<void(bool)> onAnswer)
    {
        ShellRequest request;
        request.kind = ShellRequest::Kind::Confirm;
        request.text = std::move(message);
        request.onConfirm = std::move(onAnswer);
        m_requests.push_back(std::move(request));
    }

    bool SlateShellBridge::Pop(ShellRequest* request)
    {
        if (!request || m_requests.empty())
        {
            return false;
        }
        *request = std::move(m_requests.front());
        m_requests.pop_front();
        return true;
    }

    void SlateShellBridge::Defer(ShellRequest request)
    {
        m_requests.push_front(std::move(request));
    }
}
