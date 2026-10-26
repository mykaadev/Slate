#include "Modes/Slate/SlateModeBase.h"

#include "App/Slate/Commands/SlateCommandDispatcher.h"
#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Editor/SlateEditorContext.h"
#include "App/Slate/Core/SlateEvents.h"
#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/Extensions/SlateExtensionRegistry.h"
#include "App/Slate/Extensions/SlateShellBridge.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "Core/Runtime/CommandRegistry.h"
#include "Core/Runtime/ToolRegistry.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <string_view>
#include <utility>
#include <vector>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        struct CommandMatch
        {
            Software::Core::Runtime::CommandDefinition definition;
            int score = 0;
        };

        // Lower is better; -1 means no match. Only the first word of the prompt is matched.
        int ScoreCommand(const Software::Core::Runtime::CommandDefinition& command, const std::string& query)
        {
            if (query.empty())
            {
                return 10;
            }

            const auto lower = [](std::string value) { return Software::Slate::PathUtils::ToLower(std::move(value)); };
            int best = -1;
            const auto consider = [&best](int score) {
                if (best < 0 || score < best)
                {
                    best = score;
                }
            };
            for (const auto& alias : command.aliases)
            {
                const std::string value = lower(alias);
                if (value == query) consider(0);
                else if (value.rfind(query, 0) == 0) consider(1);
                else if (value.find(query) != std::string::npos) consider(4);
            }
            const std::string id = lower(command.id);
            if (id == query) consider(0);
            else if (id.substr(id.find_last_of('.') + 1).rfind(query, 0) == 0) consider(2);
            else if (id.find(query) != std::string::npos) consider(4);
            const std::string label = lower(command.label);
            if (label.rfind(query, 0) == 0 || label.find(" " + query) != std::string::npos) consider(3);
            else if (label.find(query) != std::string::npos) consider(5);
            if (lower(command.description).find(query) != std::string::npos) consider(6);
            return best;
        }

        std::vector<CommandMatch> MatchCommands(const Software::Core::Runtime::CommandRegistry& registry,
                                                const std::string& buffer)
        {
            const std::string trimmed = Software::Slate::PathUtils::Trim(buffer);
            const std::string query = Software::Slate::PathUtils::ToLower(trimmed.substr(0, trimmed.find_first_of(" \t")));
            std::vector<CommandMatch> matches;
            for (auto& definition : registry.List())
            {
                const int score = ScoreCommand(definition, query);
                if (score >= 0)
                {
                    matches.push_back({std::move(definition), score});
                }
            }
            std::stable_sort(matches.begin(), matches.end(), [](const CommandMatch& lhs, const CommandMatch& rhs) {
                return lhs.score < rhs.score;
            });
            return matches;
        }

        std::string CommandToken(const Software::Core::Runtime::CommandDefinition& command)
        {
            return command.aliases.empty() ? command.id : command.aliases.front();
        }

        struct CommandPromptPayload
        {
            std::string* text = nullptr;
            std::string completion;
            int historyDelta = 0;
        };

        int CommandPromptCallback(ImGuiInputTextCallbackData* data)
        {
            auto* payload = static_cast<CommandPromptPayload*>(data->UserData);
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
            {
                payload->text->resize(static_cast<std::size_t>(data->BufTextLen));
                data->Buf = payload->text->data();
            }
            else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion && !payload->completion.empty())
            {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, payload->completion.c_str());
            }
            else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
            {
                payload->historyDelta = data->EventKey == ImGuiKey_UpArrow ? -1 : 1;
            }
            return 0;
        }
    }
    void SlateModeBase::OnExit(Software::Core::Runtime::AppContext& context)
    {
        EditorContext(context).ReleaseFocus();
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
        ProcessShellRequests(context);

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
            EditorContext(context).ReleaseFocus();
        }

        DrawRootBegin(context);

        const bool handleInput = !overlayOpen;
        if (m_helpOpen)
        {
            DrawHelp(context);
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

    void SlateModeBase::DrawHelp(Software::Core::Runtime::AppContext& context) const
    {
        const auto& shortcuts = WorkspaceContext(context).Shortcuts();
        ImGui::TextColored(Cyan, "shortcuts");
        ImGui::Separator();
        TextLine((shortcuts.Label(Software::Slate::ShortcutAction::NavigateUp) + "/" +
                  shortcuts.Label(Software::Slate::ShortcutAction::NavigateDown)).c_str(), "move");
        TextLine((shortcuts.Label(Software::Slate::ShortcutAction::NavigateLeft) + "/" +
                  shortcuts.Label(Software::Slate::ShortcutAction::NavigateRight)).c_str(), "tree nav");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::Accept).c_str(), "open/accept");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::Cancel).c_str(), "back/cancel");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::BrowserFilter).c_str(), "search/filter");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::CommandPalette).c_str(), "command");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::EditorSave).c_str(), "save");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::HomeJournal).c_str(), "journal");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::HomeNewNote).c_str(), "new");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::HomeFiles).c_str(), "files");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::HomeTodos).c_str(), "todos");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::HomeSettings).c_str(), "config");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::BrowserNewFolder).c_str(), "folder");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::HomeWorkspaces).c_str(), "workspaces");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::BrowserMove).c_str(), "move path");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::BrowserDelete).c_str(), "delete");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::ToggleHelp).c_str(), "help");
        TextLine(shortcuts.Label(Software::Slate::ShortcutAction::Quit).c_str(), "quit");
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
        m_prompt.commandMode = true;
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

        // Plugins may rewrite the text right before it hits disk
        auto* document = workspace.Documents().Active();
        const bool wasDirty = document && document->dirty;
        const Software::Slate::fs::path savedPath = document ? document->relativePath : Software::Slate::fs::path();
        if (wasDirty)
        {
            std::string text = document->text;
            context.events.Publish(Software::Slate::Events::DocumentWillSave{savedPath, &text});
            if (text != document->text)
            {
                editor.ReplaceAllText(workspace.Documents(), text, m_nowSeconds);
            }
        }

        std::string error;
        if (workspace.SaveDocument(&error))
        {
            SetStatus("saved");
            if (wasDirty)
            {
                context.events.Publish(Software::Slate::Events::DocumentSaved{savedPath});
            }
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
        // Leaving the previous screen released focus; a freshly opened note is ready to type into
        editor.SetTextFocused(true);
        context.events.Publish(Software::Slate::Events::DocumentOpened{workspace.Documents().Active()->relativePath});

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
        context.events.Publish(Software::Slate::Events::WorkspaceOpened{workspace.Workspace().Root()});
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
        context.events.Publish(Software::Slate::Events::WorkspaceOpened{workspace.Workspace().Root()});
        ActivateMode(Software::Slate::ModeIds::Home, context);
        return true;
    }

    bool SlateModeBase::OpenTodayJournal(Software::Core::Runtime::AppContext& context)
    {
        auto& workspaceContext = WorkspaceContext(context);
        const auto expectedDailyPath = workspaceContext.Workspace().DailyNoteRelativePath();
        const bool createdTodayNote =
            !Software::Slate::fs::exists(workspaceContext.Workspace().Resolve(expectedDailyPath));

        Software::Slate::fs::path daily;
        std::string error;
        if (!workspaceContext.OpenTodayJournal(&daily, &error))
        {
            SetError(error);
            return false;
        }

        if (!OpenDocument(daily, context))
        {
            return false;
        }

        if (createdTodayNote)
        {
            EditorContext(context).JumpToLine(4);
        }
        return true;
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
        const ImVec2 size(std::min(620.0f, std::max(340.0f, ImGui::GetWindowWidth() * 0.54f)), 104.0f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f, ImGui::GetWindowHeight() - size.y - 46.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(Cyan.x, Cyan.y, Cyan.z, 0.28f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 14.0f));
        const ImVec2 promptPos = ImGui::GetCursorPos();
        if (m_prompt.commandMode)
        {
            DrawCommandSuggestions(context, promptPos, size.x);
            ImGui::SetCursorPos(promptPos);
        }
        ImGui::BeginChild("PromptOverlay", size, true);
        ImGui::TextColored(Cyan, "%s", m_prompt.title.c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (m_prompt.focus)
        {
            ImGui::SetKeyboardFocusHere();
            m_prompt.focus = false;
        }

        bool submitted = false;
        if (m_prompt.commandMode)
        {
            const auto matches = MatchCommands(context.commands, m_prompt.buffer);
            CommandPromptPayload payload;
            payload.text = &m_prompt.buffer;
            if (!matches.empty())
            {
                const auto& selected = matches[static_cast<std::size_t>(
                    std::clamp(m_prompt.selected, 0, static_cast<int>(matches.size()) - 1))].definition;
                payload.completion = CommandToken(selected) + (selected.acceptsArguments ? " " : "");
            }
            if (m_prompt.buffer.capacity() < m_prompt.buffer.size() + 256)
            {
                m_prompt.buffer.reserve(m_prompt.buffer.size() + 256);
            }
            submitted = ImGui::InputText("##prompt", m_prompt.buffer.data(), m_prompt.buffer.capacity() + 1,
                                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize |
                                             ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory,
                                         CommandPromptCallback, &payload);
            m_prompt.buffer.resize(std::strlen(m_prompt.buffer.c_str()));
            if (payload.historyDelta != 0 && !matches.empty())
            {
                const int count = static_cast<int>(matches.size());
                m_prompt.selected = (m_prompt.selected + payload.historyDelta + count) % count;
            }

            // A typed command that resolves runs as typed; otherwise the highlighted suggestion runs
            Software::Core::Runtime::CommandRequest resolved;
            if (submitted && !context.commands.Resolve(m_prompt.buffer, &resolved) && !matches.empty())
            {
                const std::string trimmed = Software::Slate::PathUtils::Trim(m_prompt.buffer);
                const std::size_t split = trimmed.find_first_of(" \t");
                const std::string arguments = split == std::string::npos ? std::string() : trimmed.substr(split + 1);
                const auto& selected = matches[static_cast<std::size_t>(
                    std::clamp(m_prompt.selected, 0, static_cast<int>(matches.size()) - 1))].definition;
                m_prompt.buffer = selected.id + (arguments.empty() ? std::string() : " " + arguments);
            }
        }
        else
        {
            submitted = InputTextString("##prompt", m_prompt.buffer, ImGuiInputTextFlags_EnterReturnsTrue).submitted;
        }

        if (submitted)
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

        if (WorkspaceContext(context).Shortcuts().Pressed(Software::Slate::ShortcutAction::Cancel))
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

        const auto& shortcuts = WorkspaceContext(context).Shortcuts();
        std::string actions = shortcuts.Helper(Software::Slate::ShortcutAction::ConfirmAccept, m_confirm.acceptLabel);
        if (!m_confirm.declineLabel.empty())
        {
            actions += "   " + shortcuts.Helper(Software::Slate::ShortcutAction::ConfirmDecline, m_confirm.declineLabel);
        }
        actions += "   " + shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, m_confirm.cancelLabel);
        DrawShortcutTextCentered(actions);
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        if (shortcuts.Pressed(Software::Slate::ShortcutAction::ConfirmAccept) ||
            shortcuts.Pressed(Software::Slate::ShortcutAction::Accept))
        {
            auto callback = std::move(m_confirm.callback);
            m_confirm = {};
            if (callback)
            {
                callback(true, context);
            }
        }
        else if (!m_confirm.declineLabel.empty() && shortcuts.Pressed(Software::Slate::ShortcutAction::ConfirmDecline))
        {
            auto callback = std::move(m_confirm.callback);
            m_confirm = {};
            if (callback)
            {
                callback(false, context);
            }
        }
        else if (shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
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

        // Plugin status items sit on the right, separated like a terminal status bar
        if (const auto registry = context.services.Resolve<Software::Slate::SlateExtensionRegistry>();
            registry && !registry->StatusItems().empty())
        {
            std::string right;
            for (const auto& item : registry->StatusItems())
            {
                right += (right.empty() ? "" : "  \xc2\xb7  ") + item.text;
            }
            const float width = ImGui::CalcTextSize(right.c_str()).x;
            ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - width));
            ImGui::TextColored(Muted, "%s", right.c_str());
        }
    }

    void SlateModeBase::DrawCommandSuggestions(Software::Core::Runtime::AppContext& context, const ImVec2& promptPos, float width)
    {
        if (m_prompt.buffer != m_prompt.lastQuery)
        {
            m_prompt.lastQuery = m_prompt.buffer;
            m_prompt.selected = 0;
        }

        const auto matches = MatchCommands(context.commands, m_prompt.buffer);
        if (matches.empty())
        {
            return;
        }

        const int count = static_cast<int>(matches.size());
        m_prompt.selected = std::clamp(m_prompt.selected, 0, count - 1);
        const int visible = std::min(count, 8);
        const int first = std::clamp(m_prompt.selected - visible + 1, 0, count - visible);
        const float rowHeight = ImGui::GetTextLineHeightWithSpacing();
        const float height = rowHeight * static_cast<float>(visible) + 22.0f;

        // Key chords bound through plugins are shown next to their commands
        std::vector<std::pair<std::string, std::string>> chords;
        if (const auto registry = context.services.Resolve<Software::Slate::SlateExtensionRegistry>())
        {
            for (const auto& binding : registry->KeyBindings())
            {
                chords.emplace_back(binding.command, Software::Slate::ShortcutService::SerializeBinding(binding.binding));
            }
        }

        ImGui::SetCursorPos(ImVec2(promptPos.x, promptPos.y - height - 6.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(Muted.x, Muted.y, Muted.z, 0.22f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));
        ImGui::BeginChild("CommandSuggestions", ImVec2(width, height), true, ImGuiWindowFlags_NoScrollbar);
        float tokenWidth = 0.0f;
        for (int index = first; index < first + visible; ++index)
        {
            tokenWidth = std::max(tokenWidth, ImGui::CalcTextSize(CommandToken(matches[static_cast<std::size_t>(index)].definition).c_str()).x);
        }
        tokenWidth += ImGui::CalcTextSize("> ").x + 14.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        for (int index = first; index < first + visible; ++index)
        {
            const auto& command = matches[static_cast<std::size_t>(index)].definition;
            const bool selected = index == m_prompt.selected;
            const float rowY = ImGui::GetCursorPosY();
            ImGui::TextColored(selected ? Green : Muted, "%s", selected ? ">" : " ");
            ImGui::SameLine();
            ImGui::TextColored(selected ? Green : Amber, "%s", CommandToken(command).c_str());
            ImGui::SameLine(tokenWidth);
            ImGui::TextColored(selected ? Primary : ImVec4(Primary.x, Primary.y, Primary.z, 0.78f), "%s", command.label.c_str());

            std::string hint = command.category;
            for (const auto& [chordCommand, chord] : chords)
            {
                if (chordCommand == command.id || chordCommand == CommandToken(command))
                {
                    hint = chord;
                }
            }
            const float hintWidth = ImGui::CalcTextSize(hint.c_str()).x;
            ImGui::SameLine(std::max(tokenWidth + 160.0f, ImGui::GetWindowWidth() - 14.0f - hintWidth));
            ImGui::SetCursorPosY(rowY);
            ImGui::TextColored(Muted, "%s", hint.c_str());
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    void SlateModeBase::ProcessShellRequests(Software::Core::Runtime::AppContext& context)
    {
        const auto bridge = context.services.Resolve<Software::Slate::SlateShellBridge>();
        if (!bridge)
        {
            return;
        }

        // A handful per frame keeps a runaway plugin from stalling the UI
        Software::Slate::ShellRequest request;
        for (int processed = 0; processed < 16 && bridge->Pop(&request); ++processed)
        {
            switch (request.kind)
            {
            case Software::Slate::ShellRequest::Kind::Command:
                HandleCommand(request.text, context);
                break;
            case Software::Slate::ShellRequest::Kind::Status:
                SetStatus(request.text);
                break;
            case Software::Slate::ShellRequest::Kind::Error:
                SetError(request.text);
                break;
            case Software::Slate::ShellRequest::Kind::Prompt:
                if (m_prompt.open || m_confirm.open)
                {
                    bridge->Defer(std::move(request));
                    return;
                }
                BeginPrompt(request.text, request.value,
                            [onPrompt = std::move(request.onPrompt)](const std::string& value, Software::Core::Runtime::AppContext&) {
                                if (onPrompt)
                                {
                                    onPrompt(value);
                                }
                            });
                break;
            case Software::Slate::ShellRequest::Kind::Confirm:
                if (m_prompt.open || m_confirm.open)
                {
                    bridge->Defer(std::move(request));
                    return;
                }
                BeginConfirm(request.text, false,
                             [onConfirm = std::move(request.onConfirm)](bool accepted, Software::Core::Runtime::AppContext&) {
                                 if (onConfirm)
                                 {
                                     onConfirm(accepted);
                                 }
                             });
                break;
            }
        }
    }

    std::string SlateModeBase::HelperText(const Software::Core::Runtime::AppContext& context) const
    {
        const auto& shortcuts = WorkspaceContext(const_cast<Software::Core::Runtime::AppContext&>(context)).Shortcuts();
        if (m_helpOpen)
        {
            return shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "close") + "   " +
                   shortcuts.Helper(Software::Slate::ShortcutAction::ToggleHelp, "help");
        }
        if (m_searchOverlayOpen)
        {
            if (auto* search = SearchOverlay(const_cast<Software::Core::Runtime::AppContext&>(context)))
            {
                return search->HelperText(shortcuts);
            }
        }
        if (m_workspaceOverlayOpen)
        {
            if (auto* workspaces = WorkspaceSwitcherOverlay(const_cast<Software::Core::Runtime::AppContext&>(context)))
            {
                return workspaces->HelperText(shortcuts);
            }
        }
        if (auto* todos = TodoOverlay(const_cast<Software::Core::Runtime::AppContext&>(context)); todos && todos->IsAnyOpen())
        {
            return todos->HelperText(shortcuts);
        }
        if (m_prompt.open)
        {
            return shortcuts.Helper(Software::Slate::ShortcutAction::Accept, "accept") + "   " +
                   shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "cancel");
        }
        if (m_confirm.open)
        {
            std::string helper = shortcuts.Helper(Software::Slate::ShortcutAction::ConfirmAccept, m_confirm.acceptLabel);
            if (!m_confirm.declineLabel.empty())
            {
                helper += "   " + shortcuts.Helper(Software::Slate::ShortcutAction::ConfirmDecline, m_confirm.declineLabel);
            }
            helper += "   " + shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, m_confirm.cancelLabel);
            return helper;
        }

        return ModeHelperText(context);
    }

    void SlateModeBase::HandleGlobalKeys(Software::Core::Runtime::AppContext& context)
    {
        const ImGuiIO& io = ImGui::GetIO();
        auto& shortcuts = WorkspaceContext(context).Shortcuts();

        if (shortcuts.Pressed(Software::Slate::ShortcutAction::EditorSave))
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
            if (shortcuts.Pressed(Software::Slate::ShortcutAction::ToggleHelp) ||
                shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
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

        if (shortcuts.Pressed(Software::Slate::ShortcutAction::ToggleHelp))
        {
            m_helpOpen = true;
            return;
        }

        if (shortcuts.Pressed(Software::Slate::ShortcutAction::CommandPalette) ||
            (io.KeyCtrl && io.KeyShift && IsKeyPressed(ImGuiKey_P)))
        {
            BeginCommandPrompt();
            return;
        }

        if (shortcuts.Pressed(Software::Slate::ShortcutAction::HomeWorkspaces))
        {
            ShowWorkspaceSwitcher(context);
            return;
        }
        if (shortcuts.Pressed(Software::Slate::ShortcutAction::HomeTodos))
        {
            ShowTodos(context);
            return;
        }
        if (shortcuts.Pressed(Software::Slate::ShortcutAction::HomeSettings))
        {
            ShowSettings(context);
            return;
        }

        if (shortcuts.Pressed(Software::Slate::ShortcutAction::Quit))
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
            EditorContext(context).SetTextFocused(true);
        }
    }
}


