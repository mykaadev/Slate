#include "Modes/Slate/SlateModeBase.h"

#include "App/Slate/Commands/SlateCommandDispatcher.h"
#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Editor/SlateEditorContext.h"
#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "Core/Runtime/CommandRegistry.h"
#include "Core/Runtime/ToolRegistry.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;
    void SlateModeBase::OnExit(Software::Core::Runtime::AppContext& context)
    {
        EditorContext(context).SetNativeEditorVisible(false);
        m_prompt = {};
        m_confirm = {};
        if (auto* search = SearchOverlay(context))
        {
            search->Reset();
        }
        if (auto* workspaces = WorkspaceSwitcherOverlay(context))
        {
            workspaces->Reset();
        }
        m_searchOverlayOpen = false;
        m_workspaceOverlayOpen = false;
        if (auto* todos = TodoOverlay(context))
        {
            todos->Reset();
        }
        m_helpOpen = false;
    }

    void SlateModeBase::DrawUI(Software::Core::Runtime::AppContext& context)
    {
        m_nowSeconds = context.frame.elapsedSeconds;

        std::string backgroundError;
        if (WorkspaceContext(context).ConsumeBackgroundError(&backgroundError))
        {
            SetError(backgroundError);
        }

        HandleGlobalKeys(context);
        const bool overlayOpen =
            m_helpOpen || m_prompt.open || m_confirm.open || SearchOpen() ||
            (WorkspaceSwitcherOverlay(context) && WorkspaceSwitcherOverlay(context)->IsOpen()) ||
            (TodoOverlay(context) && TodoOverlay(context)->IsAnyOpen());
        if (auto* todos = TodoOverlay(context); todos && todos->IsFormOpen())
        {
            EditorContext(context).ReleaseNativeEditorFocus();
        }
        EditorContext(context).SetNativeEditorVisible(WantsNativeEditorVisible(context) && !overlayOpen);

        DrawRootBegin(context);

        const bool handleInput = !overlayOpen;
        if (m_helpOpen)
        {
            DrawHelp();
        }
        else
        {
            DrawMode(context, handleInput);
        }

        if (SearchOpen())
        {
            DrawSearchOverlay(context);
        }
        if (auto* workspaces = WorkspaceSwitcherOverlay(context); workspaces && workspaces->IsOpen())
        {
            DrawWorkspaceOverlay(context);
        }
        if (auto* todos = TodoOverlay(context); todos && todos->IsAnyOpen())
        {
            DrawTodoOverlay(context);
        }
        if (m_prompt.open)
        {
            DrawPromptOverlay(context);
        }
        if (m_confirm.open)
        {
            DrawConfirmOverlay(context);
        }

        DrawStatusLine(context);
        ImGui::End();
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(12);
    }

    bool SlateModeBase::WantsNativeEditorVisible(const Software::Core::Runtime::AppContext& context) const
    {
        (void)context;
        return false;
    }

    void SlateModeBase::DrawHelp() const
    {
        ImGui::TextColored(Cyan, "shortcuts");
        ImGui::Separator();
        TextLine("arrows", "move");
        TextLine("enter", "open");
        TextLine("esc", "back");
        TextLine("/", "search");
        TextLine(":", "command");
        TextLine("^s", "save");
        TextLine("j", "journal");
        TextLine("n", "new");
        TextLine("f", "files");
        TextLine("t", "todos");
        TextLine("c", "config");
        TextLine("a", "folder");
        TextLine("w", "workspaces");
        TextLine("m", "move");
        TextLine("d", "delete");
        TextLine("?", "help");
        TextLine("q", "quit");
    }

    void SlateModeBase::DrawFooterControls(Software::Core::Runtime::AppContext& context)
    {
        (void)context;
    }

    Software::Slate::SlateWorkspaceContext& SlateModeBase::WorkspaceContext(Software::Core::Runtime::AppContext& context) const
    {
        return *context.services.Resolve<Software::Slate::SlateWorkspaceContext>();
    }

    Software::Slate::SlateEditorContext& SlateModeBase::EditorContext(Software::Core::Runtime::AppContext& context) const
    {
        return *context.services.Resolve<Software::Slate::SlateEditorContext>();
    }

    Software::Slate::SlateUiState& SlateModeBase::UiState(Software::Core::Runtime::AppContext& context) const
    {
        return *context.services.Resolve<Software::Slate::SlateUiState>();
    }

    Software::Slate::SlateSearchOverlayController* SlateModeBase::SearchOverlay(Software::Core::Runtime::AppContext& context) const
    {
        return context.services.Resolve<Software::Slate::SlateSearchOverlayController>().get();
    }

    Software::Slate::SlateWorkspaceSwitcherOverlay* SlateModeBase::WorkspaceSwitcherOverlay(Software::Core::Runtime::AppContext& context) const
    {
        return context.services.Resolve<Software::Slate::SlateWorkspaceSwitcherOverlay>().get();
    }

    Software::Slate::SlateTodoOverlayController* SlateModeBase::TodoOverlay(Software::Core::Runtime::AppContext& context) const
    {
        return context.services.Resolve<Software::Slate::SlateTodoOverlayController>().get();
    }

    void SlateModeBase::ActivateMode(const char* modeName, Software::Core::Runtime::AppContext& context) const
    {
        context.tools.Activate(modeName, context);
    }

    void SlateModeBase::SetStatus(std::string message)
    {
        m_status = std::move(message);
        m_statusSeconds = m_nowSeconds;
        m_statusIsError = false;
    }

    void SlateModeBase::SetError(std::string message)
    {
        m_status = std::move(message);
        m_statusSeconds = m_nowSeconds;
        m_statusIsError = true;
    }

    bool SlateModeBase::IsTodoSlashCommand(std::string_view text)
    {
        return Software::Slate::SlateTodoOverlayController::IsSlashCommand(text);
    }

    void SlateModeBase::BeginCommandPrompt()
    {
        BeginPrompt("command", "", [this](const std::string& value, Software::Core::Runtime::AppContext& context) {
            HandleCommand(value, context);
        });
    }

    void SlateModeBase::BeginPrompt(std::string title, std::string initialValue, PromptCallback callback)
    {
        m_prompt.open = true;
        m_prompt.title = std::move(title);
        m_prompt.buffer = std::move(initialValue);
        m_prompt.focus = true;
        m_prompt.callback = std::move(callback);
    }

    void SlateModeBase::BeginConfirm(std::string message, bool destructive, ConfirmCallback callback,
                                     std::string acceptLabel, std::string declineLabel, std::string cancelLabel)
    {
        m_confirm.open = true;
        m_confirm.message = std::move(message);
        m_confirm.destructive = destructive;
        m_confirm.acceptLabel = acceptLabel.empty() ? "yes" : std::move(acceptLabel);
        m_confirm.declineLabel = std::move(declineLabel);
        m_confirm.cancelLabel = cancelLabel.empty() ? "cancel" : std::move(cancelLabel);
        m_confirm.callback = std::move(callback);
    }

    void SlateModeBase::BeginQuitConfirm()
    {
        BeginConfirm("quit Slate?", false,
                     [this](bool accepted, Software::Core::Runtime::AppContext& context) {
                         if (!accepted)
                         {
                             return;
                         }

                         if (SaveActiveDocument(context))
                         {
                             context.quitRequested = true;
                         }
                     });
    }

    void SlateModeBase::BeginSearchOverlay(bool clearQuery, Software::Core::Runtime::AppContext& context,
                                           SearchOverlayScope scope)
    {
        EditorContext(context).CommitToActiveDocument(WorkspaceContext(context).Documents(), m_nowSeconds);
        if (auto* search = SearchOverlay(context))
        {
            search->Open(clearQuery, scope);
            m_searchOverlayOpen = true;
            m_searchOverlayScope = search->Scope();
            m_searchMode = search->Mode();
        }
    }

    void SlateModeBase::CloseSearchOverlay(Software::Core::Runtime::AppContext& context)
    {
        if (auto* search = SearchOverlay(context))
        {
            search->Close();
        }
        m_searchOverlayOpen = false;
    }

    bool SlateModeBase::SaveActiveDocument(Software::Core::Runtime::AppContext& context)
    {
        auto& workspace = WorkspaceContext(context);
        auto& editor = EditorContext(context);
        editor.CommitToActiveDocument(workspace.Documents(), m_nowSeconds);

        std::string error;
        if (workspace.SaveDocument(&error))
        {
            editor.NotifyDocumentSaved();
            SetStatus("saved");
            return true;
        }

        SetError(error);
        return false;
    }

    bool SlateModeBase::OpenDocument(const Software::Slate::fs::path& relativePath,
                                     Software::Core::Runtime::AppContext& context,
                                     bool recordHistory)
    {
        auto& workspace = WorkspaceContext(context);
        auto& editor = EditorContext(context);
        auto& ui = UiState(context);

        if (workspace.Documents().HasOpenDocument())
        {
            if (!SaveActiveDocument(context))
            {
                return false;
            }
        }

        std::string error;
        if (!workspace.OpenDocument(relativePath, &error))
        {
            SetError(error);
            return false;
        }

        editor.LoadFromActiveDocument(workspace.Documents());
        ui.editorView = Software::Slate::SlateEditorView::Document;
        ui.folderPickerActive = false;
        ui.folderPickerAction = Software::Slate::FolderPickerAction::None;
        CloseSearchOverlay(context);

        if (recordHistory)
        {
            const auto normalized = Software::Slate::PathUtils::NormalizeRelative(relativePath);
            if (ui.documentHistoryIndex + 1 < static_cast<int>(ui.documentHistory.size()))
            {
                ui.documentHistory.erase(ui.documentHistory.begin() + ui.documentHistoryIndex + 1,
                                         ui.documentHistory.end());
            }
            if (ui.documentHistory.empty() || ui.documentHistory.back() != normalized)
            {
                ui.documentHistory.push_back(normalized);
            }
            ui.documentHistoryIndex = static_cast<int>(ui.documentHistory.size()) - 1;
        }

        SetStatus("opened " + relativePath.generic_string());
        ActivateMode(Software::Slate::ModeIds::Editor, context);

        const auto* document = workspace.Documents().Active();
        if (document && document->recoveryAvailable)
        {
            BeginConfirm("newer recovery found.", false,
                         [this](bool accepted, Software::Core::Runtime::AppContext& callbackContext) {
                             auto& callbackWorkspace = WorkspaceContext(callbackContext);
                             auto& callbackEditor = EditorContext(callbackContext);
                             std::string recoveryError;
                             if (accepted)
                             {
                                 if (callbackWorkspace.Documents().RestoreRecovery(&recoveryError))
                                 {
                                     callbackEditor.LoadFromActiveDocument(callbackWorkspace.Documents());
                                     SetStatus("recovery restored");
                                 }
                                 else
                                 {
                                     SetError(recoveryError);
                                 }
                             }
                             else
                             {
                                 if (callbackWorkspace.Documents().DiscardRecovery(&recoveryError))
                                 {
                                     SetStatus("recovery discarded");
                                 }
                                 else
                                 {
                                     SetError(recoveryError);
                                 }
                             }
                         },
                         "restore", "discard", "later");
        }

        return true;
    }

    bool SlateModeBase::NavigateDocumentHistory(Software::Core::Runtime::AppContext& context, int delta)
    {
        auto& ui = UiState(context);
        if (delta == 0 || ui.documentHistory.empty())
        {
            return false;
        }

        if (ui.documentHistoryIndex < 0)
        {
            ui.documentHistoryIndex = static_cast<int>(ui.documentHistory.size()) - 1;
        }

        const int nextIndex = ui.documentHistoryIndex + delta;
        if (nextIndex < 0 || nextIndex >= static_cast<int>(ui.documentHistory.size()))
        {
            SetStatus(delta < 0 ? "no previous note" : "no next note");
            return false;
        }

        const auto target = ui.documentHistory[static_cast<std::size_t>(nextIndex)];
        if (!OpenDocument(target, context, false))
        {
            return false;
        }

        ui.documentHistoryIndex = nextIndex;
        SetStatus(std::string(delta < 0 ? "back: " : "forward: ") + target.generic_string());
        return true;
    }

    bool SlateModeBase::OpenWorkspace(const Software::Slate::fs::path& root,
                                      Software::Core::Runtime::AppContext& context)
    {
        auto& workspace = WorkspaceContext(context);
        auto& editor = EditorContext(context);
        auto& ui = UiState(context);

        if (!SaveActiveDocument(context))
        {
            return false;
        }

        std::string error;
        if (!workspace.OpenWorkspace(root, &error))
        {
            SetError(error);
            return false;
        }

        editor.Clear();
        ui.ResetForWorkspace(workspace.Workspace());
        SetStatus("workspace opened: " + workspace.Workspace().Root().string());
        ActivateMode(Software::Slate::ModeIds::Home, context);
        return true;
    }

    bool SlateModeBase::OpenVault(const Software::Slate::WorkspaceVault& vault,
                                  Software::Core::Runtime::AppContext& context)
    {
        auto& workspace = WorkspaceContext(context);
        auto& editor = EditorContext(context);
        auto& ui = UiState(context);

        if (!SaveActiveDocument(context))
        {
            return false;
        }

        std::string error;
        if (!workspace.OpenVault(vault, &error))
        {
            SetError(error);
            return false;
        }

        editor.Clear();
        ui.ResetForWorkspace(workspace.Workspace());
        SetStatus(vault.emoji + " " + vault.title);
        ActivateMode(Software::Slate::ModeIds::Home, context);
        return true;
    }

    bool SlateModeBase::OpenTodayJournal(Software::Core::Runtime::AppContext& context)
    {
        Software::Slate::fs::path daily;
        std::string error;
        if (!WorkspaceContext(context).OpenTodayJournal(&daily, &error))
        {
            SetError(error);
            return false;
        }
        return OpenDocument(daily, context);
    }

    void SlateModeBase::ShowBrowser(Software::Slate::SlateBrowserView view,
                                    Software::Core::Runtime::AppContext& context,
                                    bool clearFilter)
    {
        if (!WorkspaceContext(context).HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        auto& ui = UiState(context);
        ui.browserView = view;
        if (clearFilter)
        {
            ui.filterText.clear();
        }
        ui.filterActive = false;
        ui.focusFilter = false;
        ui.folderPickerActive = false;
        ui.folderPickerAction = Software::Slate::FolderPickerAction::None;
        ui.navigation.Reset();
        ActivateMode(Software::Slate::ModeIds::Browser, context);
    }

    void SlateModeBase::BeginNewNoteFlow(Software::Core::Runtime::AppContext& context)
    {
        if (!WorkspaceContext(context).HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        auto& ui = UiState(context);
        ui.browserView = Software::Slate::SlateBrowserView::FileTree;
        ui.pendingFolderPath = ".";
        ui.folderPickerActive = true;
        ui.folderPickerAction = Software::Slate::FolderPickerAction::NewNote;
        ui.filterText.clear();
        ui.filterActive = false;
        ui.focusFilter = false;
        ui.navigation.Reset();
        ActivateMode(Software::Slate::ModeIds::Browser, context);
        SetStatus("choose a folder for the new note");
    }

    void SlateModeBase::BeginFolderCreateFlow(Software::Core::Runtime::AppContext& context)
    {
        if (!WorkspaceContext(context).HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        auto& ui = UiState(context);
        ui.pendingFolderPath = SelectedTreeFolderPath(ui);
        BeginPrompt("new folder name", "New Folder",
                    [this](const std::string& value, Software::Core::Runtime::AppContext& callbackContext) {
                        auto& workspace = WorkspaceContext(callbackContext);
                        auto& uiState = UiState(callbackContext);
                        Software::Slate::fs::path created;
                        std::string error;
                        if (!workspace.CreateNewFolder(uiState.pendingFolderPath, value, &created, &error))
                        {
                            SetError(error);
                            ShowBrowser(Software::Slate::SlateBrowserView::FileTree, callbackContext, false);
                            return;
                        }

                        uiState.collapsedFolders.erase(Software::Slate::TreePathKey(uiState.pendingFolderPath));
                        uiState.collapsedFolders.insert(Software::Slate::TreePathKey(created));
                        uiState.browserView = Software::Slate::SlateBrowserView::FileTree;
                        ActivateMode(Software::Slate::ModeIds::Browser, callbackContext);
                        SetStatus("created folder " + created.generic_string());
                    });
    }

    void SlateModeBase::ShowWorkspaceSwitcher(Software::Core::Runtime::AppContext& context)
    {
        if (!WorkspaceContext(context).HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        if (auto* workspaces = WorkspaceSwitcherOverlay(context))
        {
            workspaces->Open(WorkspaceContext(context));
            m_workspaceOverlayOpen = true;
        }
    }

    void SlateModeBase::ShowWorkspaceSetup(Software::Core::Runtime::AppContext& context)
    {
        auto& ui = UiState(context);
        ui.workspaceView = Software::Slate::SlateWorkspaceView::Setup;
        ActivateMode(Software::Slate::ModeIds::Workspace, context);
    }

    void SlateModeBase::ShowSettings(Software::Core::Runtime::AppContext& context)
    {
        if (!WorkspaceContext(context).HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        auto& ui = UiState(context);
        ui.navigation.SetCount(2);
        ui.navigation.Reset();
        ActivateMode(Software::Slate::ModeIds::Settings, context);
    }

    void SlateModeBase::ShowTodos(Software::Core::Runtime::AppContext& context)
    {
        if (!WorkspaceContext(context).HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        if (auto* todos = TodoOverlay(context))
        {
            todos->OpenList(context);
        }
    }

    void SlateModeBase::BeginTodoCreate(Software::Core::Runtime::AppContext& context, bool fromSlashCommand)
    {
        if (!WorkspaceContext(context).HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        if (auto* todos = TodoOverlay(context))
        {
            todos->BeginCreate(context, fromSlashCommand);
        }
    }

    void SlateModeBase::ShowHome(Software::Core::Runtime::AppContext& context)
    {
        if (!WorkspaceContext(context).HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        ActivateMode(Software::Slate::ModeIds::Home, context);
    }
    void SlateModeBase::HandleCommand(const std::string& command, Software::Core::Runtime::AppContext& context)
    {
        Software::Slate::SlateCommandHandlers handlers;
        handlers.quit = [this]() {
            BeginQuitConfirm();
        };
        handlers.saveDocument = [this, &context]() {
            return SaveActiveDocument(context);
        };
        handlers.openTodayJournal = [this, &context]() {
            OpenTodayJournal(context);
        };
        handlers.openDocument = [this, &context](const std::string& path) {
            return OpenDocument(path, context);
        };
        handlers.newDocument = [this, &context]() {
            BeginNewNoteFlow(context);
        };
        handlers.newFolder = [this, &context]() {
            BeginFolderCreateFlow(context);
        };
        handlers.showWorkspaceSwitcher = [this, &context]() {
            ShowWorkspaceSwitcher(context);
        };
        handlers.showTodos = [this, &context]() {
            ShowTodos(context);
        };
        handlers.showSettings = [this, &context]() {
            ShowSettings(context);
        };
        handlers.showFiles = [this, &context]() {
            ShowBrowser(Software::Slate::SlateBrowserView::FileTree, context);
        };
        handlers.showSearch = [this, &context](const std::string& query, bool clearQuery) {
            if (!query.empty())
            {
                if (auto* search = SearchOverlay(context))
                {
                    search->SetQuery(query);
                }
            }
            BeginSearchOverlay(clearQuery, context);
        };
        handlers.showWorkspaceSetup = [this, &context]() {
            ShowWorkspaceSetup(context);
        };
        handlers.openWorkspace = [this, &context](const std::string& root) {
            return OpenWorkspace(root, context);
        };

        const auto result = Software::Slate::DispatchSlateCommand(Software::Slate::PathUtils::Trim(command), context, handlers);
        if (!result.handled)
        {
            SetError("unknown command: " + result.message);
        }
        else if (!result.succeeded)
        {
            SetError(result.message.empty() ? ("command failed: " + result.commandId) : result.message);
        }
        else if (!result.message.empty())
        {
            SetStatus(result.message);
        }
    }
    Software::Slate::fs::path SlateModeBase::SelectedTreeFolderPath(const Software::Slate::SlateUiState& ui) const
    {
        if (!ui.navigation.HasSelection() || ui.treeRows.empty())
        {
            return ".";
        }

        const auto& row = ui.treeRows[ui.navigation.Selected()];
        if (row.isDirectory)
        {
            return row.relativePath;
        }

        const auto parent = row.relativePath.parent_path();
        return parent.empty() ? Software::Slate::fs::path(".") : parent;
    }

    bool SlateModeBase::IsKeyPressed(ImGuiKey key) const
    {
        return ImGui::IsKeyPressed(key, false);
    }

    bool SlateModeBase::IsCtrlPressed(ImGuiKey key) const
    {
        const ImGuiIO& io = ImGui::GetIO();
        return io.KeyCtrl && ImGui::IsKeyPressed(key, false);
    }

    bool SlateModeBase::HelpOpen() const
    {
        return m_helpOpen;
    }

    bool SlateModeBase::SearchOpen() const
    {
        return m_searchOverlayOpen;
    }

    bool SlateModeBase::PromptOpen() const
    {
        return m_prompt.open;
    }

    bool SlateModeBase::ConfirmOpen() const
    {
        return m_confirm.open;
    }

    SearchOverlayScope SlateModeBase::SearchScopeValue() const
    {
        return m_searchOverlayScope;
    }

    Software::Slate::SearchMode SlateModeBase::SearchModeValue() const
    {
        return m_searchMode;
    }

    void SlateModeBase::DrawRootBegin(Software::Core::Runtime::AppContext& context)
    {
        const auto& workspace = WorkspaceContext(context);
        const int scrollbarStyle = workspace.HasWorkspaceLoaded()
                                       ? workspace.CurrentEditorSettings().scrollbarStyle
                                       : 1;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Background);
        ImGui::PushStyleColor(ImGuiCol_Text, Primary);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Panel);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Panel);
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(Muted.x, Muted.y, Muted.z, 0.34f));
        ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, ImVec4(Amber.x, Amber.y, Amber.z, 0.42f));
        ImGui::PushStyleColor(ImGuiCol_SeparatorActive, ImVec4(Green.x, Green.y, Green.z, 0.50f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
        PushSlateScrollbarStyle(scrollbarStyle);
        ImGui::Begin("SlateRoot", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
    }

    void SlateModeBase::DrawSearchOverlay(Software::Core::Runtime::AppContext& context)
    {
        auto* search = SearchOverlay(context);
        if (!search)
        {
            m_searchOverlayOpen = false;
            return;
        }

        Software::Slate::SearchOverlayCallbacks callbacks;
        callbacks.openSelection = [this, &context](const Software::Slate::SearchOverlaySelection& selection) {
            OpenSearchResult(selection, context);
        };

        search->Draw(context, callbacks);
        m_searchOverlayOpen = search->IsOpen();
        m_searchOverlayScope = search->Scope();
        m_searchMode = search->Mode();
    }

    void SlateModeBase::DrawWorkspaceOverlay(Software::Core::Runtime::AppContext& context)
    {
        auto* workspaces = WorkspaceSwitcherOverlay(context);
        if (!workspaces)
        {
            m_workspaceOverlayOpen = false;
            return;
        }

        Software::Slate::WorkspaceSwitcherCallbacks callbacks;
        callbacks.openVault = [this, &context](const Software::Slate::WorkspaceVault& vault) {
            OpenVault(vault, context);
        };
        callbacks.showSetup = [this, &context]() {
            ShowWorkspaceSetup(context);
        };
        callbacks.setStatus = [this](std::string message) {
            SetStatus(std::move(message));
        };

        workspaces->Draw(context, callbacks);
        m_workspaceOverlayOpen = workspaces->IsOpen();
    }

    void SlateModeBase::DrawTodoOverlay(Software::Core::Runtime::AppContext& context)
    {
        auto* todos = TodoOverlay(context);
        if (!todos)
        {
            return;
        }

        Software::Slate::TodoOverlayCallbacks callbacks;
        callbacks.openDocument = [this, &context](const Software::Slate::fs::path& relativePath) {
            return OpenDocument(relativePath, context);
        };
        callbacks.restoreEditorFocus = [this, &context]() {
            RestoreEditorFocusIfWanted(context);
        };
        callbacks.setStatus = [this](std::string message) {
            SetStatus(std::move(message));
        };
        callbacks.setError = [this](std::string message) {
            SetError(std::move(message));
        };

        todos->Draw(context, callbacks);
    }

    void SlateModeBase::DrawPromptOverlay(Software::Core::Runtime::AppContext& context)
    {
        const ImVec2 size(std::min(560.0f, std::max(320.0f, ImGui::GetWindowWidth() * 0.50f)), 104.0f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f, ImGui::GetWindowHeight() - 150.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(Cyan.x, Cyan.y, Cyan.z, 0.28f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 14.0f));
        ImGui::BeginChild("PromptOverlay", size, true);
        ImGui::TextColored(Cyan, "%s", m_prompt.title.c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (m_prompt.focus)
        {
            ImGui::SetKeyboardFocusHere();
            m_prompt.focus = false;
        }
        const auto result = InputTextString("##prompt", m_prompt.buffer, ImGuiInputTextFlags_EnterReturnsTrue);
        if (result.submitted)
        {
            auto callback = std::move(m_prompt.callback);
            const std::string value = Software::Slate::PathUtils::Trim(m_prompt.buffer);
            m_prompt = {};
            if (callback)
            {
                callback(value, context);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        if (IsKeyPressed(ImGuiKey_Escape))
        {
            m_prompt = {};
        }
    }

    void SlateModeBase::DrawConfirmOverlay(Software::Core::Runtime::AppContext& context)
    {
        const ImVec2 size(std::min(520.0f, std::max(320.0f, ImGui::GetWindowWidth() * 0.46f)), 128.0f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   (ImGui::GetWindowHeight() - size.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(Cyan.x, Cyan.y, Cyan.z, 0.28f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 14.0f));
        ImGui::BeginChild("ConfirmOverlay", size, true);
        ImGui::Dummy(ImVec2(1.0f, 18.0f));
        TextCentered(m_confirm.message.c_str(), m_confirm.destructive ? Red : Amber);
        ImGui::Dummy(ImVec2(1.0f, 12.0f));

        std::string actions = "(y) " + m_confirm.acceptLabel;
        if (!m_confirm.declineLabel.empty())
        {
            actions += "   (n) " + m_confirm.declineLabel;
        }
        actions += "   (esc) " + m_confirm.cancelLabel;
        DrawShortcutTextCentered(actions);
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        if (IsKeyPressed(ImGuiKey_Y) || IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter))
        {
            auto callback = std::move(m_confirm.callback);
            m_confirm = {};
            if (callback)
            {
                callback(true, context);
            }
        }
        else if (!m_confirm.declineLabel.empty() && IsKeyPressed(ImGuiKey_N))
        {
            auto callback = std::move(m_confirm.callback);
            m_confirm = {};
            if (callback)
            {
                callback(false, context);
            }
        }
        else if (IsKeyPressed(ImGuiKey_Escape))
        {
            m_confirm = {};
        }
    }

    void SlateModeBase::DrawStatusLine(Software::Core::Runtime::AppContext& context)
    {
        DrawFooterControls(context);

        const float helperY = ImGui::GetWindowHeight() - 52.0f;
        ImGui::SetCursorPosY(helperY);
        ImGui::Separator();
        DrawShortcutText(HelperText(context));
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 26.0f);
        ImVec4 statusColor = m_statusIsError ? Red : Muted;
        if (!m_statusIsError)
        {
            const float age = static_cast<float>(std::max(0.0, m_nowSeconds - m_statusSeconds));
            statusColor.w = std::clamp(1.0f - age / 4.0f, 0.22f, 1.0f);
        }
        ImGui::TextColored(statusColor, "%s", m_status.c_str());
    }

    std::string SlateModeBase::HelperText(const Software::Core::Runtime::AppContext& context) const
    {
        if (m_helpOpen)
        {
            return "(esc) close   (?) help";
        }
        if (m_searchOverlayOpen)
        {
            if (auto* search = SearchOverlay(const_cast<Software::Core::Runtime::AppContext&>(context)))
            {
                return search->HelperText();
            }
        }
        if (m_workspaceOverlayOpen)
        {
            if (auto* workspaces = WorkspaceSwitcherOverlay(const_cast<Software::Core::Runtime::AppContext&>(context)))
            {
                return workspaces->HelperText();
            }
        }
        if (auto* todos = TodoOverlay(const_cast<Software::Core::Runtime::AppContext&>(context)); todos && todos->IsAnyOpen())
        {
            return todos->HelperText();
        }
        if (m_prompt.open)
        {
            return "(enter) accept   (esc) cancel";
        }
        if (m_confirm.open)
        {
            std::string helper = "(y) " + m_confirm.acceptLabel;
            if (!m_confirm.declineLabel.empty())
            {
                helper += "   (n) " + m_confirm.declineLabel;
            }
            helper += "   (esc) " + m_confirm.cancelLabel;
            return helper;
        }

        return ModeHelperText(context);
    }

    void SlateModeBase::HandleGlobalKeys(Software::Core::Runtime::AppContext& context)
    {
        const ImGuiIO& io = ImGui::GetIO();

        if (IsCtrlPressed(ImGuiKey_S))
        {
            SaveActiveDocument(context);
        }

        if (m_prompt.open || m_confirm.open || m_searchOverlayOpen || m_workspaceOverlayOpen ||
            (TodoOverlay(context) && TodoOverlay(context)->IsAnyOpen()))
        {
            return;
        }

        if (m_helpOpen)
        {
            if (IsShiftQuestion() || IsKeyPressed(ImGuiKey_Escape))
            {
                m_helpOpen = false;
            }
            return;
        }

        if (io.KeyAlt && IsKeyPressed(ImGuiKey_LeftArrow))
        {
            NavigateDocumentHistory(context, -1);
            return;
        }
        if (io.KeyAlt && IsKeyPressed(ImGuiKey_RightArrow))
        {
            NavigateDocumentHistory(context, 1);
            return;
        }

        if (EditorContext(context).IsTextFocused() || io.WantTextInput)
        {
            return;
        }

        if (IsShiftQuestion())
        {
            m_helpOpen = true;
            return;
        }

        if (io.KeyShift && IsKeyPressed(ImGuiKey_Semicolon))
        {
            BeginCommandPrompt();
            return;
        }

        if (IsKeyPressed(ImGuiKey_W))
        {
            ShowWorkspaceSwitcher(context);
            return;
        }
        if (IsKeyPressed(ImGuiKey_T))
        {
            ShowTodos(context);
            return;
        }
        if (IsKeyPressed(ImGuiKey_C))
        {
            ShowSettings(context);
            return;
        }

        if (IsCtrlPressed(ImGuiKey_Q))
        {
            BeginQuitConfirm();
        }
    }

    void SlateModeBase::OpenSearchResult(const Software::Slate::SearchOverlaySelection& selection,
                                         Software::Core::Runtime::AppContext& context)
    {
        if (selection.scope == SearchOverlayScope::Document)
        {
            auto& ui = UiState(context);
            ui.editorView = Software::Slate::SlateEditorView::Document;
            EditorContext(context).JumpToLine(selection.result.line);
            ActivateMode(Software::Slate::ModeIds::Editor, context);
            return;
        }

        if (OpenDocument(selection.result.relativePath, context) &&
            selection.mode == Software::Slate::SearchMode::FullText && selection.result.line > 0)
        {
            EditorContext(context).JumpToLine(selection.result.line);
        }
    }

    void SlateModeBase::RestoreEditorFocusIfWanted(Software::Core::Runtime::AppContext& context)
    {
        if (WantsNativeEditorVisible(context))
        {
            EditorContext(context).Editor().RequestFocus();
            EditorContext(context).SetTextFocused(true);
        }
    }
}


