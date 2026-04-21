#include "Modes/Slate/SlateHomeMode.h"

#include "App/Slate/SlateModeIds.h"
#include "App/Slate/SlateWorkspaceContext.h"
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
            else if (IsKeyPressed(ImGuiKey_O))
            {
                ShowBrowser(Software::Slate::SlateBrowserView::QuickOpen, context);
            }
            else if (IsKeyPressed(ImGuiKey_S))
            {
                BeginSearchOverlay(true, context);
            }
            else if (IsKeyPressed(ImGuiKey_R))
            {
                ShowBrowser(Software::Slate::SlateBrowserView::Recent, context);
            }
            else if (IsKeyPressed(ImGuiKey_F))
            {
                ShowBrowser(Software::Slate::SlateBrowserView::FileTree, context);
            }
            else if (IsKeyPressed(ImGuiKey_L))
            {
                ShowBrowser(Software::Slate::SlateBrowserView::Library, context);
            }
            else if (IsKeyPressed(ImGuiKey_W))
            {
                ShowWorkspaceSwitcher(context);
            }
            else if (IsKeyPressed(ImGuiKey_T))
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
        auto section = [](const char* title) {
            ImGui::Dummy(ImVec2(1.0f, 8.0f));
            ImGui::TextColored(Muted, "%s", title);
        };
        section("write");
        TextLine("j", "journal");
        TextLine("n", "new");
        section("browse");
        TextLine("l", "library");
        TextLine("f", "files");
        TextLine("r", "recent");
        section("find");
        TextLine("s", "search");
        TextLine("o", "open");
        section("system");
        TextLine("w", "workspaces");
        TextLine("t", "theme");
        TextLine("?", "help");
        TextLine("q", "quit");
        ImGui::EndGroup();

        ImGui::Dummy(ImVec2(1.0f, 24.0f));
        const std::string stat = std::to_string(workspace.Workspace().MarkdownFiles().size()) + " markdown files";
        TextCentered(stat.c_str(), Green);
    }

    std::string SlateHomeMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        (void)context;
        return "(j) journal   (n) new   (l) library   (o) open   (?) help";
    }
}
