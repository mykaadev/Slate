#include "Modes/Slate/SlateWorkspaceMode.h"

#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateDesign.h"
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
        using namespace Software::Slate::UI;
        ImGui::Dummy(ImVec2(1.0f, ImGui::GetContentRegionAvail().y * 0.16f));
        BlockLogo(std::clamp(ImGui::GetWindowHeight() / 70.0f, 7.0f, 13.0f), context.frame.elapsedSeconds);
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
        TextCentered("notes, todos and a journal in plain markdown", TextSecondary());
        ImGui::Dummy(ImVec2(1.0f, Tokens::Space2XL));

        // Welcome card: what a workspace is and how to start one
        const float cardWidth = std::min(460.0f, ImGui::GetWindowWidth() - 48.0f);
        const auto& shortcuts = WorkspaceContext(context).Shortcuts();
        const float lineHeight = ImGui::GetTextLineHeight();
        const float rowHeight = std::max(lineHeight, KeycapHeight()) + 8.0f;
        const ImVec2 size(cardWidth, Tokens::SpaceL * 2.0f + lineHeight * 3.0f + rowHeight * 3.0f + Tokens::SpaceM * 2.0f);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - cardWidth) * 0.5f);
        BeginCard("WorkspaceWelcome", size, Cyan);
        CardTitle("start a workspace", Cyan);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(TextFaint(), "a workspace is a folder of markdown files on your disk");
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
        auto action = [&](Software::Slate::ShortcutAction shortcut, const char* label, const ImVec4& accent) {
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const std::string key = shortcuts.Label(shortcut);
            auto* drawList = ImGui::GetWindowDrawList();
            DrawKeycap(drawList, ImVec2(start.x, start.y + (rowHeight - KeycapHeight()) * 0.5f), key, accent);
            drawList->AddText(ImVec2(start.x + KeycapWidth("ctrl+k") + 10.0f, start.y + (rowHeight - lineHeight) * 0.5f),
                              U32(Primary), label);
            ImGui::Dummy(ImVec2(1.0f, rowHeight));
        };
        action(Software::Slate::ShortcutAction::WorkspaceNew, "create a new workspace", Green);
        action(Software::Slate::ShortcutAction::CommandPalette, "run a command", Cyan);
        action(Software::Slate::ShortcutAction::Quit, "quit", Muted);
        EndCard();

        const std::string defaultPath =
            "new workspaces go to " + Software::Slate::WorkspaceRegistryService::DefaultWorkspaceRoot().string();
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceL));
        TextCentered(defaultPath.c_str(), TextFaint());
    }

    void SlateWorkspaceMode::DrawWorkspaceSwitcher(Software::Core::Runtime::AppContext& context)
    {
        using namespace Software::Slate::UI;
        auto& workspace = WorkspaceContext(context);
        auto& ui = UiState(context);
        const auto& vaults = workspace.WorkspaceRegistry().Vaults();
        ui.workspaceNavigation.SetCount(vaults.size());

        const float width = CenteredColumnWidth(760.0f);
        const float height = std::max(1.0f, ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - Tokens::FooterHeight - 2.0f);
        SetCursorCenteredForWidth(width);
        ImGui::BeginChild("WorkspaceSwitcherList", ImVec2(width, height), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);

        const std::string count = std::to_string(vaults.size()) + (vaults.size() == 1 ? " workspace" : " workspaces");
        ScreenHeader("workspaces", nullptr, count.c_str(), Green);

        if (vaults.empty())
        {
            ImGui::TextColored(TextFaint(), "no workspaces registered");
            ImGui::EndChild();
            return;
        }

        const auto* activeVault = workspace.WorkspaceRegistry().ActiveVault();
        auto* drawList = ImGui::GetWindowDrawList();
        const float lineHeight = ImGui::GetTextLineHeight();
        const float rowHeight = lineHeight * 2.0f + 14.0f;
        for (std::size_t i = 0; i < vaults.size(); ++i)
        {
            const bool selected = i == ui.workspaceNavigation.Selected();
            const bool active = activeVault && vaults[i].id == activeVault->id;
            const auto& vault = vaults[i];
            ImVec2 rowMin;
            ImVec2 rowMax;
            ImGui::PushID(static_cast<int>(i));
            if (ListRow("##vault", selected, rowHeight, Green, &rowMin, &rowMax))
            {
                ui.workspaceNavigation.SetSelected(i);
            }
            ImGui::PopID();

            const float tile = lineHeight * 1.7f;
            const ImVec2 tileMin(rowMin.x + 12.0f, rowMin.y + (rowHeight - tile) * 0.5f);
            drawList->AddRectFilled(tileMin, ImVec2(tileMin.x + tile, tileMin.y + tile), U32(Mix(Surface(3), Green, selected ? 0.18f : 0.06f)),
                                    Tokens::RadiusM);
            const float emojiWidth = ImGui::CalcTextSize(vault.emoji.c_str()).x;
            drawList->AddText(ImVec2(tileMin.x + (tile - emojiWidth) * 0.5f, tileMin.y + (tile - lineHeight) * 0.5f), U32(Primary),
                              vault.emoji.c_str());
            const float textX = tileMin.x + tile + 12.0f;
            drawList->AddText(ImVec2(textX, rowMin.y + 6.0f), U32(selected ? Green : Primary), vault.title.c_str());
            drawList->PushClipRect(rowMin, ImVec2(rowMax.x - 90.0f, rowMax.y), true);
            drawList->AddText(ImVec2(textX, rowMin.y + 8.0f + lineHeight), U32(TextFaint()), vault.path.generic_string().c_str());
            drawList->PopClipRect();
            if (active)
            {
                DrawPill(drawList, ImVec2(rowMax.x - PillWidth("open") - 12.0f, rowMin.y + 8.0f), "open", Green);
            }
            ImGui::Dummy(ImVec2(1.0f, 2.0f));
        }
        ImGui::EndChild();
    }
}
