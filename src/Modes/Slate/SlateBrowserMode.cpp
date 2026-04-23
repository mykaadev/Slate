#include "Modes/Slate/SlateBrowserMode.h"

#include "App/Slate/PathUtils.h"
#include "App/Slate/SlateEditorContext.h"
#include "App/Slate/SlateModeIds.h"
#include "App/Slate/SlateUiState.h"
#include "App/Slate/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>
#include <utility>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        bool PathEqualsOrDescendantOf(const Software::Slate::fs::path& candidate,
                                      const Software::Slate::fs::path& root)
        {
            const auto normalizedCandidate = Software::Slate::PathUtils::NormalizeRelative(candidate);
            const auto normalizedRoot = Software::Slate::PathUtils::NormalizeRelative(root);
            return normalizedCandidate == normalizedRoot ||
                   Software::Slate::PathIsDescendantOf(normalizedCandidate, normalizedRoot);
        }

        Software::Slate::fs::path RebaseRelativePath(const Software::Slate::fs::path& candidate,
                                                     const Software::Slate::fs::path& oldRoot,
                                                     const Software::Slate::fs::path& newRoot)
        {
            const auto normalizedCandidate = Software::Slate::PathUtils::NormalizeRelative(candidate);
            const auto normalizedOldRoot = Software::Slate::PathUtils::NormalizeRelative(oldRoot);
            Software::Slate::fs::path tail;

            auto candidateIt = normalizedCandidate.begin();
            auto oldIt = normalizedOldRoot.begin();
            for (; oldIt != normalizedOldRoot.end() && candidateIt != normalizedCandidate.end(); ++oldIt, ++candidateIt)
            {
            }
            for (; candidateIt != normalizedCandidate.end(); ++candidateIt)
            {
                tail /= *candidateIt;
            }
            return Software::Slate::PathUtils::NormalizeRelative(
                Software::Slate::PathUtils::NormalizeRelative(newRoot) / tail);
        }

        Software::Slate::fs::path FinalMoveTargetPath(const Software::Slate::fs::path& source,
                                                      const Software::Slate::fs::path& requestedTarget)
        {
            auto target = Software::Slate::PathUtils::NormalizeRelative(requestedTarget);
            if (Software::Slate::PathUtils::IsMarkdownFile(source) && !Software::Slate::PathUtils::IsMarkdownFile(target))
            {
                target += ".md";
            }
            return Software::Slate::PathUtils::NormalizeRelative(target);
        }
    }

    const char* SlateBrowserMode::ModeName() const
    {
        return Software::Slate::ModeIds::Browser;
    }

    void SlateBrowserMode::DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput)
    {
        if (!WorkspaceContext(context).HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        auto& ui = UiState(context);
        switch (ui.browserView)
        {
        case Software::Slate::SlateBrowserView::QuickOpen:
        case Software::Slate::SlateBrowserView::Recent:
            PrepareList(context);
            if (handleInput)
            {
                HandleListKeys(context);
            }
            DrawPathList(context, ui.browserView == Software::Slate::SlateBrowserView::QuickOpen ? "quick open" : "recent files",
                         "no matching files");
            break;
        case Software::Slate::SlateBrowserView::Library:
            PrepareTreeRows(context, false);
            if (handleInput)
            {
                HandleLibraryKeys(context);
            }
            DrawFileTree(context, false);
            break;
        case Software::Slate::SlateBrowserView::FileTree:
            PrepareTreeRows(context, ui.folderPickerActive);
            if (handleInput)
            {
                HandleTreeKeys(context, ui.folderPickerActive);
            }
            DrawFileTree(context, ui.folderPickerActive);
            break;
        }
    }

    std::string SlateBrowserMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        const auto& ui = UiState(const_cast<Software::Core::Runtime::AppContext&>(context));
        switch (ui.browserView)
        {
        case Software::Slate::SlateBrowserView::QuickOpen:
        case Software::Slate::SlateBrowserView::Recent:
            return "(up/down) move   (enter) open   (/) search   (n) new   (esc) home";
        case Software::Slate::SlateBrowserView::Library:
            return "(up/down) move   (left/right) fold   (enter) open   (/) filter   (esc) home";
        case Software::Slate::SlateBrowserView::FileTree:
            if (ui.folderPickerActive && ui.folderPickerAction == Software::Slate::FolderPickerAction::MoveDestination)
            {
                return "(up/down) choose   (left/right) fold   (enter) select   (a) folder   (/) filter   (esc) cancel";
            }
            return ui.folderPickerActive
                       ? "(up/down) choose   (left/right) fold   (enter) select   (a) folder   (/) filter   (esc) cancel"
                       : "(up/down) move   (left/right) fold   (enter) open   (n) note   (a) folder   (m) move   (d) delete   (/) filter   (esc) home";
        }
        return {};
    }

    void SlateBrowserMode::DrawFooterControls(Software::Core::Runtime::AppContext& context)
    {
        auto& ui = UiState(context);
        if (ui.filterActive && (ui.browserView == Software::Slate::SlateBrowserView::FileTree ||
                                ui.browserView == Software::Slate::SlateBrowserView::Library) &&
            !SearchOpen() && !PromptOpen() && !ConfirmOpen())
        {
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 82.0f);
            const float rowWidth = std::min(620.0f, ImGui::GetContentRegionAvail().x);
            SetCursorCenteredForWidth(rowWidth);
            ImGui::BeginGroup();
            ImGui::TextColored(Amber, "(/)");
            ImGui::SameLine();
            ImGui::TextColored(Primary, "filter");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(std::max(220.0f, rowWidth - 92.0f));
            if (ui.focusFilter)
            {
                ImGui::SetKeyboardFocusHere();
                ui.focusFilter = false;
            }
            InputTextString("##TreeFilter", ui.filterText, 0);
            ImGui::EndGroup();
        }
    }

    void SlateBrowserMode::PrepareList(Software::Core::Runtime::AppContext& context)
    {
        auto& workspace = WorkspaceContext(context);
        auto& ui = UiState(context);
        ui.visiblePaths.clear();

        const auto& source = ui.browserView == Software::Slate::SlateBrowserView::Recent
                                 ? workspace.Workspace().RecentFiles()
                                 : workspace.Workspace().MarkdownFiles();

        for (const auto& path : source)
        {
            if (ui.filterText.empty() || ContainsFilter(path.generic_string(), ui.filterText.c_str()))
            {
                ui.visiblePaths.push_back(path);
            }
        }

        ui.navigation.SetCount(ui.visiblePaths.size());
    }

    void SlateBrowserMode::PrepareTreeRows(Software::Core::Runtime::AppContext& context, bool folderPicker)
    {
        auto& workspace = WorkspaceContext(context);
        auto& ui = UiState(context);
        ui.treeRows.clear();

        if (folderPicker)
        {
            Software::Slate::TreeViewRow root;
            root.relativePath = ".";
            root.isDirectory = true;
            root.depth = 0;
            root.selectable = true;
            root.matchedFilter = false;
            ui.treeRows.push_back(root);

            auto rows = Software::Slate::BuildTreeRows(workspace.Workspace().Entries(), ui.collapsedFolders, ui.filterText,
                                                       Software::Slate::TreeViewMode::Folders);
            if (ui.folderPickerAction == Software::Slate::FolderPickerAction::MoveDestination &&
                Software::Slate::fs::is_directory(workspace.Workspace().Resolve(ui.pendingPath)))
            {
                rows.erase(std::remove_if(rows.begin(), rows.end(),
                                          [&](const Software::Slate::TreeViewRow& row) {
                                              return PathEqualsOrDescendantOf(row.relativePath, ui.pendingPath);
                                          }),
                           rows.end());
            }
            for (auto& row : rows)
            {
                row.depth += 1;
                row.selectable = true;
                ui.treeRows.push_back(std::move(row));
            }
        }
        else
        {
            const auto mode = ui.browserView == Software::Slate::SlateBrowserView::Library
                                  ? Software::Slate::TreeViewMode::Library
                                  : Software::Slate::TreeViewMode::Files;
            ui.treeRows =
                Software::Slate::BuildTreeRows(workspace.Workspace().Entries(), ui.collapsedFolders, ui.filterText, mode);
        }

        ui.navigation.SetCount(ui.treeRows.size());
    }

    void SlateBrowserMode::OpenSelectedPath(Software::Core::Runtime::AppContext& context)
    {
        auto& ui = UiState(context);
        if (!ui.navigation.HasSelection() || ui.visiblePaths.empty())
        {
            return;
        }
        OpenDocument(ui.visiblePaths[ui.navigation.Selected()], context);
    }

    void SlateBrowserMode::ActivateSelectedTreeRow(Software::Core::Runtime::AppContext& context)
    {
        auto& workspace = WorkspaceContext(context);
        auto& ui = UiState(context);
        if (!ui.navigation.HasSelection() || ui.treeRows.empty())
        {
            return;
        }

        const auto& row = ui.treeRows[ui.navigation.Selected()];
        if (ui.folderPickerActive)
        {
            if (row.isDirectory)
            {
                ui.pendingFolderPath = row.relativePath;
                if (ui.folderPickerAction == Software::Slate::FolderPickerAction::MoveDestination)
                {
                    BeginPrompt("move as", ui.pendingPath.filename().string(),
                                [this](const std::string& value, Software::Core::Runtime::AppContext& callbackContext) {
                                    auto& callbackWorkspace = WorkspaceContext(callbackContext);
                                    auto& callbackUi = UiState(callbackContext);
                                    const auto source = Software::Slate::PathUtils::NormalizeRelative(callbackUi.pendingPath);
                                    const std::string finalName = value.empty() ? source.filename().string() : value;
                                    if (finalName.empty())
                                    {
                                        SetError("target name required");
                                        return;
                                    }

                                    Software::Slate::fs::path folder =
                                        Software::Slate::PathUtils::NormalizeRelative(callbackUi.pendingFolderPath);
                                    if (folder == ".")
                                    {
                                        folder.clear();
                                    }

                                    callbackUi.pendingTargetPath = FinalMoveTargetPath(source, folder / finalName);
                                    callbackUi.pendingRewritePlan =
                                        callbackWorkspace.BuildRenamePlan(source, callbackUi.pendingTargetPath);
                                    callbackUi.folderPickerActive = false;
                                    callbackUi.folderPickerAction = Software::Slate::FolderPickerAction::None;
                                    if (callbackUi.pendingRewritePlan.TotalReplacements() > 0)
                                    {
                                        BeginConfirm("update " +
                                                         std::to_string(callbackUi.pendingRewritePlan.TotalReplacements()) +
                                                         " links too?",
                                                     false,
                                                     [this](bool accepted,
                                                            Software::Core::Runtime::AppContext& confirmContext) {
                                                         if (!accepted)
                                                         {
                                                             return;
                                                         }

                                                         std::string error;
                                                         if (!WorkspaceContext(confirmContext)
                                                                  .ApplyRewritePlan(UiState(confirmContext).pendingRewritePlan,
                                                                                    &error))
                                                         {
                                                             SetError(error);
                                                             return;
                                                         }
                                                         ApplyPendingRenameMove(confirmContext);
                                                     },
                                                     "apply");
                                    }
                                    else
                                    {
                                        ApplyPendingRenameMove(callbackContext);
                                    }
                                });
                }
                else
                {
                    BeginPrompt("new note name", "Untitled.md",
                                [this](const std::string& value, Software::Core::Runtime::AppContext& callbackContext) {
                                    auto& callbackWorkspace = WorkspaceContext(callbackContext);
                                    auto& callbackUi = UiState(callbackContext);
                                    Software::Slate::fs::path created;
                                    std::string error;
                                    if (!callbackWorkspace.CreateNewNote(callbackUi.pendingFolderPath, value, &created, &error))
                                    {
                                        SetError(error);
                                        return;
                                    }
                                    callbackUi.folderPickerActive = false;
                                    callbackUi.folderPickerAction = Software::Slate::FolderPickerAction::None;
                                    OpenDocument(created, callbackContext);
                                });
                }
            }
            return;
        }

        if (row.isDirectory)
        {
            ToggleSelectedFolder(context, ui.collapsedFolders.find(Software::Slate::TreePathKey(row.relativePath)) !=
                                              ui.collapsedFolders.end());
        }
        else
        {
            OpenDocument(row.relativePath, context);
        }
    }

    void SlateBrowserMode::ToggleSelectedFolder(Software::Core::Runtime::AppContext& context, bool expanded)
    {
        auto& ui = UiState(context);
        if (!ui.navigation.HasSelection() || ui.treeRows.empty())
        {
            return;
        }

        const auto& row = ui.treeRows[ui.navigation.Selected()];
        if (!row.isDirectory || row.relativePath == ".")
        {
            return;
        }

        const std::string key = Software::Slate::TreePathKey(row.relativePath);
        if (expanded)
        {
            ui.collapsedFolders.erase(key);
        }
        else
        {
            ui.collapsedFolders.insert(key);
        }
    }

    bool SlateBrowserMode::ApplyPendingRenameMove(Software::Core::Runtime::AppContext& context)
    {
        auto& workspace = WorkspaceContext(context);
        auto& editor = EditorContext(context);
        auto& ui = UiState(context);
        const auto source = Software::Slate::PathUtils::NormalizeRelative(ui.pendingPath);
        const auto target = FinalMoveTargetPath(source, ui.pendingTargetPath);
        ui.pendingTargetPath = target;
        const bool sourceIsDirectory = Software::Slate::fs::is_directory(workspace.Workspace().Resolve(source));

        if (source == target)
        {
            SetError("source and target are the same");
            return false;
        }

        if (sourceIsDirectory && PathEqualsOrDescendantOf(target, source))
        {
            SetError("cannot move a folder into itself");
            return false;
        }

        bool activeWasMoved = false;
        Software::Slate::fs::path movedActivePath;
        if (const auto* active = workspace.Documents().Active())
        {
            activeWasMoved = PathEqualsOrDescendantOf(active->relativePath, source);
            if (activeWasMoved)
            {
                movedActivePath = RebaseRelativePath(active->relativePath, source, target);
                if (!SaveActiveDocument(context))
                {
                    return false;
                }
            }
        }

        std::string error;
        if (!workspace.RenameOrMove(source, target, &error))
        {
            SetError(error);
            return false;
        }

        SetStatus("moved " + source.generic_string() + " -> " + target.generic_string());
        const auto targetParent = target.parent_path();
        ui.collapsedFolders.erase(Software::Slate::TreePathKey(targetParent.empty() ? Software::Slate::fs::path(".") : targetParent));
        if (sourceIsDirectory)
        {
            ui.collapsedFolders.insert(Software::Slate::TreePathKey(target));
        }

        if (activeWasMoved)
        {
            workspace.CloseDocument();
            editor.Clear();
            if (Software::Slate::PathUtils::IsMarkdownFile(movedActivePath) &&
                Software::Slate::fs::exists(workspace.Workspace().Resolve(movedActivePath)))
            {
                if (workspace.OpenDocument(movedActivePath, &error))
                {
                    editor.LoadFromActiveDocument(workspace.Documents());
                }
                else
                {
                    SetError(error);
                }
            }
        }

        ui.browserView = Software::Slate::SlateBrowserView::FileTree;
        ActivateMode(Software::Slate::ModeIds::Browser, context);
        return true;
    }

    void SlateBrowserMode::BeginMovePicker(Software::Core::Runtime::AppContext& context,
                                           const Software::Slate::fs::path& relativePath)
    {
        auto& ui = UiState(context);
        ui.pendingPath = Software::Slate::PathUtils::NormalizeRelative(relativePath);
        const auto parent = ui.pendingPath.parent_path();
        ui.pendingFolderPath = parent.empty() ? Software::Slate::fs::path(".") : parent;
        ui.pendingRewritePlan = {};
        ui.folderPickerActive = true;
        ui.folderPickerAction = Software::Slate::FolderPickerAction::MoveDestination;
        ui.browserView = Software::Slate::SlateBrowserView::FileTree;
        ui.filterText.clear();
        ui.filterActive = false;
        ui.navigation.Reset();
        SetStatus("choose destination for " + ui.pendingPath.generic_string());
    }

    void SlateBrowserMode::BeginDelete(Software::Core::Runtime::AppContext& context,
                                       const Software::Slate::fs::path& relativePath)
    {
        auto& ui = UiState(context);
        ui.pendingPath = relativePath;
        BeginConfirm("delete " + relativePath.generic_string() + "?", true,
                     [this](bool accepted, Software::Core::Runtime::AppContext& callbackContext) {
                         if (!accepted)
                         {
                             return;
                         }

                         auto& workspace = WorkspaceContext(callbackContext);
                         auto& editor = EditorContext(callbackContext);
                         auto& uiState = UiState(callbackContext);
                         const auto* active = workspace.Documents().Active();
                         const bool activeWasDeleted =
                             active && PathEqualsOrDescendantOf(active->relativePath, uiState.pendingPath);
                         std::string error;
                         if (workspace.DeletePath(uiState.pendingPath, &error))
                         {
                             SetStatus("deleted " + uiState.pendingPath.generic_string());
                             if (activeWasDeleted)
                             {
                                 workspace.CloseDocument();
                                 editor.Clear();
                             }
                         }
                         else
                         {
                             SetError(error);
                         }
                         uiState.browserView = Software::Slate::SlateBrowserView::FileTree;
                         ActivateMode(Software::Slate::ModeIds::Browser, callbackContext);
                     });
    }

    void SlateBrowserMode::HandleListKeys(Software::Core::Runtime::AppContext& context)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        {
            UiState(context).navigation.MoveNext();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        {
            UiState(context).navigation.MovePrevious();
        }
        if (IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter))
        {
            OpenSelectedPath(context);
        }
        if (IsKeyPressed(ImGuiKey_Slash))
        {
            BeginSearchOverlay(true, context);
        }
        if (IsKeyPressed(ImGuiKey_Escape))
        {
            ShowHome(context);
        }
        if (IsKeyPressed(ImGuiKey_N))
        {
            BeginNewNoteFlow(context);
        }
    }

    void SlateBrowserMode::HandleTreeKeys(Software::Core::Runtime::AppContext& context, bool folderPicker)
    {
        auto& ui = UiState(context);
        const ImGuiIO& io = ImGui::GetIO();
        if (!io.KeyShift && IsKeyPressed(ImGuiKey_Slash))
        {
            ui.filterActive = true;
            ui.focusFilter = true;
        }

        if (ui.filterActive)
        {
            if (IsKeyPressed(ImGuiKey_Escape))
            {
                ui.filterActive = false;
                return;
            }
        }
        else if (IsKeyPressed(ImGuiKey_Escape))
        {
            const bool wasMovePicker = ui.folderPickerAction == Software::Slate::FolderPickerAction::MoveDestination;
            ui.folderPickerActive = false;
            ui.folderPickerAction = Software::Slate::FolderPickerAction::None;
            if (wasMovePicker)
            {
                ui.browserView = Software::Slate::SlateBrowserView::FileTree;
            }
            else
            {
                ShowHome(context);
            }
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        {
            ui.navigation.MoveNext();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        {
            ui.navigation.MovePrevious();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
        {
            ToggleSelectedFolder(context, true);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
        {
            ToggleSelectedFolder(context, false);
        }
        if (IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter))
        {
            ActivateSelectedTreeRow(context);
        }

        if (ui.filterActive)
        {
            return;
        }

        if (IsKeyPressed(ImGuiKey_A))
        {
            BeginFolderCreateFlow(context);
        }
        if (!folderPicker && IsKeyPressed(ImGuiKey_M) && ui.navigation.HasSelection() && !ui.treeRows.empty())
        {
            BeginMovePicker(context, ui.treeRows[ui.navigation.Selected()].relativePath);
        }
        if (!folderPicker && (IsKeyPressed(ImGuiKey_D) || IsKeyPressed(ImGuiKey_Delete)) && ui.navigation.HasSelection() &&
            !ui.treeRows.empty())
        {
            BeginDelete(context, ui.treeRows[ui.navigation.Selected()].relativePath);
        }
        if (!folderPicker && IsKeyPressed(ImGuiKey_N))
        {
            BeginNewNoteFlow(context);
        }
    }

    void SlateBrowserMode::HandleLibraryKeys(Software::Core::Runtime::AppContext& context)
    {
        auto& ui = UiState(context);
        const ImGuiIO& io = ImGui::GetIO();
        if (!io.KeyShift && IsKeyPressed(ImGuiKey_Slash))
        {
            ui.filterActive = true;
            ui.focusFilter = true;
        }

        if (ui.filterActive)
        {
            if (IsKeyPressed(ImGuiKey_Escape))
            {
                ui.filterActive = false;
                return;
            }
        }
        else if (IsKeyPressed(ImGuiKey_Escape))
        {
            ShowHome(context);
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        {
            ui.navigation.MoveNext();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        {
            ui.navigation.MovePrevious();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
        {
            ToggleSelectedFolder(context, true);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
        {
            ToggleSelectedFolder(context, false);
        }
        if (IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter))
        {
            ActivateSelectedTreeRow(context);
        }
    }

    void SlateBrowserMode::DrawPathList(Software::Core::Runtime::AppContext& context, const char* title, const char* emptyText)
    {
        auto& ui = UiState(context);
        const float width = CenteredColumnWidth(760.0f);
        const float height = std::max(1.0f, ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - 68.0f);
        SetCursorCenteredForWidth(width);
        ImGui::BeginChild("BrowserPathList", ImVec2(width, height), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ImGui::TextColored(Cyan, "%s", title);
        ImGui::Separator();

        if (ui.visiblePaths.empty())
        {
            ImGui::TextColored(Muted, "%s", emptyText);
            ImGui::EndChild();
            return;
        }

        for (std::size_t i = 0; i < ui.visiblePaths.size(); ++i)
        {
            const bool selected = i == ui.navigation.Selected();
            ImGui::TextColored(selected ? Green : Primary, "%s %s", selected ? ">" : " ",
                               ui.visiblePaths[i].generic_string().c_str());
        }
        ImGui::EndChild();
    }

    void SlateBrowserMode::DrawFileTree(Software::Core::Runtime::AppContext& context, bool folderPicker)
    {
        auto& ui = UiState(context);
        const float width = CenteredColumnWidth(820.0f);
        const float height = std::max(1.0f, ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - 68.0f);
        SetCursorCenteredForWidth(width);
        ImGui::BeginChild("BrowserFileTree", ImVec2(width, height), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);

        const bool movePicker = folderPicker && ui.folderPickerAction == Software::Slate::FolderPickerAction::MoveDestination;
        const char* title = ui.browserView == Software::Slate::SlateBrowserView::Library
                                ? "library"
                                : (movePicker ? "move to folder" : (folderPicker ? "choose folder" : "file tree"));
        ImGui::TextColored(Cyan, "%s", title);
        if (!ui.filterText.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored(Amber, " filter: %s", ui.filterText.c_str());
        }
        if (movePicker)
        {
            ImGui::TextColored(Muted, "moving: %s", ui.pendingPath.generic_string().c_str());
        }
        ImGui::Separator();

        if (ui.treeRows.empty())
        {
            ImGui::TextColored(Muted, "no matching entries");
            ImGui::EndChild();
            return;
        }

        for (std::size_t i = 0; i < ui.treeRows.size(); ++i)
        {
            const auto& row = ui.treeRows[i];
            const bool selected = i == ui.navigation.Selected();
            const std::string indent(static_cast<std::size_t>(std::max(0, row.depth)) * 2, ' ');
            const bool collapsed =
                row.isDirectory && ui.collapsedFolders.find(Software::Slate::TreePathKey(row.relativePath)) !=
                                       ui.collapsedFolders.end();
            const char* marker = selected ? ">" : " ";
            const char* type = row.isDirectory ? (collapsed ? "[+]" : "[-]") : "   ";
            const ImVec4 color = selected ? Green : (row.matchedFilter ? Amber : Primary);
            ImGui::TextColored(color, "%s %s%s %s", marker, indent.c_str(), type, DisplayNameForTreeRow(row).c_str());
        }
        ImGui::EndChild();
    }
}
