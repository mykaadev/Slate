#include "Modes/Slate/SlateHomeMode.h"

#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/Extensions/SlateExtensionRegistry.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    const char* SlateHomeMode::ModeName() const
    {
        return Software::Slate::ModeIds::Home;
    }

    void SlateHomeMode::DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput)
    {
        auto& workspace = WorkspaceContext(context);
        if (!workspace.HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        auto& shortcuts = workspace.Shortcuts();
        if (handleInput)
        {
            if (shortcuts.Pressed(Software::Slate::ShortcutAction::HomeJournal))
            {
                OpenTodayJournal(context);
            }
            else if (shortcuts.Pressed(Software::Slate::ShortcutAction::HomeNewNote))
            {
                BeginNewNoteFlow(context);
            }
            else if (shortcuts.Pressed(Software::Slate::ShortcutAction::HomeSearch))
            {
                BeginSearchOverlay(true, context);
            }
            else if (shortcuts.Pressed(Software::Slate::ShortcutAction::HomeFiles))
            {
                ShowBrowser(Software::Slate::SlateBrowserView::FileTree, context);
            }
            else if (shortcuts.Pressed(Software::Slate::ShortcutAction::HomeWorkspaces))
            {
                ShowWorkspaceSwitcher(context);
            }
            else if (shortcuts.Pressed(Software::Slate::ShortcutAction::HomeTodos))
            {
                ShowTodos(context);
            }
            else if (shortcuts.Pressed(Software::Slate::ShortcutAction::HomeSettings))
            {
                ShowSettings(context);
            }
            else if (shortcuts.Pressed(Software::Slate::ShortcutAction::Quit))
            {
                BeginQuitConfirm();
            }
            else if (const auto registry = context.services.Resolve<Software::Slate::SlateExtensionRegistry>())
            {
                // Plugin entries only get keys that no built-in home action already uses
                const ImGuiIO& io = ImGui::GetIO();
                for (const auto& entry : registry->HomeEntries())
                {
                    Software::Slate::KeyBinding binding;
                    if (!entry.key.empty() && Software::Slate::ShortcutService::ParseBinding(entry.key, &binding) &&
                        ImGui::IsKeyPressed(binding.key, false) && io.KeyCtrl == binding.ctrl &&
                        io.KeyShift == binding.shift && io.KeyAlt == binding.alt)
                    {
                        HandleCommand(entry.command, context);
                        break;
                    }
                }
            }
        }

        ImGui::Dummy(ImVec2(1.0f, ImGui::GetContentRegionAvail().y * 0.10f));
        TextCentered("Slate", Cyan);
        ImGui::Spacing();
        if (const auto* vault = workspace.WorkspaceRegistry().ActiveVault())
        {
            const std::string title = vault->emoji + " " + vault->title;
            TextCentered(title.c_str(), Muted);
        }
        else
        {
            TextCentered(workspace.Workspace().Root().filename().string().c_str(), Muted);
        }

        ImGui::Spacing();
        TextCentered("quiet markdown workspace", Primary);

        ImGui::Dummy(ImVec2(1.0f, 24.0f));
        const float columnWidth = 360.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - columnWidth) * 0.5f);
        ImGui::BeginGroup();
        auto section = [](const char* title, const ImVec4& color) {
            ImGui::Dummy(ImVec2(1.0f, 8.0f));
            ImGui::TextColored(color, "%s", title);
        };
        auto action = [&](Software::Slate::ShortcutAction shortcut, const char* label, const ImVec4& color) {
            ImGui::TextColored(color, "(%s)", shortcuts.Label(shortcut).c_str());
            ImGui::SameLine();
            ImGui::TextColored(Primary, "%s", label);
        };
        const ImVec4 systemColor{0.66f, 0.66f, 0.60f, 1.0f};

        // Plugin entries join a built-in section with the same name, or get their own section
        const auto registry = context.services.Resolve<Software::Slate::SlateExtensionRegistry>();
        auto pluginEntries = [&](const std::string& name, const ImVec4& color) {
            if (!registry)
            {
                return;
            }
            for (const auto& entry : registry->HomeEntries())
            {
                if (entry.section == name)
                {
                    ImGui::TextColored(color, "(%s)", entry.key.empty() ? ":" : entry.key.c_str());
                    ImGui::SameLine();
                    ImGui::TextColored(Primary, "%s", entry.label.c_str());
                }
            }
        };

        section("write", Green);
        action(Software::Slate::ShortcutAction::HomeJournal, "journal", Green);
        action(Software::Slate::ShortcutAction::HomeNewNote, "new", Green);
        action(Software::Slate::ShortcutAction::HomeTodos, "todos", Green);
        pluginEntries("write", Green);

        section("find", Cyan);
        action(Software::Slate::ShortcutAction::HomeSearch, "search", Cyan);
        pluginEntries("find", Cyan);

        section("browse", Amber);
        action(Software::Slate::ShortcutAction::HomeFiles, "files", Amber);
        pluginEntries("browse", Amber);

        if (registry)
        {
            std::vector<std::string> sections;
            for (const auto& entry : registry->HomeEntries())
            {
                const bool builtIn = entry.section == "write" || entry.section == "find" ||
                                     entry.section == "browse" || entry.section == "system";
                if (!builtIn && std::find(sections.begin(), sections.end(), entry.section) == sections.end())
                {
                    sections.push_back(entry.section);
                }
            }
            for (const auto& name : sections)
            {
                section(name.c_str(), MarkdownImage);
                pluginEntries(name, MarkdownImage);
            }
        }

        section("system", systemColor);
        action(Software::Slate::ShortcutAction::HomeWorkspaces, "workspaces", systemColor);
        action(Software::Slate::ShortcutAction::HomeSettings, "config", systemColor);
        action(Software::Slate::ShortcutAction::ToggleHelp, "help", systemColor);
        action(Software::Slate::ShortcutAction::Quit, "quit", systemColor);
        pluginEntries("system", systemColor);

        ImGui::EndGroup();

        ImGui::Dummy(ImVec2(1.0f, 24.0f));
        const std::string stat = std::to_string(workspace.Workspace().MarkdownFiles().size()) + " markdown files";
        TextCentered(stat.c_str(), Green);
    }

    std::string SlateHomeMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        const auto& shortcuts = WorkspaceContext(const_cast<Software::Core::Runtime::AppContext&>(context)).Shortcuts();
        return shortcuts.Helper(Software::Slate::ShortcutAction::HomeJournal, "journal") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::HomeNewNote, "new") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::HomeSearch, "search") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::HomeFiles, "files") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::HomeTodos, "todos") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::HomeSettings, "config") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::HomeWorkspaces, "workspaces") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::ToggleHelp, "help");
    }
}
