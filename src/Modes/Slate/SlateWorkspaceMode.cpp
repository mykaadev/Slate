#include "Modes/Slate/SlateWorkspaceMode.h"

#include "App/Slate/SlateModeIds.h"
#include "App/Slate/SlateUiState.h"
#include "App/Slate/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    const char* SlateWorkspaceMode::ModeName() const
    {
        return Software::Slate::ModeIds::Workspace;
    }

    void SlateWorkspaceMode::DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput)
    {
        auto& workspace = WorkspaceContext(context);
        auto& ui = UiState(context);

        if (handleInput)
        {
            if (ui.workspaceView == Software::Slate::SlateWorkspaceView::Setup)
            {
                if (IsKeyPressed(ImGuiKey_N))
                {
                    BeginWorkspaceCreate(context);
                }
                else if (IsKeyPressed(ImGuiKey_Q))
                {
                    BeginQuitConfirm();
                }
            }
            else
            {
                const auto& vaults = workspace.WorkspaceRegistry().Vaults();
                ui.workspaceNavigation.SetCount(vaults.size());
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
                {
                    ui.workspaceNavigation.MoveNext();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
                {
                    ui.workspaceNavigation.MovePrevious();
                }
                if ((IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter)) &&
                    ui.workspaceNavigation.HasSelection() && !vaults.empty())
                {
                    OpenVault(vaults[ui.workspaceNavigation.Selected()], context);
                }
                if (IsKeyPressed(ImGuiKey_N))
                {
                    BeginWorkspaceCreate(context);
                }
                if (IsKeyPressed(ImGuiKey_D) && ui.workspaceNavigation.HasSelection() && !vaults.empty())
                {
                    const auto id = vaults[ui.workspaceNavigation.Selected()].id;
                    if (workspace.WorkspaceRegistry().RemoveVault(id))
                    {
                        workspace.WorkspaceRegistry().Save(nullptr);
                        ui.workspaceNavigation.SetCount(workspace.WorkspaceRegistry().Vaults().size());
                        SetStatus("workspace removed from list");
                    }
                }
                if (IsKeyPressed(ImGuiKey_Escape))
                {
                    if (workspace.HasWorkspaceLoaded())
                    {
                        ShowHome(context);
                    }
                    else
                    {
                        ui.workspaceView = Software::Slate::SlateWorkspaceView::Setup;
                    }
                }
            }
        }

        if (ui.workspaceView == Software::Slate::SlateWorkspaceView::Setup)
        {
            DrawWorkspaceSetup();
        }
        else
        {
            DrawWorkspaceSwitcher(context);
        }
    }

    std::string SlateWorkspaceMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        const auto& ui = UiState(const_cast<Software::Core::Runtime::AppContext&>(context));
        return ui.workspaceView == Software::Slate::SlateWorkspaceView::Setup
                   ? "(n) new   (:) command   (q) quit"
                   : "(up/down) choose   (enter) switch   (n) new   (d) remove   (esc) home";
    }

    void SlateWorkspaceMode::BeginWorkspaceCreate(Software::Core::Runtime::AppContext& context)
    {
        BeginPrompt("workspace title", "My Workspace",
                    [this](const std::string& title, Software::Core::Runtime::AppContext& titleContext) {
                        auto& ui = UiState(titleContext);
                        ui.pendingWorkspaceTitle = title.empty() ? "My Workspace" : title;
                        BeginPrompt("workspace emoji", Software::Slate::WorkspaceRegistryService::DefaultEmoji(),
                                    [this](const std::string& emoji, Software::Core::Runtime::AppContext& emojiContext) {
                                        auto& emojiUi = UiState(emojiContext);
                                        emojiUi.pendingWorkspaceEmoji =
                                            emoji.empty() ? Software::Slate::WorkspaceRegistryService::DefaultEmoji() : emoji;
                                        const auto defaultPath =
                                            Software::Slate::WorkspaceRegistryService::DefaultWorkspacePathForTitle(
                                                emojiUi.pendingWorkspaceTitle);
                                        BeginPrompt("workspace location", defaultPath.string(),
                                                    [this](const std::string& pathText,
                                                           Software::Core::Runtime::AppContext& pathContext) {
                                                        auto& workspace = WorkspaceContext(pathContext);
                                                        auto& pathUi = UiState(pathContext);
                                                        const auto defaultPathValue =
                                                            Software::Slate::WorkspaceRegistryService::DefaultWorkspacePathForTitle(
                                                                pathUi.pendingWorkspaceTitle);
                                                        const auto path = pathText.empty() ? defaultPathValue
                                                                                           : Software::Slate::fs::path(pathText);
                                                        Software::Slate::WorkspaceVault vault;
                                                        std::string error;
                                                        if (!workspace.CreateWorkspaceVault(pathUi.pendingWorkspaceTitle,
                                                                                           pathUi.pendingWorkspaceEmoji,
                                                                                           path,
                                                                                           &vault,
                                                                                           &error))
                                                        {
                                                            SetError(error);
                                                            ShowWorkspaceSetup(pathContext);
                                                            return;
                                                        }
                                                        OpenVault(vault, pathContext);
                                                    });
                                    });
                    });
    }

    void SlateWorkspaceMode::DrawWorkspaceSetup()
    {
        ImGui::Dummy(ImVec2(1.0f, ImGui::GetContentRegionAvail().y * 0.18f));
        TextCentered("Slate", Cyan);
        ImGui::Spacing();
        TextCentered("no workspace yet", Amber);
        ImGui::Dummy(ImVec2(1.0f, 28.0f));
        TextCentered("create a local markdown workspace to begin", Primary);
        ImGui::Dummy(ImVec2(1.0f, 24.0f));

        const float columnWidth = 360.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - columnWidth) * 0.5f);
        ImGui::BeginGroup();
        TextLine("n", "new");
        TextLine(":", "command");
        TextLine("q", "quit");
        ImGui::EndGroup();

        const std::string defaultPath =
            "default: " + Software::Slate::WorkspaceRegistryService::DefaultWorkspaceRoot().string();
        ImGui::Dummy(ImVec2(1.0f, 24.0f));
        TextCentered(defaultPath.c_str(), Muted);
    }

    void SlateWorkspaceMode::DrawWorkspaceSwitcher(Software::Core::Runtime::AppContext& context)
    {
        auto& workspace = WorkspaceContext(context);
        auto& ui = UiState(context);
        const auto& vaults = workspace.WorkspaceRegistry().Vaults();
        ui.workspaceNavigation.SetCount(vaults.size());

        ImGui::TextColored(Cyan, "workspaces");
        ImGui::Separator();

        if (vaults.empty())
        {
            ImGui::TextColored(Muted, "no workspaces registered");
            return;
        }

        for (std::size_t i = 0; i < vaults.size(); ++i)
        {
            const bool selected = i == ui.workspaceNavigation.Selected();
            const auto& vault = vaults[i];
            ImGui::TextColored(selected ? Green : Primary, "%s %s %s", selected ? ">" : " ", vault.emoji.c_str(),
                               vault.title.c_str());
            ImGui::SameLine();
            ImGui::TextColored(Muted, "%s", vault.path.string().c_str());
        }
    }
}
