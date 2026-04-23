#include "Modes/Slate/SlateModeBase.h"

#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Editor/SlateEditorContext.h"
#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "Core/Runtime/ToolRegistry.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        const char* WorkspaceSearchModeLabel(Software::Slate::SearchMode mode)
        {
            switch (mode)
            {
            case Software::Slate::SearchMode::FileNames:
                return "docs";
            case Software::Slate::SearchMode::FullText:
                return "text";
            case Software::Slate::SearchMode::Recent:
                return "recent";
            }
            return "docs";
        }

        const ImVec4& WorkspaceSearchModeColor(Software::Slate::SearchMode mode)
        {
            switch (mode)
            {
            case Software::Slate::SearchMode::FileNames:
                return Cyan;
            case Software::Slate::SearchMode::FullText:
                return Amber;
            case Software::Slate::SearchMode::Recent:
                return Green;
            }
            return Cyan;
        }

        const char* WorkspaceSearchModeHint(Software::Slate::SearchMode mode)
        {
            switch (mode)
            {
            case Software::Slate::SearchMode::FileNames:
                return "type to search docs. Tab cycles docs / text / recent";
            case Software::Slate::SearchMode::FullText:
                return "type to search note text. Tab cycles docs / text / recent";
            case Software::Slate::SearchMode::Recent:
                return "recent files. Type to filter. Tab cycles docs / text / recent";
            }
            return "type to search";
        }

        Software::Slate::SearchMode NextWorkspaceSearchMode(Software::Slate::SearchMode mode)
        {
            switch (mode)
            {
            case Software::Slate::SearchMode::FileNames:
                return Software::Slate::SearchMode::FullText;
            case Software::Slate::SearchMode::FullText:
                return Software::Slate::SearchMode::Recent;
            case Software::Slate::SearchMode::Recent:
                return Software::Slate::SearchMode::FileNames;
            }
            return Software::Slate::SearchMode::FileNames;
        }

        std::vector<Software::Slate::SearchResult> QueryRecentFiles(const Software::Slate::SlateWorkspaceContext& workspace,
                                                                    const std::string& query,
                                                                    std::size_t limit)
        {
            std::vector<Software::Slate::SearchResult> results;
            for (const auto& recent : workspace.Workspace().RecentFiles())
            {
                if (!query.empty() && !ContainsFilter(recent.generic_string(), query.c_str()))
                {
                    continue;
                }

                Software::Slate::SearchResult result;
                result.relativePath = recent;
                result.path = recent;
                result.line = 1;
                result.column = 1;
                result.snippet = "recent";
                result.score = static_cast<int>(limit - std::min(limit, results.size()));
                results.push_back(std::move(result));
                if (results.size() >= limit)
                {
                    break;
                }
            }
            return results;
        }

        std::string NextSearchModeHelper(Software::Slate::SearchMode mode)
        {
            std::string helper = "(tab) ";
            helper += WorkspaceSearchModeLabel(NextWorkspaceSearchMode(mode));
            helper += "   (up/down) move   (enter) open   (esc) close";
            return helper;
        }

        constexpr std::array<const char*, 5> TodoFilterLabels{{"all", "open", "research", "doing", "done"}};

        const char* TodoFilterLabel(int filter)
        {
            const int index = std::clamp(filter, 0, static_cast<int>(TodoFilterLabels.size() - 1));
            return TodoFilterLabels[static_cast<std::size_t>(index)];
        }

        int TodoStateSortKey(Software::Slate::TodoState state)
        {
            switch (state)
            {
            case Software::Slate::TodoState::Open:
                return 0;
            case Software::Slate::TodoState::Research:
                return 1;
            case Software::Slate::TodoState::Doing:
                return 2;
            case Software::Slate::TodoState::Done:
                return 3;
            }
            return 0;
        }

        const ImVec4& TodoFilterColor(int filter)
        {
            switch (std::clamp(filter, 0, 4))
            {
            case 1:
                return TodoStateColor(Software::Slate::TodoState::Open);
            case 2:
                return TodoStateColor(Software::Slate::TodoState::Research);
            case 3:
                return TodoStateColor(Software::Slate::TodoState::Doing);
            case 4:
                return TodoStateColor(Software::Slate::TodoState::Done);
            default:
                return Primary;
            }
        }

        bool MatchesTodoFilter(const Software::Slate::TodoTicket& todo, int filter)
        {
            switch (std::clamp(filter, 0, 4))
            {
            case 1:
                return todo.state == Software::Slate::TodoState::Open;
            case 2:
                return todo.state == Software::Slate::TodoState::Research;
            case 3:
                return todo.state == Software::Slate::TodoState::Doing;
            case 4:
                return todo.state == Software::Slate::TodoState::Done;
            default:
                return true;
            }
        }

        bool TodoMatchesQuery(const Software::Slate::TodoTicket& todo, const std::string& query)
        {
            if (query.empty())
            {
                return true;
            }

            if (ContainsFilter(todo.title, query.c_str()) ||
                ContainsFilter(todo.description, query.c_str()) ||
                ContainsFilter(todo.relativePath.generic_string(), query.c_str()) ||
                ContainsFilter(Software::Slate::MarkdownService::TodoStateLabel(todo.state), query.c_str()))
            {
                return true;
            }

            for (const auto& tag : todo.tags)
            {
                const std::string needle = "#" + tag;
                if (ContainsFilter(needle, query.c_str()) || ContainsFilter(tag, query.c_str()))
                {
                    return true;
                }
            }
            return false;
        }

        std::vector<Software::Slate::TodoTicket> CollectTodos(const Software::Slate::SlateWorkspaceContext& workspace)
        {
            std::vector<Software::Slate::TodoTicket> todos;
            Software::Slate::MarkdownService markdown;
            const auto* activeDocument = workspace.Documents().Active();
            const auto activePath = activeDocument ? Software::Slate::PathUtils::NormalizeRelative(activeDocument->relativePath)
                                                   : Software::Slate::fs::path{};
            for (const auto& relativePath : workspace.Workspace().MarkdownFiles())
            {
                std::string text;
                const auto normalized = Software::Slate::PathUtils::NormalizeRelative(relativePath);
                if (activeDocument && normalized == activePath)
                {
                    text = activeDocument->text;
                }
                else if (!Software::Slate::DocumentService::ReadTextFile(workspace.Workspace().Resolve(relativePath),
                                                                         &text,
                                                                         nullptr))
                {
                    continue;
                }

                auto summary = markdown.Parse(text);
                for (auto& todo : summary.todos)
                {
                    todo.relativePath = relativePath;
                    todos.push_back(std::move(todo));
                }
            }

            std::sort(todos.begin(), todos.end(), [](const auto& left, const auto& right) {
                const int leftState = TodoStateSortKey(left.state);
                const int rightState = TodoStateSortKey(right.state);
                if (leftState != rightState)
                {
                    return leftState < rightState;
                }
                if (left.relativePath != right.relativePath)
                {
                    return left.relativePath.generic_string() < right.relativePath.generic_string();
                }
                return left.line < right.line;
            });
            return todos;
        }
    }

    void SlateModeBase::OnExit(Software::Core::Runtime::AppContext& context)
    {
        EditorContext(context).SetNativeEditorVisible(false);
        m_prompt = {};
        m_confirm = {};
        m_searchOverlayOpen = false;
        m_workspaceOverlayOpen = false;
        m_todoOverlayOpen = false;
        m_todoForm = {};
        m_searchNavigation.Reset();
        m_todoNavigation.Reset();
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
            m_helpOpen || m_prompt.open || m_confirm.open || m_searchOverlayOpen || m_workspaceOverlayOpen ||
            m_todoOverlayOpen || m_todoForm.open;
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

        if (m_searchOverlayOpen)
        {
            DrawSearchOverlay(context);
        }
        if (m_workspaceOverlayOpen)
        {
            DrawWorkspaceOverlay(context);
        }
        if (m_todoOverlayOpen)
        {
            DrawTodoOverlay(context);
        }
        if (m_todoForm.open)
        {
            DrawTodoFormOverlay(context);
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
        if (clearQuery)
        {
            m_searchBuffer.clear();
            m_searchNavigation.Reset();
        }
        m_searchOverlayScope = scope;
        if (scope == SearchOverlayScope::Workspace)
        {
            m_searchMode = Software::Slate::SearchMode::FileNames;
        }
        m_searchOverlayOpen = true;
        m_focusSearch = true;
    }

    void SlateModeBase::CloseSearchOverlay()
    {
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
        CloseSearchOverlay();

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

        auto& ui = UiState(context);
        const auto& registry = WorkspaceContext(context).WorkspaceRegistry();
        const auto& vaults = registry.Vaults();
        ui.workspaceNavigation.SetCount(vaults.size());
        ui.workspaceNavigation.Reset();
        if (const auto* activeVault = registry.ActiveVault())
        {
            for (std::size_t i = 0; i < vaults.size(); ++i)
            {
                if (vaults[i].id == activeVault->id)
                {
                    ui.workspaceNavigation.SetSelected(i);
                    break;
                }
            }
        }
        m_workspaceOverlayOpen = true;
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

        EditorContext(context).CommitToActiveDocument(WorkspaceContext(context).Documents(), m_nowSeconds);
        EditorContext(context).ReleaseNativeEditorFocus();
        m_todoOverlayOpen = true;
        m_focusTodoSearch = true;
        m_todoNavigation.Reset();
    }

    void SlateModeBase::BeginTodoCreate(Software::Core::Runtime::AppContext& context)
    {
        if (!WorkspaceContext(context).HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }
        auto& workspace = WorkspaceContext(context);
        auto& editor = EditorContext(context);
        auto& documents = workspace.Documents();

        std::string activeLine;
        const bool hasActiveLine = editor.ActiveLineText(&activeLine);
        const auto* active = documents.Active();
        const std::size_t activeLineNumber =
            editor.NativeEditorAvailable()
                ? static_cast<std::size_t>(std::max(1, editor.NativeEditorScrollState().caretLine + 1))
                : (editor.Editor().ActiveLine() + 1);

        editor.CommitToActiveDocument(documents, m_nowSeconds);
        editor.ReleaseNativeEditorFocus();
        m_todoForm = {};
        m_todoForm.open = true;
        m_todoForm.focusTitle = true;
        m_todoForm.mode = TodoFormMode::Create;
        m_todoForm.state = Software::Slate::TodoState::Open;
        m_todoForm.replaceActiveLine = hasActiveLine && Software::Slate::PathUtils::Trim(activeLine) == "/todo";
        if (m_todoForm.replaceActiveLine && active)
        {
            m_todoForm.pendingCommandPath = Software::Slate::PathUtils::NormalizeRelative(active->relativePath);
            m_todoForm.pendingCommandLine = activeLineNumber;
        }
        m_todoForm.title = "Untitled todo";
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
        const std::string trimmed = Software::Slate::PathUtils::Trim(command);
        if (trimmed == "q")
        {
            BeginQuitConfirm();
        }
        else if (trimmed == "w")
        {
            SaveActiveDocument(context);
        }
        else if (trimmed == "wq")
        {
            if (SaveActiveDocument(context))
            {
                BeginQuitConfirm();
            }
        }
        else if (trimmed == "today" || trimmed == "j")
        {
            OpenTodayJournal(context);
        }
        else if (trimmed.rfind("open ", 0) == 0)
        {
            OpenDocument(trimmed.substr(5), context);
        }
        else if (trimmed == "new" || trimmed.rfind("new ", 0) == 0)
        {
            BeginNewNoteFlow(context);
        }
        else if (trimmed == "folder" || trimmed == "mkdir")
        {
            BeginFolderCreateFlow(context);
        }
        else if (trimmed == "workspaces" || trimmed == "vaults")
        {
            ShowWorkspaceSwitcher(context);
        }
        else if (trimmed == "todo" || trimmed == "todos")
        {
            ShowTodos(context);
        }
        else if (trimmed == "c" || trimmed == "config" || trimmed == "theme" || trimmed == "settings")
        {
            ShowSettings(context);
        }
        else if (trimmed == "files" || trimmed == "f" || trimmed == "tree" || trimmed == "library" || trimmed == "l")
        {
            ShowBrowser(Software::Slate::SlateBrowserView::FileTree, context);
        }
        else if (trimmed == "search" || trimmed == "s" || trimmed == "docs")
        {
            BeginSearchOverlay(true, context);
        }
        else if (trimmed == "new workspace" || trimmed == "new vault")
        {
            ShowWorkspaceSetup(context);
        }
        else if (trimmed.rfind("workspace ", 0) == 0)
        {
            OpenWorkspace(trimmed.substr(10), context);
        }
        else if (trimmed.rfind("search ", 0) == 0)
        {
            m_searchBuffer = trimmed.substr(7);
            BeginSearchOverlay(false, context);
        }
        else
        {
            SetError("unknown command: " + trimmed);
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
        auto& workspace = WorkspaceContext(context);
        if (m_searchOverlayScope == SearchOverlayScope::Document)
        {
            const auto* document = workspace.Documents().Active();
            m_visibleSearchResults = document ? Software::Slate::SearchService::QueryDocument(document->text,
                                                                                              m_searchBuffer,
                                                                                              100)
                                              : std::vector<Software::Slate::SearchResult>{};
        }
        else
        {
            m_visibleSearchResults = m_searchMode == Software::Slate::SearchMode::Recent
                                         ? QueryRecentFiles(workspace, m_searchBuffer, 100)
                                         : workspace.Search().Query(m_searchBuffer, m_searchMode, 100);
        }
        m_searchNavigation.SetCount(m_visibleSearchResults.size());

        if (m_searchOverlayScope == SearchOverlayScope::Workspace && IsKeyPressed(ImGuiKey_Tab))
        {
            m_searchMode = NextWorkspaceSearchMode(m_searchMode);
            m_searchNavigation.Reset();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        {
            m_searchNavigation.MoveNext();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        {
            m_searchNavigation.MovePrevious();
        }
        if ((IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter)) &&
            m_searchNavigation.HasSelection() && !m_visibleSearchResults.empty())
        {
            if (m_searchOverlayScope == SearchOverlayScope::Document && ImGui::GetIO().KeyShift)
            {
                m_searchNavigation.MovePrevious();
            }
            OpenSelectedSearchResult(context);
            return;
        }
        if (IsKeyPressed(ImGuiKey_Escape))
        {
            CloseSearchOverlay();
            return;
        }

        const ImVec2 size(std::min(760.0f, std::max(360.0f, ImGui::GetWindowWidth() * 0.58f)),
                          std::min(520.0f, std::max(280.0f, ImGui::GetWindowHeight() * 0.52f)));
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   (ImGui::GetWindowHeight() - size.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(Cyan.x, Cyan.y, Cyan.z, 0.28f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
        ImGui::BeginChild("SearchOverlay", size, true);
        const bool documentFind = m_searchOverlayScope == SearchOverlayScope::Document;
        ImGui::TextColored(Cyan, "%s", documentFind ? "find in file" : "search");
        if (!documentFind)
        {
            ImGui::SameLine();
            ImGui::TextColored(WorkspaceSearchModeColor(m_searchMode), "%s", WorkspaceSearchModeLabel(m_searchMode));
        }
        ImGui::SetNextItemWidth(-1.0f);
        if (m_focusSearch)
        {
            ImGui::SetKeyboardFocusHere();
            m_focusSearch = false;
        }
        InputTextString("##SearchQuery", m_searchBuffer, 0);
        ImGui::Separator();

        const bool showQueryHint = m_searchBuffer.empty() &&
                                   (documentFind || m_searchMode != Software::Slate::SearchMode::Recent);
        if (showQueryHint)
        {
            ImGui::TextColored(Muted, "%s", documentFind ? "type to find in this file"
                                                         : WorkspaceSearchModeHint(m_searchMode));
        }
        else if (m_visibleSearchResults.empty())
        {
            ImGui::TextColored(Muted, "no matches");
        }
        else
        {
            for (std::size_t i = 0; i < m_visibleSearchResults.size(); ++i)
            {
                const auto& result = m_visibleSearchResults[i];
                const bool selected = i == m_searchNavigation.Selected();
                if (documentFind)
                {
                    ImGui::TextColored(selected ? Green : Primary, "%s line %zu:%zu", selected ? ">" : " ",
                                       result.line, result.column);
                }
                else
                {
                    std::string label = result.relativePath.generic_string();
                    if (m_searchMode == Software::Slate::SearchMode::FullText)
                    {
                        label += ":" + std::to_string(result.line);
                    }
                    ImGui::TextColored(selected ? Green : Primary, "%s %s", selected ? ">" : " ", label.c_str());
                }
                ImGui::SameLine();
                ImGui::TextColored(Muted, "%s", result.snippet.c_str());
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    void SlateModeBase::DrawWorkspaceOverlay(Software::Core::Runtime::AppContext& context)
    {
        auto& workspace = WorkspaceContext(context);
        auto& ui = UiState(context);
        auto& registry = workspace.WorkspaceRegistry();
        const auto& vaults = registry.Vaults();
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
            const auto selectedVault = vaults[ui.workspaceNavigation.Selected()];
            m_workspaceOverlayOpen = false;
            OpenVault(selectedVault, context);
            return;
        }
        if (IsKeyPressed(ImGuiKey_N))
        {
            m_workspaceOverlayOpen = false;
            ShowWorkspaceSetup(context);
            SetStatus("press n to create a workspace");
            return;
        }
        if (IsKeyPressed(ImGuiKey_D) && ui.workspaceNavigation.HasSelection() && !vaults.empty())
        {
            const auto id = vaults[ui.workspaceNavigation.Selected()].id;
            if (registry.RemoveVault(id))
            {
                registry.Save(nullptr);
                ui.workspaceNavigation.SetCount(registry.Vaults().size());
                SetStatus("workspace removed from list");
                if (registry.Vaults().empty())
                {
                    m_workspaceOverlayOpen = false;
                    ShowWorkspaceSetup(context);
                    return;
                }
            }
        }
        if (IsKeyPressed(ImGuiKey_Escape))
        {
            m_workspaceOverlayOpen = false;
            return;
        }

        const ImVec2 size(std::min(760.0f, std::max(360.0f, ImGui::GetWindowWidth() * 0.58f)),
                          std::min(460.0f, std::max(240.0f, ImGui::GetWindowHeight() * 0.46f)));
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   (ImGui::GetWindowHeight() - size.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(Green.x, Green.y, Green.z, 0.30f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
        ImGui::BeginChild("WorkspaceOverlay", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ImGui::TextColored(Green, "workspaces");
        ImGui::Separator();

        if (vaults.empty())
        {
            ImGui::TextColored(Muted, "no workspaces registered");
        }
        else
        {
            const auto* activeVault = registry.ActiveVault();
            for (std::size_t i = 0; i < vaults.size(); ++i)
            {
                const bool selected = i == ui.workspaceNavigation.Selected();
                const bool active = activeVault && vaults[i].id == activeVault->id;
                const auto& vault = vaults[i];
                ImGui::TextColored(selected ? Green : Primary, "%s %s %s%s",
                                   selected ? ">" : " ", vault.emoji.c_str(), vault.title.c_str(),
                                   active ? " *" : "");
                ImGui::SameLine();
                ImGui::TextColored(Muted, "%s", vault.path.string().c_str());
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    void SlateModeBase::DrawTodoOverlay(Software::Core::Runtime::AppContext& context)
    {
        auto& workspace = WorkspaceContext(context);
        const bool allowInput = !m_todoForm.open;
        m_visibleTodos = CollectTodos(workspace);
        m_visibleTodos.erase(std::remove_if(m_visibleTodos.begin(),
                                            m_visibleTodos.end(),
                                            [this](const auto& todo) {
                                                return !MatchesTodoFilter(todo, m_todoStateFilter) ||
                                                       !TodoMatchesQuery(todo, m_todoSearchBuffer);
                                            }),
                             m_visibleTodos.end());
        m_todoNavigation.SetCount(m_visibleTodos.size());

        if (allowInput && IsKeyPressed(ImGuiKey_Tab))
        {
            m_todoStateFilter = (m_todoStateFilter + 1) % 5;
            m_todoNavigation.Reset();
        }
        if (allowInput && !ImGui::GetIO().KeyShift && IsKeyPressed(ImGuiKey_Slash))
        {
            m_focusTodoSearch = true;
        }
        if (allowInput && ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        {
            m_todoNavigation.MoveNext();
        }
        if (allowInput && ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        {
            m_todoNavigation.MovePrevious();
        }
        if (allowInput && IsKeyPressed(ImGuiKey_Escape))
        {
            m_todoOverlayOpen = false;
            if (WantsNativeEditorVisible(context))
            {
                EditorContext(context).Editor().RequestFocus();
                EditorContext(context).SetTextFocused(true);
            }
            return;
        }
        if (allowInput && (IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter)) &&
            m_todoNavigation.HasSelection() && !m_visibleTodos.empty())
        {
            BeginTodoEdit(m_visibleTodos[m_todoNavigation.Selected()]);
            return;
        }
        if (allowInput && IsKeyPressed(ImGuiKey_O) && m_todoNavigation.HasSelection() && !m_visibleTodos.empty())
        {
            const auto todo = m_visibleTodos[m_todoNavigation.Selected()];
            m_todoOverlayOpen = false;
            if (OpenDocument(todo.relativePath, context))
            {
                EditorContext(context).JumpToLine(todo.line);
            }
            return;
        }

        const ImVec2 size(std::min(760.0f, std::max(420.0f, ImGui::GetWindowWidth() * 0.60f)),
                          std::min(500.0f, std::max(280.0f, ImGui::GetWindowHeight() * 0.54f)));
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   (ImGui::GetWindowHeight() - size.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(Amber.x, Amber.y, Amber.z, 0.30f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
        ImGui::BeginChild("TodoOverlay", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        const bool overlayHovered =
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ImGui::TextColored(Amber, "todos");
        ImGui::SameLine();
        ImGui::TextColored(TodoFilterColor(m_todoStateFilter), "%s", TodoFilterLabel(m_todoStateFilter));
        ImGui::SetNextItemWidth(-1.0f);
        if (allowInput && m_focusTodoSearch)
        {
            ImGui::SetKeyboardFocusHere();
            m_focusTodoSearch = false;
        }
        InputTextString("##TodoSearch", m_todoSearchBuffer, allowInput ? 0 : ImGuiInputTextFlags_ReadOnly);
        ImGui::Separator();

        if (m_visibleTodos.empty())
        {
            ImGui::TextColored(Muted, "no todos");
        }
        else
        {
            Software::Slate::TodoState currentGroup = Software::Slate::TodoState::Open;
            bool groupStarted = false;
            for (std::size_t i = 0; i < m_visibleTodos.size(); ++i)
            {
                const auto& todo = m_visibleTodos[i];
                const ImVec4& stateColor = TodoStateColor(todo.state);
                if (m_todoStateFilter == 0 && (!groupStarted || todo.state != currentGroup))
                {
                    if (groupStarted)
                    {
                        ImGui::Dummy(ImVec2(1.0f, 6.0f));
                    }
                    currentGroup = todo.state;
                    groupStarted = true;
                    ImGui::TextColored(stateColor, "%s", Software::Slate::MarkdownService::TodoStateLabel(todo.state));
                    ImGui::Separator();
                }

                const bool selected = i == m_todoNavigation.Selected();
                ImGui::TextColored(selected ? Green : Primary, "%s %s", selected ? ">" : " ", todo.title.c_str());
                ImGui::SameLine();
                ImGui::TextColored(stateColor, "[%s]", Software::Slate::MarkdownService::TodoStateLabel(todo.state));
                ImGui::SameLine();
                ImGui::TextColored(Muted, "%s:%zu", todo.relativePath.generic_string().c_str(), todo.line);
                if (!todo.description.empty())
                {
                    ImGui::TextColored(Muted, "    %s", todo.description.c_str());
                }
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        if (allowInput && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !overlayHovered)
        {
            m_todoOverlayOpen = false;
            if (WantsNativeEditorVisible(context))
            {
                EditorContext(context).Editor().RequestFocus();
                EditorContext(context).SetTextFocused(true);
            }
            return;
        }
    }

    void SlateModeBase::DrawTodoFormOverlay(Software::Core::Runtime::AppContext& context)
    {
        const ImVec2 size(std::min(620.0f, std::max(360.0f, ImGui::GetWindowWidth() * 0.52f)),
                          std::min(220.0f, std::max(176.0f, ImGui::GetWindowHeight() * 0.24f)));
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   (ImGui::GetWindowHeight() - size.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(Amber.x, Amber.y, Amber.z, 0.30f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
        ImGui::BeginChild("TodoFormOverlay", size, true);
        const bool formHovered =
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ImGui::TextColored(Amber, "%s", m_todoForm.mode == TodoFormMode::Create ? "new todo" : "edit todo");
        ImGui::SameLine();
        ImGui::TextColored(TodoStateColor(m_todoForm.state),
                           "[%s]",
                           Software::Slate::MarkdownService::TodoStateLabel(m_todoForm.state));
        if (m_todoForm.focusTitle)
        {
            ImGui::SetKeyboardFocusHere();
            m_todoForm.focusTitle = false;
        }
        ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
        ImGui::SetNextItemWidth(-1.0f);
        const auto titleResult =
            InputTextString("##TodoTitle", m_todoForm.title, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SetNextItemWidth(-1.0f);
        const auto descriptionResult =
            InputTextString("##TodoDescription", m_todoForm.description, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemFlag();
        ImGui::Separator();
        DrawShortcutText("(tab) state   (enter) save   (esc) cancel", Amber, Primary);
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        if (IsKeyPressed(ImGuiKey_Tab))
        {
            m_todoForm.state = Software::Slate::MarkdownService::NextTodoState(m_todoForm.state);
        }
        if (IsKeyPressed(ImGuiKey_Escape))
        {
            CancelTodoForm(context);
            return;
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !formHovered)
        {
            CancelTodoForm(context);
            return;
        }
        if (titleResult.submitted || descriptionResult.submitted)
        {
            if (AcceptTodoForm(context))
            {
                return;
            }
        }
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
            if (m_searchOverlayScope == SearchOverlayScope::Document)
            {
                return "(up/down) move   (enter) jump   (shift+enter) prev   (esc) close";
            }
            return NextSearchModeHelper(m_searchMode);
        }
        if (m_workspaceOverlayOpen)
        {
            return "(up/down) choose   (enter) switch   (n) new   (d) remove   (esc) close";
        }
        if (m_todoForm.open)
        {
            return "(tab) state   (enter) save   (esc) cancel";
        }
        if (m_todoOverlayOpen)
        {
            return "(tab) state   (/) filter   (enter) edit   (o) open   (esc) close";
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

        if (m_prompt.open || m_confirm.open || m_searchOverlayOpen || m_workspaceOverlayOpen || m_todoOverlayOpen ||
            m_todoForm.open)
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

    void SlateModeBase::OpenSelectedSearchResult(Software::Core::Runtime::AppContext& context)
    {
        if (!m_searchNavigation.HasSelection() || m_visibleSearchResults.empty())
        {
            return;
        }

        const auto result = m_visibleSearchResults[m_searchNavigation.Selected()];
        CloseSearchOverlay();
        if (m_searchOverlayScope == SearchOverlayScope::Document)
        {
            auto& ui = UiState(context);
            ui.editorView = Software::Slate::SlateEditorView::Document;
            EditorContext(context).JumpToLine(result.line);
            ActivateMode(Software::Slate::ModeIds::Editor, context);
            return;
        }

        if (OpenDocument(result.relativePath, context) &&
            m_searchMode == Software::Slate::SearchMode::FullText && result.line > 0)
        {
            EditorContext(context).JumpToLine(result.line);
        }
    }

    void SlateModeBase::BeginTodoEdit(const Software::Slate::TodoTicket& ticket)
    {
        m_todoForm = {};
        m_todoForm.open = true;
        m_todoForm.focusTitle = true;
        m_todoForm.mode = TodoFormMode::Edit;
        m_todoForm.ticket = ticket;
        m_todoForm.state = ticket.state;
        m_todoForm.title = ticket.title;
        m_todoForm.description = ticket.description;
    }

    void SlateModeBase::CancelTodoForm(Software::Core::Runtime::AppContext& context)
    {
        if (m_todoForm.mode == TodoFormMode::Create && m_todoForm.replaceActiveLine)
        {
            RemovePendingTodoCommand(context);
        }

        m_todoForm = {};
        if (!m_todoOverlayOpen && WantsNativeEditorVisible(context))
        {
            EditorContext(context).Editor().RequestFocus();
            EditorContext(context).SetTextFocused(true);
        }
    }

    bool SlateModeBase::RemovePendingTodoCommand(Software::Core::Runtime::AppContext& context)
    {
        auto& workspace = WorkspaceContext(context);
        auto& documents = workspace.Documents();
        auto& editor = EditorContext(context);
        auto* active = documents.Active();
        if (!active || m_todoForm.pendingCommandLine == 0)
        {
            return false;
        }

        if (!m_todoForm.pendingCommandPath.empty() &&
            Software::Slate::PathUtils::NormalizeRelative(active->relativePath) != m_todoForm.pendingCommandPath)
        {
            return false;
        }

        const std::string lineEnding = active->lineEnding.empty()
                                           ? Software::Slate::PathUtils::DetectLineEnding(active->text)
                                           : active->lineEnding;
        auto lines = Software::Slate::MarkdownService::SplitLines(active->text);
        if (lines.empty())
        {
            return false;
        }
        const std::size_t removeIndex = std::min(m_todoForm.pendingCommandLine - 1, lines.size() - 1);
        if (Software::Slate::PathUtils::Trim(lines[removeIndex]) != "/todo")
        {
            return false;
        }

        if (lines.size() == 1)
        {
            lines.front().clear();
        }
        else
        {
            lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(removeIndex));
        }

        std::string updatedText;
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (i > 0)
            {
                updatedText += lineEnding;
            }
            updatedText += lines[i];
        }

        active->text = std::move(updatedText);
        documents.MarkEdited(m_nowSeconds);
        editor.LoadFromActiveDocument(documents);
        editor.JumpToLine(std::min(removeIndex + 1, lines.size()));
        return true;
    }

    
    bool SlateModeBase::ReplacePendingTodoCommand(Software::Core::Runtime::AppContext& context, const std::string& block)
    {
        auto& workspace = WorkspaceContext(context);
        auto& documents = workspace.Documents();
        auto& editor = EditorContext(context);
        auto* active = documents.Active();
        if (!active || m_todoForm.pendingCommandLine == 0)
        {
            return false;
        }

        if (!m_todoForm.pendingCommandPath.empty() &&
            Software::Slate::PathUtils::NormalizeRelative(active->relativePath) != m_todoForm.pendingCommandPath)
        {
            return false;
        }

        const std::string lineEnding = active->lineEnding.empty()
                                           ? Software::Slate::PathUtils::DetectLineEnding(active->text)
                                           : active->lineEnding;
        auto lines = Software::Slate::MarkdownService::SplitLines(active->text);
        if (lines.empty())
        {
            return false;
        }

        const std::size_t replaceIndex = std::min(m_todoForm.pendingCommandLine - 1, lines.size() - 1);
        if (Software::Slate::PathUtils::Trim(lines[replaceIndex]) != "/todo")
        {
            return false;
        }

        auto replacement = Software::Slate::MarkdownService::SplitLines(block);
        if (replacement.empty())
        {
            replacement.emplace_back();
        }

        lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(replaceIndex));
        lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(replaceIndex), replacement.begin(), replacement.end());

        std::string updatedText;
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (i > 0)
            {
                updatedText += lineEnding;
            }
            updatedText += lines[i];
        }

        active->text = std::move(updatedText);
        documents.MarkEdited(m_nowSeconds);
        editor.LoadFromActiveDocument(documents);
        editor.JumpToLine(replaceIndex + 1);
        return true;
    }

    bool SlateModeBase::AcceptTodoForm(Software::Core::Runtime::AppContext& context)
    {
        const auto state = m_todoForm.state;
        const std::string title = Software::Slate::PathUtils::Trim(m_todoForm.title).empty()
                                      ? "Untitled todo"
                                      : Software::Slate::PathUtils::Trim(m_todoForm.title);
        const std::string description = Software::Slate::PathUtils::Trim(m_todoForm.description);

        if (m_todoForm.mode == TodoFormMode::Create)
        {
            const std::string block = Software::Slate::MarkdownService::FormatTodoBlock(
                state, title, description, WorkspaceContext(context).Documents().Active()
                                               ? WorkspaceContext(context).Documents().Active()->lineEnding
                                               : "\n");
            bool inserted = false;
            if (m_todoForm.replaceActiveLine)
            {
                inserted = ReplacePendingTodoCommand(context, block);
            }
            else
            {
                EditorContext(context).InsertTextAtCursor(WorkspaceContext(context).Documents(), block, m_nowSeconds);
                inserted = true;
            }

            if (!inserted)
            {
                SetError("could not insert todo");
                return false;
            }

            WorkspaceContext(context).Workspace().Scan(nullptr);
            WorkspaceContext(context).Search().Rebuild(WorkspaceContext(context).Workspace());
            SetStatus("todo created");
        }
        else
        {
            if (!UpdateTodoTicket(context, m_todoForm.ticket, state, title, description))
            {
                return false;
            }
            SetStatus("todo updated");
        }

        m_todoForm = {};
        m_focusTodoSearch = false;
        if (!m_todoOverlayOpen && WantsNativeEditorVisible(context))
        {
            EditorContext(context).Editor().RequestFocus();
            EditorContext(context).SetTextFocused(true);
        }
        return true;
    }

    bool SlateModeBase::UpdateTodoTicket(Software::Core::Runtime::AppContext& context,
                                         const Software::Slate::TodoTicket& ticket,
                                         Software::Slate::TodoState state,
                                         std::string_view title,
                                         std::string_view description)
    {
        auto& workspace = WorkspaceContext(context);
        auto& documents = workspace.Documents();
        auto& editor = EditorContext(context);
        const auto normalized = Software::Slate::PathUtils::NormalizeRelative(ticket.relativePath);
        auto* active = documents.Active();
        std::string updatedText;
        std::string preservedTitle = Software::Slate::PathUtils::Trim(title);
        for (const auto& tag : ticket.tags)
        {
            if (tag.empty() || tag == "todo")
            {
                continue;
            }
            const std::string token = "#" + tag;
            if (!ContainsFilter(preservedTitle, token.c_str()))
            {
                if (!preservedTitle.empty())
                {
                    preservedTitle += " ";
                }
                preservedTitle += token;
            }
        }

        if (active && Software::Slate::PathUtils::NormalizeRelative(active->relativePath) == normalized)
        {
            if (!Software::Slate::MarkdownService::ReplaceTodoTicketBlock(active->text,
                                                                          ticket,
                                                                          state,
                                                                          preservedTitle,
                                                                          description,
                                                                          &updatedText))
            {
                SetError("could not update todo");
                return false;
            }

            active->text = updatedText;
            documents.MarkEdited(m_nowSeconds);
            editor.LoadFromActiveDocument(documents);
            editor.JumpToLine(ticket.line);
        }
        else
        {
            std::string text;
            std::string error;
            const auto absolutePath = workspace.Workspace().Resolve(normalized);
            if (!Software::Slate::DocumentService::ReadTextFile(absolutePath, &text, &error))
            {
                SetError(error);
                return false;
            }
            if (!Software::Slate::MarkdownService::ReplaceTodoTicketBlock(text,
                                                                          ticket,
                                                                          state,
                                                                          preservedTitle,
                                                                          description,
                                                                          &updatedText))
            {
                SetError("could not update todo");
                return false;
            }
            if (!Software::Slate::DocumentService::AtomicWriteText(absolutePath, updatedText, &error))
            {
                SetError(error);
                return false;
            }
        }

        workspace.Workspace().Scan(nullptr);
        workspace.Search().Rebuild(workspace.Workspace());
        return true;
    }
}


