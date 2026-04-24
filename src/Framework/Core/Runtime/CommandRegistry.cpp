#include "CommandRegistry.h"

#include "Core/Runtime/AppContext.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace Software::Core::Runtime
{
    CommandResult CommandResult::NotFound(std::string input)
    {
        CommandResult result;
        result.handled = false;
        result.succeeded = false;
        result.message = std::move(input);
        return result;
    }

    CommandResult CommandResult::Handled(std::string id, std::string message)
    {
        CommandResult result;
        result.handled = true;
        result.succeeded = true;
        result.commandId = std::move(id);
        result.message = std::move(message);
        return result;
    }

    CommandResult CommandResult::Failed(std::string id, std::string message)
    {
        CommandResult result;
        result.handled = true;
        result.succeeded = false;
        result.commandId = std::move(id);
        result.message = std::move(message);
        return result;
    }

    void CommandRegistry::Register(CommandDefinition definition)
    {
        if (definition.id.empty())
        {
            return;
        }

        if (definition.aliases.empty())
        {
            definition.aliases.push_back(definition.id);
        }

        if (Entry* existing = FindMutable(definition.id))
        {
            for (const auto& alias : existing->definition.aliases)
            {
                m_aliasToId.erase(NormalizeToken(alias));
            }
            existing->definition = std::move(definition);
            for (const auto& alias : existing->definition.aliases)
            {
                const std::string key = NormalizeToken(alias);
                if (!key.empty())
                {
                    m_aliasToId[key] = existing->definition.id;
                }
            }
            return;
        }

        Entry entry;
        entry.definition = std::move(definition);
        for (const auto& alias : entry.definition.aliases)
        {
            const std::string key = NormalizeToken(alias);
            if (!key.empty())
            {
                m_aliasToId[key] = entry.definition.id;
            }
        }
        m_entries.push_back(std::move(entry));
    }

    void CommandRegistry::Clear()
    {
        m_entries.clear();
        m_aliasToId.clear();
    }

    bool CommandRegistry::Resolve(const std::string& input, CommandRequest* outRequest) const
    {
        const std::string trimmed = Trim(input);
        if (trimmed.empty())
        {
            return false;
        }

        std::string bestAlias;
        std::string bestId;

        for (const auto& pair : m_aliasToId)
        {
            const std::string& alias = pair.first;
            if (alias.empty() || alias.size() < bestAlias.size())
            {
                continue;
            }

            if (trimmed.size() < alias.size())
            {
                continue;
            }

            std::string prefix = trimmed.substr(0, alias.size());
            prefix = NormalizeToken(prefix);
            if (prefix != alias)
            {
                continue;
            }

            if (trimmed.size() > alias.size() && trimmed[alias.size()] != ' ' && trimmed[alias.size()] != '\t')
            {
                continue;
            }

            const auto* definition = Find(pair.second);
            if (!definition)
            {
                continue;
            }

            if (trimmed.size() > alias.size() && !definition->acceptsArguments)
            {
                continue;
            }

            bestAlias = alias;
            bestId = pair.second;
        }

        if (bestId.empty())
        {
            return false;
        }

        if (outRequest)
        {
            outRequest->raw = input;
            outRequest->id = bestId;
            outRequest->token = bestAlias;
            outRequest->arguments = Trim(trimmed.substr(bestAlias.size()));
        }
        return true;
    }

    CommandResult CommandRegistry::Execute(const std::string& input, AppContext& context) const
    {
        CommandRequest request;
        if (!Resolve(input, &request))
        {
            return CommandResult::NotFound(input);
        }
        return ExecuteById(request.id, request.arguments, context);
    }

    CommandResult CommandRegistry::ExecuteById(const std::string& id, const std::string& arguments, AppContext& context) const
    {
        const CommandDefinition* definition = Find(id);
        if (!definition)
        {
            return CommandResult::NotFound(id);
        }

        CommandRequest request;
        request.raw = arguments.empty() ? id : id + " " + arguments;
        request.id = id;
        request.token = id;
        request.arguments = arguments;

        if (definition->execute)
        {
            return definition->execute(context, request);
        }

        return CommandResult::Handled(id);
    }

    const CommandDefinition* CommandRegistry::Find(const std::string& id) const
    {
        const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry& entry) {
            return entry.definition.id == id;
        });
        return it == m_entries.end() ? nullptr : &it->definition;
    }

    std::vector<CommandDefinition> CommandRegistry::List() const
    {
        std::vector<CommandDefinition> result;
        result.reserve(m_entries.size());
        for (const Entry& entry : m_entries)
        {
            result.push_back(entry.definition);
        }
        return result;
    }

    CommandRegistry::Entry* CommandRegistry::FindMutable(const std::string& id)
    {
        const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry& entry) {
            return entry.definition.id == id;
        });
        return it == m_entries.end() ? nullptr : &(*it);
    }

    std::string CommandRegistry::NormalizeToken(const std::string& value)
    {
        std::string result = Trim(value);
        if (!result.empty() && (result.front() == ':' || result.front() == '/'))
        {
            result.erase(result.begin());
        }
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return result;
    }

    std::string CommandRegistry::Trim(const std::string& value)
    {
        const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
            return std::isspace(c) != 0;
        });
        const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
            return std::isspace(c) != 0;
        }).base();
        if (begin >= end)
        {
            return {};
        }
        return std::string(begin, end);
    }
}
