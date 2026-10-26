#pragma once

#include "App/Slate/Input/ShortcutService.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Software::Slate
{
    // One extra action shown on the home screen
    struct HomeEntry
    {
        // Stores the plugin or module that owns the entry
        std::string owner;
        // Stores a stable id unique inside the owner
        std::string id;
        // Stores the home section, for example write or tools
        std::string section;
        // Stores the visible label
        std::string label;
        // Stores an optional single key binding shown in parentheses
        std::string key;
        // Stores the command line executed when the entry is used
        std::string command;
    };

    // One text segment shown on the right side of the status line
    struct StatusItem
    {
        // Stores the plugin or module that owns the item
        std::string owner;
        // Stores a stable id unique inside the owner
        std::string id;
        // Stores the visible text
        std::string text;
    };

    // One slash command contributed by a plugin or module
    struct SlashExtension
    {
        // Stores the plugin or module that owns the command
        std::string owner;
        // Stores the token typed after the slash
        std::string id;
        // Stores the short palette label
        std::string label;
        // Stores the one line description
        std::string description;
        // Builds the replacement text for the slash token; returns false on failure
        std::function<bool(std::string* replacement, std::string* error)> run;
    };

    // One global key chord bound to a command line
    struct CommandKeyBinding
    {
        // Stores the plugin or module that owns the binding
        std::string owner;
        // Stores the parsed chord
        KeyBinding binding;
        // Stores the command line executed when the chord is pressed
        std::string command;
    };

    // Where a contributed view appears
    enum class ViewLocation
    {
        // A full screen of its own, opened with :view <id>
        Page,
        // A panel in the right sidebar of the note screen
        Sidebar
    };

    // A screen or panel drawn by a plugin or module
    struct ViewContribution
    {
        // Stores the plugin or module that owns the view
        std::string owner;
        // Stores the globally unique view id
        std::string id;
        // Stores the visible title
        std::string title;
        // Stores where the view appears
        ViewLocation location = ViewLocation::Page;
        // Draws the view into the current ImGui region of the given size
        std::function<void(float width, float height)> draw;
    };

    // Collects UI contributions from plugins and modules so shell code never depends on a plugin runtime
    class SlateExtensionRegistry
    {
    public:
        // Adds or replaces one home entry
        void SetHomeEntry(HomeEntry entry);
        // Adds or replaces one status item; empty text removes it
        void SetStatusItem(StatusItem item);
        // Adds or replaces one slash command
        void SetSlashCommand(SlashExtension command);
        // Adds or replaces one key binding for the same chord
        void SetKeyBinding(CommandKeyBinding binding);
        // Adds or replaces one view by id
        void SetView(ViewContribution view);
        // Returns views in registration order
        const std::vector<ViewContribution>& Views() const;
        // Finds a view by id
        const ViewContribution* FindView(const std::string& id) const;
        // Removes everything contributed by one owner
        void RemoveOwner(const std::string& owner);

        // Returns home entries in registration order
        const std::vector<HomeEntry>& HomeEntries() const;
        // Returns status items in registration order
        const std::vector<StatusItem>& StatusItems() const;
        // Returns slash commands in registration order
        const std::vector<SlashExtension>& SlashCommands() const;
        // Returns key bindings in registration order
        const std::vector<CommandKeyBinding>& KeyBindings() const;
        // Finds a slash command by token
        const SlashExtension* FindSlashCommand(const std::string& id) const;
        // Changes whenever key bindings change so native hosts can resync cheaply
        std::uint64_t KeyBindingRevision() const;

    private:
        std::vector<HomeEntry> m_homeEntries;
        std::vector<StatusItem> m_statusItems;
        std::vector<SlashExtension> m_slashCommands;
        std::vector<CommandKeyBinding> m_keyBindings;
        std::vector<ViewContribution> m_views;
        std::uint64_t m_keyBindingRevision = 1;
    };
}
