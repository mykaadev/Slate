#include "Modes/Slate/SlateWorkspaceMode.h"

#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>

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
        auto& shortcuts = workspace.Shortcuts();
        auto& ui = UiState(context);

        if (handleInput)
        {
            if (ui.workspaceView == Software::Slate::SlateWorkspaceView::Setup)
            {
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::WorkspaceNew))
                {
                    BeginWorkspaceCreate(context);
                }
                else if (shortcuts.Pressed(Software::Slate::ShortcutAction::Quit))
                {
                    BeginQuitConfirm();
                }
            }
            else
            {
                const auto& vaults = workspace.WorkspaceRegistry().Vaults();
                ui.workspaceNavigation.SetCount(vaults.size());
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateDown, true))
                {
                    ui.workspaceNavigation.MoveNext();
                }
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateUp, true))
                {
                    ui.workspaceNavigation.MovePrevious();
                }
                if ((shortcuts.Pressed(Software::Slate::ShortcutAction::Accept)) &&
                    ui.workspaceNavigation.HasSelection() && !vaults.empty())
                {
                    OpenVault(vaults[ui.workspaceNavigation.Selected()], context);
                }
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::WorkspaceNew))
                {
                    BeginWorkspaceCreate(context);
                }
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::WorkspaceRemove) && ui.workspaceNavigation.HasSelection() && !vaults.empty())
                {
                    const auto id = vaults[ui.workspaceNavigation.Selected()].id;
                    if (workspace.WorkspaceRegistry().RemoveVault(id))
                    {
                        workspace.WorkspaceRegistry().Save(nullptr);
                        ui.workspaceNavigation.SetCount(workspace.WorkspaceRegistry().Vaults().size());
                        SetStatus("workspace removed from list");
                    }
                }
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
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
            DrawWorkspaceSetup(context);
        }
        else
        {
            DrawWorkspaceSwitcher(context);
        }
    }

    std::string SlateWorkspaceMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        auto& mutableContext = const_cast<Software::Core::Runtime::AppContext&>(context);
        const auto& ui = UiState(mutableContext);
        const auto& shortcuts = WorkspaceContext(mutableContext).Shortcuts();
        const std::string choose = "(" + shortcuts.Label(Software::Slate::ShortcutAction::NavigateUp) + "/" +
                                   shortcuts.Label(Software::Slate::ShortcutAction::NavigateDown) + ") choose";
        return ui.workspaceView == Software::Slate::SlateWorkspaceView::Setup
                   ? shortcuts.Helper(Software::Slate::ShortcutAction::WorkspaceNew, "new") + "   " +
                         shortcuts.Helper(Software::Slate::ShortcutAction::CommandPalette, "command") + "   " +
                         shortcuts.Helper(Software::Slate::ShortcutAction::Quit, "quit")
                   : choose + "   " + shortcuts.Helper(Software::Slate::ShortcutAction::Accept, "switch") + "   " +
                         shortcuts.Helper(Software::Slate::ShortcutAction::WorkspaceNew, "new") + "   " +
                         shortcuts.Helper(Software::Slate::ShortcutAction::WorkspaceRemove, "remove") + "   " +
                         shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "home");
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

    void SlateWorkspaceMode::DrawWorkspaceSetup(Software::Core::Runtime::AppContext& context)
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
        const auto& shortcuts = WorkspaceContext(context).Shortcuts();
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::WorkspaceNew).c_str(), "new");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::CommandPalette).c_str(), "command");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::Quit).c_str(), "quit");
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

        const float width = CenteredColumnWidth(760.0f);
        const float height = std::max(1.0f, ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - 68.0f);
        SetCursorCenteredForWidth(width);
        ImGui::BeginChild("WorkspaceSwitcherList", ImVec2(width, height), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ImGui::TextColored(Cyan, "workspaces");
        ImGui::Separator();

        if (vaults.empty())
        {
            ImGui::TextColored(Muted, "no workspaces registered");
            ImGui::EndChild();
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
        ImGui::EndChild();
    }
}
