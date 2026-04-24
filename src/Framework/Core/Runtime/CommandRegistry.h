#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Software::Core::Runtime
{
    struct AppContext;

    /** Parsed command line passed to command executors. */
    struct CommandRequest
    {
        /** Raw text as entered by the user. */
        std::string raw;

        /** Resolved command id, for example slate.document.save. */
        std::string id;

        /** User-facing token or alias that matched the command. */
        std::string token;

        /** Remaining argument text after the matched token. */
        std::string arguments;
    };

    /** Result returned after command dispatch. */
    struct CommandResult
    {
        /** True when a registered command matched and handled the request. */
        bool handled = false;

        /** True when the command ran successfully. */
        bool succeeded = false;

        /** Resolved command id when available. */
        std::string commandId;

        /** Optional user-facing status or error message. */
        std::string message;

        static CommandResult NotFound(std::string input);
        static CommandResult Handled(std::string id, std::string message = {});
        static CommandResult Failed(std::string id, std::string message = {});
    };

    /** Command metadata and optional executor. */
    struct CommandDefinition
    {
        /** Stable extension-facing command id. */
        std::string id;

        /** Short display name. */
        std::string label;

        /** Category used by palettes and settings pages. */
        std::string category;

        /** Longer help text. */
        std::string description;

        /** Text aliases accepted by command prompt, without leading ':' or '/'. */
        std::vector<std::string> aliases;

        /** Whether this command accepts arbitrary trailing arguments. */
        bool acceptsArguments = false;

        /** Optional command executor. UI shells may still handle command ids directly. */
        std::function<CommandResult(AppContext&, const CommandRequest&)> execute;
    };

    /** Registry for decoupled app commands, aliases, and extension-visible actions. */
    class CommandRegistry
    {
    public:
        /** Registers or replaces a command definition. */
        void Register(CommandDefinition definition);

        /** Removes all registered commands and aliases. */
        void Clear();

        /** Resolves command line text into a canonical command request. */
        bool Resolve(const std::string& input, CommandRequest* outRequest) const;

        /** Executes a registered command when it has an executor. */
        CommandResult Execute(const std::string& input, AppContext& context) const;

        /** Executes a registered command id directly when it has an executor. */
        CommandResult ExecuteById(const std::string& id, const std::string& arguments, AppContext& context) const;

        /** Returns a command definition if registered. */
        const CommandDefinition* Find(const std::string& id) const;

        /** Returns all command definitions in registration order. */
        std::vector<CommandDefinition> List() const;

    private:
        struct Entry
        {
            CommandDefinition definition;
        };

        static std::string NormalizeToken(const std::string& value);
        static std::string Trim(const std::string& value);

        Entry* FindMutable(const std::string& id);

        std::vector<Entry> m_entries;
        std::unordered_map<std::string, std::string> m_aliasToId;
    };
}
