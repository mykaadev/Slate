#include "Modes/Slate/SlateHomeMode.h"

#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/Extensions/SlateExtensionRegistry.h"
#include "App/Slate/Todos/TodoService.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateDesign.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <set>
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

        // The start screen setting applies once, the first time home shows after launch
        static bool startScreenApplied = false;
        if (!startScreenApplied)
        {
            startScreenApplied = true;
            const int startScreen = workspace.CurrentEditorSettings().startScreen;
            const auto& recentFiles = workspace.Workspace().RecentFiles();
            if (startScreen == 1 && !recentFiles.empty())
            {
                OpenDocument(recentFiles.front(), context);
                return;
            }
            if (startScreen == 2)
            {
                OpenTodayJournal(context);
                return;
            }
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

        // Stats refresh every few seconds; counting todos reads files
        if (context.frame.elapsedSeconds - m_statsAt > 3.0 || m_statsAt < 0.0)
        {
            m_statsAt = context.frame.elapsedSeconds;
            m_openTodos = 0;
            if (const auto todos = context.services.Resolve<Software::Slate::TodoService>())
            {
                for (const auto& todo : todos->CollectTodos(workspace))
                {
                    m_openTodos += todo.state != Software::Slate::TodoState::Done ? 1 : 0;
                }
            }

            // Journal streak: consecutive days ending today (or yesterday) with an entry
            std::set<std::string> days;
            for (const auto& file : workspace.Workspace().MarkdownFiles())
            {
                const std::string path = file.generic_u8string();
                if (path.rfind("Journal/", 0) == 0)
                {
                    days.insert(file.stem().u8string());
                }
            }
            m_streak = 0;
            std::time_t day = std::time(nullptr);
            for (int i = 0; i < 3660; ++i, day -= 24 * 60 * 60)
            {
                std::tm local{};
#if defined(_WIN32)
                localtime_s(&local, &day);
#else
                localtime_r(&day, &local);
#endif
                char label[16];
                std::strftime(label, sizeof(label), "%Y-%m-%d", &local);
                if (days.count(label) != 0)
                {
                    ++m_streak;
                }
                else if (i > 0)
                {
                    break;
                }
            }
        }

        const auto& recent = workspace.Workspace().RecentFiles();
        const std::size_t recentCount = std::min<std::size_t>(recent.size(), 9);
        if (handleInput && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyAlt)
        {
            for (std::size_t i = 0; i < recentCount; ++i)
            {
                if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + static_cast<int>(i)), false))
                {
                    OpenDocument(recent[i], context);
                    return;
                }
            }
        }

        const float windowWidth = ImGui::GetWindowWidth();
        const bool twoColumns = windowWidth > 980.0f && recentCount > 0;
        // Actions sit in two columns: writing and finding on the left, plugins and system on the right
        const float actionColumnWidth = 200.0f;
        const float actionColumnGap = 40.0f;
        const float actionsWidth = actionColumnWidth * 2.0f + actionColumnGap;
        const float recentWidth = twoColumns ? std::min(440.0f, windowWidth * 0.32f) : 0.0f;
        const float columnGap = twoColumns ? 64.0f : 0.0f;
        const float blockWidth = actionsWidth + columnGap + recentWidth;
        auto* drawList = ImGui::GetWindowDrawList();
        using namespace Software::Slate::UI;

        ImGui::Dummy(ImVec2(1.0f, ImGui::GetContentRegionAvail().y * 0.08f));

        // Brand: the block-letter logo with its terminal cursor
        BlockLogo(std::clamp(ImGui::GetWindowHeight() / 82.0f, 6.0f, 11.0f), context.frame.elapsedSeconds);
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));

        if (const auto* vault = workspace.WorkspaceRegistry().ActiveVault())
        {
            TextCentered((vault->emoji + " " + vault->title).c_str(), TextSecondary());
        }
        else
        {
            TextCentered(workspace.Workspace().Root().filename().u8string().c_str(), TextSecondary());
        }

        // Stats as pills: notes, open todos, journal streak
        const std::string notes = std::to_string(workspace.Workspace().MarkdownFiles().size()) + " notes";
        const std::string todos = std::to_string(m_openTodos) + " open todos";
        const std::string streak = std::to_string(m_streak) + " day streak";
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceXS));
        const float pillGap = 8.0f;
        const float statsWidth = PillWidth(notes) + PillWidth(todos) + PillWidth(streak) + pillGap * 2.0f;
        ImGui::SetCursorPosX((windowWidth - statsWidth) * 0.5f);
        Pill(notes, Green);
        ImGui::SameLine(0.0f, pillGap);
        Pill(todos, Amber);
        ImGui::SameLine(0.0f, pillGap);
        Pill(streak, m_streak > 0 ? Cyan : Muted);

        ImGui::Dummy(ImVec2(1.0f, Tokens::Space2XL));
        const float blockX = std::max(ImGui::GetStyle().WindowPadding.x, (windowWidth - blockWidth) * 0.5f);
        const float blockY = ImGui::GetCursorPosY();
        ImGui::SetCursorPosX(blockX);
        ImGui::BeginGroup();
        const float keyColumn = KeycapWidth("ctrl") + 10.0f;
        bool firstSection = true;
        auto section = [&](const char* title, const ImVec4& color) {
            ImGui::Dummy(ImVec2(1.0f, firstSection ? 0.0f : Tokens::SpaceM));
            firstSection = false;
            SectionLabel(title, color);
            ImGui::Dummy(ImVec2(1.0f, 2.0f));
        };
        // One action row: keycap in a fixed column so the labels line up
        auto actionRow = [&](const std::string& key, const std::string& label, const ImVec4& color) {
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const float lineHeight = ImGui::GetTextLineHeight();
            const float rowHeight = std::max(lineHeight, KeycapHeight()) + 4.0f;
            DrawKeycap(drawList, ImVec2(start.x, start.y + (rowHeight - KeycapHeight()) * 0.5f), key, color);
            const float labelX = start.x + std::max(keyColumn, KeycapWidth(key) + 10.0f);
            drawList->AddText(ImVec2(labelX, start.y + (rowHeight - lineHeight) * 0.5f), U32(Primary), label.c_str());
            ImGui::Dummy(ImVec2(actionColumnWidth, rowHeight));
        };
        auto action = [&](Software::Slate::ShortcutAction shortcut, const char* label, const ImVec4& color) {
            actionRow(shortcuts.Label(shortcut), label, color);
        };
        const ImVec4 systemColor = Muted;

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
                    actionRow(entry.key.empty() ? ":" : entry.key, entry.label, color);
                }
            }
        };

        section("write", Green);
        action(Software::Slate::ShortcutAction::HomeJournal, "journal", Green);
        action(Software::Slate::ShortcutAction::HomeNewNote, "new note", Green);
        action(Software::Slate::ShortcutAction::HomeTodos, "todos", Green);
        pluginEntries("write", Green);

        section("find", Cyan);
        action(Software::Slate::ShortcutAction::HomeSearch, "search", Cyan);
        action(Software::Slate::ShortcutAction::HomeFiles, "files", Cyan);
        pluginEntries("find", Cyan);
        pluginEntries("browse", Cyan);
        ImGui::EndGroup();
        ImGui::SameLine(0.0f, actionColumnGap);
        ImGui::BeginGroup();
        firstSection = true;

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
        action(Software::Slate::ShortcutAction::CommandPalette, "commands", systemColor);
        action(Software::Slate::ShortcutAction::ToggleHelp, "help", systemColor);
        action(Software::Slate::ShortcutAction::Quit, "quit", systemColor);
        pluginEntries("system", systemColor);
        ImGui::EndGroup();

        // Recent notes, opened with their number key
        if (recentCount > 0)
        {
            if (twoColumns)
            {
                ImGui::SetCursorPos(ImVec2(blockX + actionsWidth + columnGap, blockY));
            }
            else
            {
                ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceL));
                ImGui::SetCursorPosX(blockX);
            }
            ImGui::BeginGroup();
            SectionLabel("recent", Amber);
            ImGui::Dummy(ImVec2(1.0f, 2.0f));
            const float rowWidth = twoColumns ? recentWidth : actionsWidth;
            const float lineHeight = ImGui::GetTextLineHeight();
            const float rowHeight = std::max(lineHeight, KeycapHeight()) + 8.0f;
            for (std::size_t i = 0; i < recentCount; ++i)
            {
                const auto& path = recent[i];
                ImVec2 rowMin;
                ImVec2 rowMax;
                ImGui::PushID(static_cast<int>(i));
                const bool clicked = ListRow("##recent", false, rowHeight, Amber, &rowMin, &rowMax, rowWidth);
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();
                if (hovered)
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }
                const std::string key = std::to_string(i + 1);
                const std::string name = path.stem().u8string();
                const std::string folder = path.parent_path().generic_u8string();
                DrawKeycap(drawList, ImVec2(rowMin.x + 6.0f, rowMin.y + (rowHeight - KeycapHeight()) * 0.5f), key, Amber);
                const float textY = rowMin.y + (rowHeight - lineHeight) * 0.5f;
                const float nameX = rowMin.x + 6.0f + KeycapWidth("9") + 10.0f;
                drawList->AddText(ImVec2(nameX, textY), U32(hovered ? Primary : TextSecondary()), name.c_str());
                if (!folder.empty())
                {
                    const float folderX = nameX + ImGui::CalcTextSize(name.c_str()).x + 12.0f;
                    drawList->PushClipRect(rowMin, ImVec2(rowMax.x - 6.0f, rowMax.y), true);
                    drawList->AddText(ImVec2(folderX, textY), U32(TextFaint()), folder.c_str());
                    drawList->PopClipRect();
                }
                if (clicked)
                {
                    ImGui::EndGroup();
                    OpenDocument(path, context);
                    return;
                }
            }
            ImGui::EndGroup();
        }
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
