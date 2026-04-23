#include "Modes/Slate/SlateHomeMode.h"

#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

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

        if (handleInput)
        {
            if (IsKeyPressed(ImGuiKey_J))
            {
                OpenTodayJournal(context);
            }
            else if (IsKeyPressed(ImGuiKey_N))
            {
                BeginNewNoteFlow(context);
            }
            else if (IsKeyPressed(ImGuiKey_S))
            {
                BeginSearchOverlay(true, context);
            }
            else if (IsKeyPressed(ImGuiKey_F))
            {
                ShowBrowser(Software::Slate::SlateBrowserView::FileTree, context);
            }
            else if (IsKeyPressed(ImGuiKey_W))
            {
                ShowWorkspaceSwitcher(context);
            }
            else if (IsKeyPressed(ImGuiKey_T))
            {
                ShowTodos(context);
            }
            else if (IsKeyPressed(ImGuiKey_C))
            {
                ShowSettings(context);
            }
            else if (IsKeyPressed(ImGuiKey_Q))
            {
                BeginQuitConfirm();
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
        auto action = [](const char* key, const char* label, const ImVec4& color) {
            ImGui::TextColored(color, "(%s)", key);
            ImGui::SameLine();
            ImGui::TextColored(Primary, "%s", label);
        };
        const ImVec4 systemColor{0.66f, 0.66f, 0.60f, 1.0f};

        section("write", Green);
        action("j", "journal", Green);
        action("n", "new", Green);
        action("t", "todos", Green);

        section("find", Cyan);
        action("s", "search", Cyan);

        section("browse", Amber);
        action("f", "files", Amber);

        section("system", systemColor);
        action("w", "workspaces", systemColor);
        action("c", "config", systemColor);
        action("?", "help", systemColor);
        action("q", "quit", systemColor);

        ImGui::EndGroup();

        ImGui::Dummy(ImVec2(1.0f, 24.0f));
        const std::string stat = std::to_string(workspace.Workspace().MarkdownFiles().size()) + " markdown files";
        TextCentered(stat.c_str(), Green);
    }

    std::string SlateHomeMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        (void)context;
        return "(j) journal   (n) new   (s) search   (f) files   (t) todos   (c) config   (?) help";
    }
}
