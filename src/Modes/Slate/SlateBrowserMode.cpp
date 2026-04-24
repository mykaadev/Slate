#include "Modes/Slate/SlateBrowserMode.h"

#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Editor/SlateEditorContext.h"
#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
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
            return "(up/down) move   (left/right) tree nav   (enter) open   (/) filter   (esc) home";
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
        auto hasVisibleChild = [&]()
        {
            if (!ui.navigation.HasSelection() || ui.treeRows.empty())
            {
                return false;
            }
            const std::size_t selectedIndex = ui.navigation.Selected();
            return selectedIndex + 1 < ui.treeRows.size() && ui.treeRows[selectedIndex + 1].depth > ui.treeRows[selectedIndex].depth;
        };
        auto isCollapsedSelection = [&]()
        {
            if (!ui.navigation.HasSelection() || ui.treeRows.empty())
            {
                return false;
            }
            const auto& selectedRow = ui.treeRows[ui.navigation.Selected()];
            if (!selectedRow.isDirectory)
            {
                return false;
            }
            return ui.collapsedFolders.find(Software::Slate::TreePathKey(selectedRow.relativePath)) != ui.collapsedFolders.end();
        };
        auto parentIndex = [&]() -> std::size_t
        {
            if (!ui.navigation.HasSelection() || ui.treeRows.empty())
            {
                return ui.treeRows.size();
            }
            const std::size_t selectedIndex = ui.navigation.Selected();
            const int selectedDepth = ui.treeRows[selectedIndex].depth;
            if (selectedDepth <= 0)
            {
                return ui.treeRows.size();
            }
            for (std::size_t scan = selectedIndex; scan > 0; --scan)
            {
                const std::size_t candidateIndex = scan - 1;
                if (ui.treeRows[candidateIndex].depth < selectedDepth)
                {
                    return candidateIndex;
                }
            }
            return ui.treeRows.size();
        };

        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
        {
            if (isCollapsedSelection())
            {
                ToggleSelectedFolder(context, true);
            }
            else if (hasVisibleChild())
            {
                ui.navigation.SetSelected(ui.navigation.Selected() + 1);
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
        {
            if (!isCollapsedSelection() && hasVisibleChild())
            {
                ToggleSelectedFolder(context, false);
            }
            else
            {
                const std::size_t selectedParentIndex = parentIndex();
                if (selectedParentIndex < ui.treeRows.size())
                {
                    ui.navigation.SetSelected(selectedParentIndex);
                }
            }
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
        auto hasVisibleChild = [&]()
        {
            if (!ui.navigation.HasSelection() || ui.treeRows.empty())
            {
                return false;
            }
            const std::size_t selectedIndex = ui.navigation.Selected();
            return selectedIndex + 1 < ui.treeRows.size() && ui.treeRows[selectedIndex + 1].depth > ui.treeRows[selectedIndex].depth;
        };
        auto isCollapsedSelection = [&]()
        {
            if (!ui.navigation.HasSelection() || ui.treeRows.empty())
            {
                return false;
            }
            const auto& selectedRow = ui.treeRows[ui.navigation.Selected()];
            if (!selectedRow.isDirectory)
            {
                return false;
            }
            return ui.collapsedFolders.find(Software::Slate::TreePathKey(selectedRow.relativePath)) != ui.collapsedFolders.end();
        };
        auto parentIndex = [&]() -> std::size_t
        {
            if (!ui.navigation.HasSelection() || ui.treeRows.empty())
            {
                return ui.treeRows.size();
            }
            const std::size_t selectedIndex = ui.navigation.Selected();
            const int selectedDepth = ui.treeRows[selectedIndex].depth;
            if (selectedDepth <= 0)
            {
                return ui.treeRows.size();
            }
            for (std::size_t scan = selectedIndex; scan > 0; --scan)
            {
                const std::size_t candidateIndex = scan - 1;
                if (ui.treeRows[candidateIndex].depth < selectedDepth)
                {
                    return candidateIndex;
                }
            }
            return ui.treeRows.size();
        };

        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
        {
            if (isCollapsedSelection())
            {
                ToggleSelectedFolder(context, true);
            }
            else if (hasVisibleChild())
            {
                ui.navigation.SetSelected(ui.navigation.Selected() + 1);
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
        {
            if (!isCollapsedSelection() && hasVisibleChild())
            {
                ToggleSelectedFolder(context, false);
            }
            else
            {
                const std::size_t selectedParentIndex = parentIndex();
                if (selectedParentIndex < ui.treeRows.size())
                {
                    ui.navigation.SetSelected(selectedParentIndex);
                }
            }
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

        auto* drawList = ImGui::GetWindowDrawList();
        const float indentStep = 20.0f;
        const float railOffset = 10.0f;
        const float rowPaddingY = 2.0f;
        const float rowCornerRounding = 6.0f;
        const float rowInsetX = 6.0f;
        const float indentHighlightInset = 2.0f;
        const float iconSize = 10.0f;
        const float iconGap = 9.0f;
        const float rowLeadX = 18.0f;
        const ImU32 railColor = ImGui::ColorConvertFloat4ToU32(ImVec4(Muted.x, Muted.y, Muted.z, 0.50f));
        const ImU32 activeBranchColor = ImGui::ColorConvertFloat4ToU32(Green);
        const ImU32 matchBranchColor = ImGui::ColorConvertFloat4ToU32(Amber);
        const ImU32 selectedRowColor = ImGui::ColorConvertFloat4ToU32(ImVec4(Green.x, Green.y, Green.z, 0.18f));
        const ImU32 hoveredRowColor = ImGui::ColorConvertFloat4ToU32(ImVec4(Cyan.x, Cyan.y, Cyan.z, 0.08f));
        const ImU32 matchRowColor = ImGui::ColorConvertFloat4ToU32(ImVec4(Amber.x, Amber.y, Amber.z, 0.08f));
        const std::size_t activeIndex = ui.navigation.Selected() < ui.treeRows.size() ? ui.navigation.Selected()
                                                                                        : ui.treeRows.size();
        const Software::Slate::TreeViewRow* activeRow = activeIndex < ui.treeRows.size() ? &ui.treeRows[activeIndex] : nullptr;
        std::vector<Software::Slate::fs::path> activeLineagePaths;
        std::vector<std::size_t> activeLineageRowIndices;
        if (activeRow != nullptr)
        {
            Software::Slate::fs::path current;
            for (const auto& part : activeRow->relativePath)
            {
                current /= part;
                activeLineagePaths.push_back(current);
                activeLineageRowIndices.push_back(activeIndex);
            }

            for (std::size_t depth = 0; depth < activeLineagePaths.size(); ++depth)
            {
                for (std::size_t rowIndex = 0; rowIndex < ui.treeRows.size(); ++rowIndex)
                {
                    if (ui.treeRows[rowIndex].relativePath == activeLineagePaths[depth])
                    {
                        activeLineageRowIndices[depth] = rowIndex;
                        break;
                    }
                }
            }
        }

        auto subtreeContinuesPastRow = [&](std::size_t rowIndex, int depthLevel)
        {
            for (std::size_t next = rowIndex + 1; next < ui.treeRows.size(); ++next)
            {
                const int nextDepth = ui.treeRows[next].depth;
                if (nextDepth < depthLevel)
                {
                    return false;
                }
                if (nextDepth >= depthLevel)
                {
                    return true;
                }
            }
            return false;
        };

        auto isDescendantOrSelf = [&](const Software::Slate::fs::path& candidate, const Software::Slate::fs::path& ancestor)
        {
            return candidate == ancestor || Software::Slate::PathIsDescendantOf(candidate, ancestor);
        };

        auto isOnActiveLineage = [&](const Software::Slate::TreeViewRow& row)
        {
            return activeRow != nullptr && isDescendantOrSelf(activeRow->relativePath, row.relativePath);
        };

        auto shouldAccentRailAtDepth = [&](std::size_t rowIndex, const Software::Slate::TreeViewRow& row, int depth)
        {
            if (activeRow == nullptr || depth < 0 || static_cast<std::size_t>(depth) >= activeLineagePaths.size())
            {
                return false;
            }

            const Software::Slate::fs::path& ancestorPath = activeLineagePaths[static_cast<std::size_t>(depth)];
            const std::size_t ancestorRowIndex = activeLineageRowIndices[static_cast<std::size_t>(depth)];
            return rowIndex > ancestorRowIndex && rowIndex <= activeIndex && isDescendantOrSelf(row.relativePath, ancestorPath);
        };

        for (std::size_t i = 0; i < ui.treeRows.size(); ++i)
        {
            const auto& row = ui.treeRows[i];
            const bool selected = i == activeIndex;
            const bool onActiveLineage = isOnActiveLineage(row);
            const float lineageAlpha = std::clamp(0.05f + static_cast<float>(std::max(0, row.depth)) * 0.028f, 0.05f, 0.16f);
            const ImVec4 color = selected ? Green
                                          : (onActiveLineage ? ImVec4(Green.x, Green.y, Green.z, 0.90f)
                                                             : (row.matchedFilter ? Amber : Primary));
            const ImVec2 rowStart = ImGui::GetCursorScreenPos();
            const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + 2.0f;
            const float rowMidY = rowStart.y + rowHeight * 0.5f;
            const float rowBottomY = rowStart.y + rowHeight - rowPaddingY;
            const float rowWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
            const float rowLeftX = rowStart.x + rowInsetX;
            const float rowRightX = rowStart.x + rowWidth - rowInsetX;
            const float baseX = rowStart.x + rowLeadX;
            const float iconX = rowStart.x + static_cast<float>(std::max(0, row.depth)) * indentStep + 22.0f;
            const float textX = iconX + iconSize + iconGap;
            const float lineageStartX = std::min(rowRightX - 18.0f, rowLeftX + indentHighlightInset + static_cast<float>(std::max(0, row.depth)) * indentStep);

            ImGui::PushID(static_cast<int>(i));
            ImGui::SetCursorScreenPos(rowStart);
            ImGui::InvisibleButton("##TreeRow", ImVec2(rowWidth, rowHeight));
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                ui.navigation.SetSelected(i);
            }
            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                ui.navigation.SetSelected(i);
                ActivateSelectedTreeRow(context);
            }
            ImGui::PopID();

            const ImU32 rowFillColor = selected ? selectedRowColor
                                                : (hovered ? hoveredRowColor
                                                           : (onActiveLineage
                                                                  ? ImGui::ColorConvertFloat4ToU32(ImVec4(Green.x, Green.y, Green.z, lineageAlpha))
                                                                  : (row.matchedFilter ? matchRowColor : 0U)));
            if (rowFillColor != 0U)
            {
                const float fillLeftX = (selected || onActiveLineage) ? lineageStartX : rowLeftX;
                drawList->AddRectFilled(ImVec2(fillLeftX, rowStart.y + 1.0f),
                                        ImVec2(rowRightX, rowStart.y + rowHeight - 1.0f),
                                        rowFillColor,
                                        rowCornerRounding);
            }

            if (selected || onActiveLineage)
            {
                const ImU32 markerColor = selected ? activeBranchColor : ImGui::ColorConvertFloat4ToU32(ImVec4(Green.x, Green.y, Green.z, std::min(0.85f, lineageAlpha + 0.25f)));
                drawList->AddLine(ImVec2(lineageStartX + 1.0f, rowStart.y + 4.0f),
                                  ImVec2(lineageStartX + 1.0f, rowStart.y + rowHeight - 4.0f),
                                  markerColor,
                                  selected ? 2.4f : 1.6f);
            }

            for (int depth = 0; depth < row.depth; ++depth)
            {
                if (!subtreeContinuesPastRow(i, depth + 1))
                {
                    continue;
                }

                const float x = baseX + static_cast<float>(depth) * indentStep + railOffset;
                const bool accentRail = shouldAccentRailAtDepth(i, row, depth);
                drawList->AddLine(ImVec2(x, rowStart.y + rowPaddingY),
                                  ImVec2(x, rowBottomY),
                                  accentRail ? activeBranchColor : railColor,
                                  accentRail ? 1.8f : 1.15f);
            }

            if (row.depth > 0)
            {
                const float railX = baseX + static_cast<float>(row.depth - 1) * indentStep + railOffset;
                const float branchEndX = iconX - 5.0f;
                const bool continueBelow = subtreeContinuesPastRow(i, row.depth);
                const ImU32 branchColor = selected || onActiveLineage
                                              ? activeBranchColor
                                              : (row.matchedFilter ? matchBranchColor : railColor);
                const float branchThickness = selected || onActiveLineage ? 2.0f : (row.matchedFilter ? 1.55f : 1.15f);

                drawList->AddLine(ImVec2(railX, rowStart.y + rowPaddingY),
                                  ImVec2(railX, rowMidY),
                                  branchColor,
                                  branchThickness);
                if (continueBelow)
                {
                    const bool accentBelow = shouldAccentRailAtDepth(i, row, row.depth - 1);
                    drawList->AddLine(ImVec2(railX, rowMidY),
                                      ImVec2(railX, rowBottomY),
                                      accentBelow ? activeBranchColor : railColor,
                                      accentBelow ? 1.8f : 1.15f);
                }

                drawList->AddBezierCubic(ImVec2(railX, rowMidY),
                                         ImVec2(railX + indentStep * 0.18f, rowMidY),
                                         ImVec2(branchEndX - indentStep * 0.20f, rowMidY),
                                         ImVec2(branchEndX, rowMidY),
                                         branchColor,
                                         branchThickness,
                                         16);
            }

            const ImU32 iconColor = selected || onActiveLineage
                                        ? activeBranchColor
                                        : (row.matchedFilter ? matchBranchColor : ImGui::ColorConvertFloat4ToU32(ImVec4(Primary.x, Primary.y, Primary.z, 0.88f)));
            const ImVec2 iconMin(iconX, rowMidY - iconSize * 0.5f);
            const ImVec2 iconMax(iconX + iconSize, rowMidY + iconSize * 0.5f);
            if (row.isDirectory)
            {
                const ImVec2 tabMin(iconMin.x + 1.5f, iconMin.y - 0.5f);
                const ImVec2 tabMax(iconMin.x + 5.6f, iconMin.y + 2.8f);
                const ImVec2 bodyMin(iconMin.x + 0.8f, iconMin.y + 1.2f);
                const ImVec2 bodyMax(iconMax.x - 0.6f, iconMax.y - 0.6f);
                drawList->AddRectFilled(bodyMin,
                                        bodyMax,
                                        ImGui::ColorConvertFloat4ToU32(ImVec4(Primary.x, Primary.y, Primary.z, selected || onActiveLineage ? 0.10f : 0.05f)),
                                        2.0f);
                drawList->AddRect(bodyMin, bodyMax, iconColor, 2.0f, 0, selected || onActiveLineage ? 1.6f : 1.25f);
                drawList->AddRectFilled(tabMin,
                                        tabMax,
                                        ImGui::ColorConvertFloat4ToU32(ImVec4(Primary.x, Primary.y, Primary.z, selected || onActiveLineage ? 0.12f : 0.06f)),
                                        1.6f);
                drawList->AddRect(tabMin, tabMax, iconColor, 1.6f, 0, selected || onActiveLineage ? 1.5f : 1.15f);
            }
            else
            {
                const ImVec2 center(iconMin.x + iconSize * 0.5f, rowMidY);
                drawList->AddCircleFilled(center, 2.3f, iconColor, 16);
                if (selected || onActiveLineage)
                {
                    drawList->AddCircle(center, 4.1f, iconColor, 16, 1.2f);
                }
            }

            std::string label = DisplayNameForTreeRow(row);
            if (row.isDirectory && label != ".")
            {
                label += "/";
            }

            drawList->AddText(ImVec2(textX, rowStart.y + 1.0f),
                              ImGui::ColorConvertFloat4ToU32(color),
                              label.c_str());
        }
        ImGui::EndChild();
    }
}
