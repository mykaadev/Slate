#include "App/Slate/Input/ShortcutService.h"

#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Core/SettingsFile.h"
#include "App/Slate/Core/AppPaths.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace Software::Slate
{
    namespace
    {
        KeyBinding Bind(ImGuiKey key, bool ctrl = false, bool shift = false, bool alt = false)
        {
            return KeyBinding{key, ctrl, shift, alt};
        }

        std::string LowerTrim(std::string_view value)
        {
            return PathUtils::ToLower(PathUtils::Trim(value));
        }

        std::vector<KeyBinding> BindingChoices()
        {
            std::vector<KeyBinding> out{
                Bind(ImGuiKey_None),
                Bind(ImGuiKey_Enter), Bind(ImGuiKey_Enter, false, true), Bind(ImGuiKey_Escape), Bind(ImGuiKey_Tab), Bind(ImGuiKey_Slash),
                Bind(ImGuiKey_UpArrow), Bind(ImGuiKey_DownArrow), Bind(ImGuiKey_LeftArrow), Bind(ImGuiKey_RightArrow),
                Bind(ImGuiKey_Delete), Bind(ImGuiKey_Backspace),
                Bind(ImGuiKey_A), Bind(ImGuiKey_C), Bind(ImGuiKey_D), Bind(ImGuiKey_F), Bind(ImGuiKey_J),
                Bind(ImGuiKey_M), Bind(ImGuiKey_N), Bind(ImGuiKey_O), Bind(ImGuiKey_P), Bind(ImGuiKey_Q),
                Bind(ImGuiKey_R), Bind(ImGuiKey_S), Bind(ImGuiKey_T), Bind(ImGuiKey_W), Bind(ImGuiKey_X), Bind(ImGuiKey_Y),
                Bind(ImGuiKey_Slash, false, true), Bind(ImGuiKey_Slash, true),
                Bind(ImGuiKey_A, true), Bind(ImGuiKey_C, true), Bind(ImGuiKey_D, true), Bind(ImGuiKey_F, true),
                Bind(ImGuiKey_J, true), Bind(ImGuiKey_M, true), Bind(ImGuiKey_N, true), Bind(ImGuiKey_O, true),
                Bind(ImGuiKey_P, true), Bind(ImGuiKey_Q, true), Bind(ImGuiKey_R, true), Bind(ImGuiKey_S, true),
                Bind(ImGuiKey_T, true), Bind(ImGuiKey_W, true), Bind(ImGuiKey_X, true), Bind(ImGuiKey_Y, true),
            };
            return out;
        }

        bool SameBinding(const KeyBinding& a, const KeyBinding& b)
        {
            return a.key == b.key && a.ctrl == b.ctrl && a.shift == b.shift && a.alt == b.alt;
        }

        const char* KeyName(ImGuiKey key)
        {
            switch (key)
            {
            case ImGuiKey_None: return "none";
            case ImGuiKey_Enter: return "enter";
            case ImGuiKey_KeypadEnter: return "enter";
            case ImGuiKey_Escape: return "esc";
            case ImGuiKey_Tab: return "tab";
            case ImGuiKey_Slash: return "/";
            case ImGuiKey_UpArrow: return "up";
            case ImGuiKey_DownArrow: return "down";
            case ImGuiKey_LeftArrow: return "left";
            case ImGuiKey_RightArrow: return "right";
            case ImGuiKey_Delete: return "del";
            case ImGuiKey_Backspace: return "backspace";
            case ImGuiKey_A: return "a";
            case ImGuiKey_C: return "c";
            case ImGuiKey_D: return "d";
            case ImGuiKey_F: return "f";
            case ImGuiKey_J: return "j";
            case ImGuiKey_M: return "m";
            case ImGuiKey_N: return "n";
            case ImGuiKey_O: return "o";
            case ImGuiKey_P: return "p";
            case ImGuiKey_Q: return "q";
            case ImGuiKey_R: return "r";
            case ImGuiKey_S: return "s";
            case ImGuiKey_T: return "t";
            case ImGuiKey_W: return "w";
            case ImGuiKey_X: return "x";
            case ImGuiKey_Y: return "y";
            default: return "key";
            }
        }

        bool KeyFromName(const std::string& key, ImGuiKey* out)
        {
            static const std::unordered_map<std::string, ImGuiKey> keys{
                {"none", ImGuiKey_None}, {"enter", ImGuiKey_Enter}, {"return", ImGuiKey_Enter},
                {"esc", ImGuiKey_Escape}, {"escape", ImGuiKey_Escape}, {"tab", ImGuiKey_Tab},
                {"/", ImGuiKey_Slash}, {"slash", ImGuiKey_Slash}, {"?", ImGuiKey_Slash},
                {"up", ImGuiKey_UpArrow}, {"down", ImGuiKey_DownArrow}, {"left", ImGuiKey_LeftArrow},
                {"right", ImGuiKey_RightArrow}, {"del", ImGuiKey_Delete}, {"delete", ImGuiKey_Delete},
                {"backspace", ImGuiKey_Backspace},
                {"a", ImGuiKey_A}, {"c", ImGuiKey_C}, {"d", ImGuiKey_D}, {"f", ImGuiKey_F},
                {"j", ImGuiKey_J}, {"m", ImGuiKey_M}, {"n", ImGuiKey_N}, {"o", ImGuiKey_O},
                {"p", ImGuiKey_P}, {"q", ImGuiKey_Q}, {"r", ImGuiKey_R}, {"s", ImGuiKey_S},
                {"t", ImGuiKey_T}, {"w", ImGuiKey_W}, {"x", ImGuiKey_X}, {"y", ImGuiKey_Y},
            };
            const auto it = keys.find(key);
            if (it == keys.end())
            {
                return false;
            }
            if (out)
            {
                *out = it->second;
            }
            return true;
        }

        std::vector<ImGuiKey> CaptureKeys()
        {
            return {
                ImGuiKey_Enter, ImGuiKey_KeypadEnter, ImGuiKey_Tab, ImGuiKey_Slash,
                ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow,
                ImGuiKey_Delete, ImGuiKey_Backspace,
                ImGuiKey_A, ImGuiKey_C, ImGuiKey_D, ImGuiKey_F, ImGuiKey_J, ImGuiKey_M,
                ImGuiKey_N, ImGuiKey_O, ImGuiKey_P, ImGuiKey_Q, ImGuiKey_R, ImGuiKey_S,
                ImGuiKey_T, ImGuiKey_W, ImGuiKey_X, ImGuiKey_Y,
            };
        }

        bool ModifiersMatch(const KeyBinding& binding)
        {
            const ImGuiIO& io = ImGui::GetIO();
            return io.KeyCtrl == binding.ctrl && io.KeyShift == binding.shift && io.KeyAlt == binding.alt;
        }
    }

    void ShortcutService::Initialize()
    {
        if (m_initialized)
        {
            return;
        }
        RegisterDefaults();
        m_settingsPath = AppPaths::ConfigFile("input", "shortcuts.tsv");
        Load();
        m_initialized = true;
    }

    void ShortcutService::RegisterDefaults()
    {
        m_definitions.clear();
        m_bindings.assign(static_cast<std::size_t>(ShortcutAction::Count), KeyBinding{});
        auto add = [&](ShortcutAction action, const char* id, const char* group, const char* label, KeyBinding binding) {
            m_definitions.push_back(ShortcutDefinition{action, id, group, label, binding});
            BindingMutable(action) = binding;
        };

        add(ShortcutAction::NavigateUp, "nav.up", "navigation", "Move up", Bind(ImGuiKey_UpArrow));
        add(ShortcutAction::NavigateDown, "nav.down", "navigation", "Move down", Bind(ImGuiKey_DownArrow));
        add(ShortcutAction::NavigateLeft, "nav.left", "navigation", "Parent / collapse", Bind(ImGuiKey_LeftArrow));
        add(ShortcutAction::NavigateRight, "nav.right", "navigation", "Child / expand", Bind(ImGuiKey_RightArrow));
        add(ShortcutAction::Accept, "nav.accept", "navigation", "Accept / open", Bind(ImGuiKey_Enter));
        add(ShortcutAction::Cancel, "nav.cancel", "navigation", "Cancel / back", Bind(ImGuiKey_Escape));
        add(ShortcutAction::ConfirmAccept, "nav.confirm_accept", "navigation", "Confirm yes", Bind(ImGuiKey_Y));
        add(ShortcutAction::ConfirmDecline, "nav.confirm_decline", "navigation", "Confirm no", Bind(ImGuiKey_N));
        add(ShortcutAction::CommandPalette, "shell.command", "shell", "Command prompt", Bind(ImGuiKey_Slash, true));
        add(ShortcutAction::ToggleHelp, "shell.help", "shell", "Help", Bind(ImGuiKey_Slash, false, true));
        add(ShortcutAction::HomeJournal, "home.journal", "home", "Journal", Bind(ImGuiKey_J));
        add(ShortcutAction::HomeNewNote, "home.new", "home", "New note", Bind(ImGuiKey_N));
        add(ShortcutAction::HomeSearch, "home.search", "home", "Search", Bind(ImGuiKey_S));
        add(ShortcutAction::HomeFiles, "home.files", "home", "Files", Bind(ImGuiKey_F));
        add(ShortcutAction::HomeTodos, "home.todos", "home", "Todos", Bind(ImGuiKey_T));
        add(ShortcutAction::HomeSettings, "home.settings", "home", "Config", Bind(ImGuiKey_C));
        add(ShortcutAction::HomeWorkspaces, "home.workspaces", "home", "Workspaces", Bind(ImGuiKey_W));
        add(ShortcutAction::BrowserFilter, "browser.filter", "browser", "Filter", Bind(ImGuiKey_Slash));
        add(ShortcutAction::BrowserNewFolder, "browser.folder", "browser", "New folder", Bind(ImGuiKey_A));
        add(ShortcutAction::BrowserMove, "browser.move", "browser", "Move", Bind(ImGuiKey_M));
        add(ShortcutAction::BrowserDelete, "browser.delete", "browser", "Delete", Bind(ImGuiKey_D));
        add(ShortcutAction::EditorSave, "editor.save", "editor", "Save", Bind(ImGuiKey_S, true));
        add(ShortcutAction::EditorPreview, "editor.preview", "editor", "Preview", Bind(ImGuiKey_P, true));
        add(ShortcutAction::EditorOutline, "editor.outline", "editor", "Outline", Bind(ImGuiKey_O, true));
        add(ShortcutAction::EditorFind, "editor.find", "editor", "Find", Bind(ImGuiKey_F, true));
        add(ShortcutAction::SearchPrevious, "search.previous", "search", "Previous match", Bind(ImGuiKey_Enter, false, true));
        add(ShortcutAction::SearchMode, "search.mode", "search", "Cycle search mode", Bind(ImGuiKey_Tab));
        add(ShortcutAction::TodoState, "todo.state", "todos", "Todo state", Bind(ImGuiKey_Tab));
        add(ShortcutAction::TodoOpen, "todo.open", "todos", "Open todo file", Bind(ImGuiKey_O));
        add(ShortcutAction::TodoDelete, "todo.delete", "todos", "Delete todo", Bind(ImGuiKey_Delete));
        add(ShortcutAction::WorkspaceNew, "workspace.new", "workspaces", "New workspace", Bind(ImGuiKey_N));
        add(ShortcutAction::WorkspaceRemove, "workspace.remove", "workspaces", "Remove workspace", Bind(ImGuiKey_D));
        add(ShortcutAction::SettingsReset, "settings.reset", "settings", "Reset settings", Bind(ImGuiKey_R));
        add(ShortcutAction::Quit, "shell.quit", "shell", "Quit", Bind(ImGuiKey_Q));
    }

    const std::vector<ShortcutDefinition>& ShortcutService::Definitions() const { return m_definitions; }

    const KeyBinding& ShortcutService::Binding(ShortcutAction action) const
    {
        const auto index = static_cast<std::size_t>(action);
        static const KeyBinding empty{};
        return index < m_bindings.size() ? m_bindings[index] : empty;
    }

    KeyBinding& ShortcutService::BindingMutable(ShortcutAction action)
    {
        const auto index = static_cast<std::size_t>(action);
        return m_bindings[index];
    }

    std::string ShortcutService::Label(ShortcutAction action) const
    {
        return SerializeBinding(Binding(action));
    }

    std::string ShortcutService::Helper(ShortcutAction action, std::string_view description) const
    {
        return "(" + Label(action) + ") " + std::string(description);
    }

    std::string ShortcutService::HelperLine(std::initializer_list<std::pair<ShortcutAction, std::string_view>> fragments) const
    {
        std::string out;
        bool first = true;
        for (const auto& fragment : fragments)
        {
            if (!first)
            {
                out += "   ";
            }
            out += Helper(fragment.first, fragment.second);
            first = false;
        }
        return out;
    }

    bool ShortcutService::Pressed(ShortcutAction action, bool repeat) const
    {
        const KeyBinding& binding = Binding(action);
        if (binding.key == ImGuiKey_None || !ModifiersMatch(binding))
        {
            return false;
        }
        if (binding.key == ImGuiKey_Enter)
        {
            return ImGui::IsKeyPressed(ImGuiKey_Enter, repeat) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, repeat);
        }
        return ImGui::IsKeyPressed(binding.key, repeat);
    }

    bool ShortcutService::AnyPressed(std::initializer_list<ShortcutAction> actions, bool repeat) const
    {
        for (const auto action : actions)
        {
            if (Pressed(action, repeat))
            {
                return true;
            }
        }
        return false;
    }

    bool ShortcutService::CapturePressedBinding(KeyBinding* out)
    {
        for (const ImGuiKey key : CaptureKeys())
        {
            if (!ImGui::IsKeyPressed(key, false))
            {
                continue;
            }

            const ImGuiIO& io = ImGui::GetIO();
            KeyBinding binding;
            binding.key = key == ImGuiKey_KeypadEnter ? ImGuiKey_Enter : key;
            binding.ctrl = io.KeyCtrl;
            binding.shift = io.KeyShift;
            binding.alt = io.KeyAlt;
            if (out)
            {
                *out = binding;
            }
            return true;
        }
        return false;
    }

    bool ShortcutService::SetBinding(ShortcutAction action, const KeyBinding& binding, std::string* error)
    {
        BindingMutable(action) = binding;
        return Save(error);
    }

    bool ShortcutService::CycleBinding(ShortcutAction action, int delta, std::string* error)
    {
        const auto choices = BindingChoices();
        const KeyBinding current = Binding(action);
        auto it = std::find_if(choices.begin(), choices.end(), [&](const KeyBinding& candidate) {
            return SameBinding(candidate, current);
        });
        std::size_t index = it == choices.end() ? 0 : static_cast<std::size_t>(it - choices.begin());
        const int next = static_cast<int>(index) + delta;
        index = static_cast<std::size_t>((next % static_cast<int>(choices.size()) + static_cast<int>(choices.size())) %
                                         static_cast<int>(choices.size()));
        return SetBinding(action, choices[index], error);
    }

    bool ShortcutService::Reset(std::string* error)
    {
        RegisterDefaults();
        return Save(error);
    }

    fs::path ShortcutService::SettingsPath() const { return m_settingsPath; }
    const char* ShortcutService::DefaultsSourcePath() { return "src/App/Slate/Input/ShortcutService.cpp"; }

    const char* ShortcutService::Id(ShortcutAction action)
    {
        switch (action)
        {
        case ShortcutAction::NavigateUp: return "nav.up";
        case ShortcutAction::NavigateDown: return "nav.down";
        case ShortcutAction::NavigateLeft: return "nav.left";
        case ShortcutAction::NavigateRight: return "nav.right";
        case ShortcutAction::Accept: return "nav.accept";
        case ShortcutAction::Cancel: return "nav.cancel";
        case ShortcutAction::ConfirmAccept: return "nav.confirm_accept";
        case ShortcutAction::ConfirmDecline: return "nav.confirm_decline";
        case ShortcutAction::CommandPalette: return "shell.command";
        case ShortcutAction::ToggleHelp: return "shell.help";
        case ShortcutAction::HomeJournal: return "home.journal";
        case ShortcutAction::HomeNewNote: return "home.new";
        case ShortcutAction::HomeSearch: return "home.search";
        case ShortcutAction::HomeFiles: return "home.files";
        case ShortcutAction::HomeTodos: return "home.todos";
        case ShortcutAction::HomeSettings: return "home.settings";
        case ShortcutAction::HomeWorkspaces: return "home.workspaces";
        case ShortcutAction::BrowserFilter: return "browser.filter";
        case ShortcutAction::BrowserNewFolder: return "browser.folder";
        case ShortcutAction::BrowserMove: return "browser.move";
        case ShortcutAction::BrowserDelete: return "browser.delete";
        case ShortcutAction::EditorSave: return "editor.save";
        case ShortcutAction::EditorPreview: return "editor.preview";
        case ShortcutAction::EditorOutline: return "editor.outline";
        case ShortcutAction::EditorFind: return "editor.find";
        case ShortcutAction::SearchPrevious: return "search.previous";
        case ShortcutAction::SearchMode: return "search.mode";
        case ShortcutAction::TodoState: return "todo.state";
        case ShortcutAction::TodoOpen: return "todo.open";
        case ShortcutAction::TodoDelete: return "todo.delete";
        case ShortcutAction::WorkspaceNew: return "workspace.new";
        case ShortcutAction::WorkspaceRemove: return "workspace.remove";
        case ShortcutAction::SettingsReset: return "settings.reset";
        case ShortcutAction::Quit: return "shell.quit";
        case ShortcutAction::Count: return "";
        }
        return "";
    }

    bool ShortcutService::ParseBinding(std::string_view text, KeyBinding* out)
    {
        KeyBinding binding;
        std::stringstream ss{std::string(LowerTrim(text))};
        std::string token;
        std::string keyToken;
        while (std::getline(ss, token, '+'))
        {
            token = LowerTrim(token);
            if (token == "ctrl" || token == "control" || token == "^") binding.ctrl = true;
            else if (token == "shift") binding.shift = true;
            else if (token == "alt") binding.alt = true;
            else keyToken = token;
        }
        if (!KeyFromName(keyToken, &binding.key))
        {
            return false;
        }
        if (out)
        {
            *out = binding;
        }
        return true;
    }

    std::string ShortcutService::SerializeBinding(const KeyBinding& binding)
    {
        if (binding.key == ImGuiKey_None)
        {
            return "none";
        }
        std::string out;
        if (binding.ctrl) out += "ctrl+";
        if (binding.shift) out += "shift+";
        if (binding.alt) out += "alt+";
        out += KeyName(binding.key);
        return out;
    }

    bool ShortcutService::Load(std::string* error)
    {
        std::ifstream file(m_settingsPath, std::ios::binary);
        if (!file)
        {
            file.open(AppPaths::LegacyConfigFile("shortcuts.tsv"), std::ios::binary);
        }
        if (!file)
        {
            return true;
        }
        std::string line;
        while (std::getline(file, line))
        {
            const auto parts = SettingsFile::SplitTabs(line);
            if (parts.size() < 2 || parts[0] != "bind")
            {
                continue;
            }
            const std::string id = SettingsFile::Unescape(parts[1]);
            const std::string value = parts.size() >= 3 ? SettingsFile::Unescape(parts[2]) : "";
            KeyBinding binding;
            if (!ParseBinding(value, &binding))
            {
                continue;
            }
            for (const auto& definition : m_definitions)
            {
                if (id == definition.id)
                {
                    BindingMutable(definition.action) = binding;
                    break;
                }
            }
        }
        if (file.bad())
        {
            if (error) *error = "Could not read shortcuts.";
            return false;
        }
        return true;
    }

    bool ShortcutService::Save(std::string* error) const
    {
        std::error_code ec;
        fs::create_directories(m_settingsPath.parent_path(), ec);
        if (ec)
        {
            if (error) *error = ec.message();
            return false;
        }
        std::ofstream file(m_settingsPath, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            if (error) *error = "Could not write shortcuts.";
            return false;
        }
        for (const auto& definition : m_definitions)
        {
            file << "bind\t" << SettingsFile::Escape(definition.id) << '\t'
                 << SettingsFile::Escape(SerializeBinding(Binding(definition.action))) << '\n';
        }
        return static_cast<bool>(file);
    }
}
