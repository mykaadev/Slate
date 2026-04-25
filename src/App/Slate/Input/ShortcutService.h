#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include "imgui.h"

#include <initializer_list>
#include <string>
#include <utility>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    /** Stable action identifiers used by the configurable shortcut system. */
    enum class ShortcutAction
    {
        NavigateUp,
        NavigateDown,
        NavigateLeft,
        NavigateRight,
        Accept,
        Cancel,
        ConfirmAccept,
        ConfirmDecline,
        CommandPalette,
        ToggleHelp,
        HomeJournal,
        HomeNewNote,
        HomeSearch,
        HomeFiles,
        HomeTodos,
        HomeSettings,
        HomeWorkspaces,
        BrowserFilter,
        BrowserNewFolder,
        BrowserMove,
        BrowserDelete,
        EditorSave,
        EditorPreview,
        EditorOutline,
        EditorFind,
        SearchPrevious,
        SearchMode,
        TodoState,
        TodoOpen,
        TodoDelete,
        WorkspaceNew,
        WorkspaceRemove,
        SettingsReset,
        Quit,
        Count
    };

    /** One keyboard chord. Modifiers are stored explicitly so user config stays simple and stable. */
    struct KeyBinding
    {
        ImGuiKey key = ImGuiKey_None;
        bool ctrl = false;
        bool shift = false;
        bool alt = false;
    };

    /** Metadata for one configurable action shown in settings. */
    struct ShortcutDefinition
    {
        ShortcutAction action = ShortcutAction::Count;
        const char* id = "";
        const char* group = "";
        const char* label = "";
        KeyBinding defaultBinding;
    };

    /** Owns Slate keyboard bindings, persistence, display labels, and press matching. */
    class ShortcutService
    {
    public:
        /** Initializes defaults and loads user overrides from disk. */
        void Initialize();
        /** Returns every configurable action in stable display order. */
        const std::vector<ShortcutDefinition>& Definitions() const;
        /** Returns the active binding for an action. */
        const KeyBinding& Binding(ShortcutAction action) const;
        /** Returns the readable key label for an action, for example ctrl+s. */
        std::string Label(ShortcutAction action) const;
        /** Returns a helper fragment like (ctrl+s) save. */
        std::string Helper(ShortcutAction action, std::string_view description) const;
        /** Joins several helper fragments with Slate's spacing convention. */
        std::string HelperLine(std::initializer_list<std::pair<ShortcutAction, std::string_view>> fragments) const;
        /** Tests whether an action was pressed this frame. */
        bool Pressed(ShortcutAction action, bool repeat = false) const;
        /** Tests whether any of the actions were pressed this frame. */
        bool AnyPressed(std::initializer_list<ShortcutAction> actions, bool repeat = false) const;
        /** Replaces a binding and writes the user shortcut file. */
        bool SetBinding(ShortcutAction action, const KeyBinding& binding, std::string* error = nullptr);
        /** Cycles an action through the built-in bindable key list. Kept for tests/debug UI. */
        bool CycleBinding(ShortcutAction action, int delta, std::string* error = nullptr);
        /** Captures the first non-modifier key pressed this frame, including current modifiers. */
        static bool CapturePressedBinding(KeyBinding* out);
        /** Restores every binding to defaults and writes the user shortcut file. */
        bool Reset(std::string* error = nullptr);
        /** Returns where user overrides are saved. */
        fs::path SettingsPath() const;
        /** Returns where default bindings are declared in source. */
        static const char* DefaultsSourcePath();
        /** Returns an action id, useful for config/debug UI. */
        static const char* Id(ShortcutAction action);
        /** Parses a string like ctrl+s or shift+enter into a binding. */
        static bool ParseBinding(std::string_view text, KeyBinding* out);
        /** Serializes a binding into the config file format. */
        static std::string SerializeBinding(const KeyBinding& binding);

    private:
        /** Loads shortcut overrides from disk. */
        bool Load(std::string* error = nullptr);
        /** Saves shortcut overrides to disk. */
        bool Save(std::string* error = nullptr) const;
        /** Builds default definitions and active bindings. */
        void RegisterDefaults();
        /** Returns a mutable binding slot. */
        KeyBinding& BindingMutable(ShortcutAction action);

        std::vector<ShortcutDefinition> m_definitions;
        std::vector<KeyBinding> m_bindings;
        fs::path m_settingsPath;
        bool m_initialized = false;
    };
}
