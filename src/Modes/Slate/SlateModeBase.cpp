#include "Modes/Slate/SlateModeBase.h"

#include "App/Slate/Commands/SlateCommandDispatcher.h"
#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Editor/SlateEditorContext.h"
#include "App/Slate/Core/SlateEvents.h"
#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/Extensions/SlateExtensionRegistry.h"
#include "App/Slate/Extensions/SlateShellBridge.h"
#include "App/Slate/Core/AppPaths.h"
#include "App/Slate/Editor/EditorSettingsService.h"
#include "App/Slate/Plugins/PluginManager.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateDesign.h"
#include "App/Slate/UI/SlateUi.h"
#include "Platform/PlatformWindowService.h"

#include "Core/Runtime/AppConfig.h"
#include "Core/Runtime/CommandRegistry.h"
#include "Core/Runtime/ToolRegistry.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <ctime>
#include <string_view>
#include <utility>
#include <vector>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        // The boot sequence plays once per launch, whichever mode draws first
        struct BootState
        {
            bool done = false;
            double start = -1.0;
        };
        BootState g_boot;
        constexpr double BootSeconds = 2.1;
        constexpr double BootFadeSeconds = 0.4;

        const Software::Slate::EditorSettings& AppSettings(const Software::Core::Runtime::AppContext& context)
        {
            static const Software::Slate::EditorSettings defaults = Software::Slate::EditorSettingsService::DefaultSettings();
            const auto workspace = context.services.Resolve<Software::Slate::SlateWorkspaceContext>();
            return workspace && workspace->HasWorkspaceLoaded() ? workspace->CurrentEditorSettings() : defaults;
        }

        bool AnyInputThisFrame()
        {
            for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key)
            {
                if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false))
                {
                    return true;
                }
            }
            return ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        }

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

        const auto& appSettings = AppSettings(context);
        bool booting = false;
        if (!g_boot.done)
        {
            if (!appSettings.bootSequence)
            {
                g_boot.done = true;
            }
            else
            {
                if (g_boot.start < 0.0)
                {
                    g_boot.start = m_nowSeconds;
                }
                const double age = m_nowSeconds - g_boot.start;
                // Any key skips straight to the fade
                if (age > 0.15 && age < BootSeconds - BootFadeSeconds && AnyInputThisFrame())
                {
                    g_boot.start = m_nowSeconds - (BootSeconds - BootFadeSeconds);
                }
                booting = m_nowSeconds - g_boot.start < BootSeconds;
                g_boot.done = !booting;
            }
        }

        if (!booting)
        {
            HandleGlobalKeys(context);
        }
        const bool overlayOpen = booting ||
            m_helpOpen || m_prompt.open || m_confirm.open || SearchOpen() ||
            (WorkspaceSwitcherOverlay(context) && WorkspaceSwitcherOverlay(context)->IsOpen()) ||
            (TodoOverlay(context) && TodoOverlay(context)->IsAnyOpen());
        if (auto* todos = TodoOverlay(context); todos && todos->IsFormOpen())
        {
            EditorContext(context).ReleaseFocus();
        }

        DrawRootBegin(context);

        const bool handleInput = !overlayOpen;
        const bool modalOpen = overlayOpen && !booting;
        if (m_helpOpen)
        {
            DrawHelp(context);
        }
        else
        {
            DrawMode(context, handleInput);
        }

        // Overlays sit on a soft backdrop; a child window keeps it above the screen's own panels
        if (modalOpen && !m_helpOpen)
        {
            const ImVec2 cursor = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
            ImGui::BeginChild("##OverlayBackdrop", ImGui::GetWindowSize(), ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
            const ImVec2 min = ImGui::GetWindowPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                min, ImVec2(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight()),
                ImGui::ColorConvertFloat4ToU32(ImVec4(Background.x, Background.y, Background.z, 0.62f)));
            ImGui::EndChild();
            ImGui::SetCursorPos(cursor);
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
        if (booting)
        {
            DrawBootSequence(context, m_nowSeconds - g_boot.start);
        }
        DrawScanlines(appSettings.scanlines);
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
        using Action = Software::Slate::ShortcutAction;
        struct Entry
        {
            std::string key;
            const char* label;
        };
        const auto key = [&shortcuts](Action action) { return shortcuts.Label(action); };
        const std::vector<std::pair<const char*, std::vector<Entry>>> groups{
            {"move around",
             {{key(Action::NavigateUp) + "/" + key(Action::NavigateDown), "move"},
              {key(Action::NavigateLeft) + "/" + key(Action::NavigateRight), "tree"},
              {key(Action::Accept), "open"},
              {key(Action::Cancel), "back"},
              {key(Action::BrowserFilter), "search"},
              {key(Action::CommandPalette), "commands"},
              {"ctrl+shift+p", "commands"},
              {"alt+left/right", "history"}}},
            {"notes",
             {{key(Action::HomeJournal), "journal"},
              {key(Action::HomeNewNote), "new note"},
              {key(Action::HomeFiles), "files"},
              {key(Action::HomeTodos), "todos"},
              {key(Action::HomeWorkspaces), "workspaces"},
              {key(Action::BrowserNewFolder), "new folder"},
              {key(Action::BrowserMove), "move"},
              {key(Action::BrowserDelete), "delete"}}},
            {"writing",
             {{key(Action::EditorSave), "save"},
              {key(Action::EditorPreview), "reading view"},
              {key(Action::EditorOutline), "outline"},
              {key(Action::EditorSidebar), "sidebar"},
              {key(Action::EditorFind), "find"},
              {"ctrl+b/i/e", "bold, italic, code"},
              {"ctrl+k", "link"},
              {"alt+up/down", "move lines"},
              {"/", "insert blocks"}}},
            {"slate",
             {{key(Action::HomeSettings), "config"},
              {key(Action::ToggleHelp), "this help"},
              {key(Action::Quit), "quit"},
              {"plugins", ":plugins"},
              {"init.lua", ":init"}}},
        };

        const float width = std::min(1080.0f, ImGui::GetWindowWidth() - 120.0f);
        const float height = std::min(520.0f, ImGui::GetWindowHeight() - Tokens::FooterHeight - 80.0f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - width) * 0.5f,
                                   std::max(40.0f, (ImGui::GetWindowHeight() - Tokens::FooterHeight - height) * 0.45f)));
        BeginCard("HelpCard", ImVec2(width, height), Cyan, Tokens::SpaceXL);
        CardTitle("keyboard", Cyan, "every key is rebindable in config");
        if (ImGui::BeginTable("HelpColumns", static_cast<int>(groups.size()), ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextRow();
            for (const auto& [title, entries] : groups)
            {
                ImGui::TableNextColumn();
                SectionLabel(title, Amber);
                ImGui::Dummy(ImVec2(1.0f, 4.0f));
                for (const auto& entry : entries)
                {
                    KeyHint(entry.key, entry.label, Amber);
                    ImGui::Dummy(ImVec2(1.0f, 2.0f));
                }
            }
            ImGui::EndTable();
        }
        EndCard();
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
        handlers.showSettings = [this, &context](const std::string& section) {
            UiState(context).settingsSection = section;
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
        handlers.showHelp = [this]() {
            m_helpOpen = true;
        };
        handlers.showPalette = [this]() {
            BeginCommandPrompt();
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
        ApplyImGuiTheme();
        const auto& appSettings = AppSettings(context);
        ImGui::GetStyle().FontScaleMain = static_cast<float>(appSettings.interfaceScale) / 100.0f;
        SetCursorBlink(appSettings.cursorBlink);
        if (const auto window = context.services.Resolve<Software::Platform::PlatformWindowService>())
        {
            window->RequestFontFamily(appSettings.fontFamily);
        }
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Background);
        ImGui::PushStyleColor(ImGuiCol_Text, Primary);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Surface(3));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Mix(Surface(3), Cyan, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Mix(Surface(3), Muted, 0.1f));
        ImGui::PushStyleColor(ImGuiCol_Separator, Border());
        ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, Alpha(Amber, 0.42f));
        ImGui::PushStyleColor(ImGuiCol_SeparatorActive, Alpha(Green, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
        PushSlateScrollbarStyle(scrollbarStyle);
        ImGui::Begin("SlateRoot", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
        SyncWindowChrome(context);
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
        const ImVec2 size(std::min(640.0f, std::max(360.0f, ImGui::GetWindowWidth() * 0.5f)), 96.0f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f, ImGui::GetWindowHeight() - size.y - Tokens::FooterHeight - 18.0f));
        const ImVec2 promptPos = ImGui::GetCursorPos();
        if (m_prompt.commandMode)
        {
            DrawCommandSuggestions(context, promptPos, size.x);
            ImGui::SetCursorPos(promptPos);
        }
        BeginCard("PromptOverlay", size, Cyan, Tokens::SpaceL);
        CardTitle(m_prompt.title.c_str(), Cyan, m_prompt.commandMode ? "tab completes" : nullptr);
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
            submitted = ImGui::InputTextWithHint("##prompt", "type a command", m_prompt.buffer.data(), m_prompt.buffer.capacity() + 1,
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
        EndCard();

        if (WorkspaceContext(context).Shortcuts().Pressed(Software::Slate::ShortcutAction::Cancel))
        {
            m_prompt = {};
        }
    }

    void SlateModeBase::DrawConfirmOverlay(Software::Core::Runtime::AppContext& context)
    {
        const auto& shortcuts = WorkspaceContext(context).Shortcuts();
        std::string actions = shortcuts.Helper(Software::Slate::ShortcutAction::ConfirmAccept, m_confirm.acceptLabel);
        if (!m_confirm.declineLabel.empty())
        {
            actions += "   " + shortcuts.Helper(Software::Slate::ShortcutAction::ConfirmDecline, m_confirm.declineLabel);
        }
        actions += "   " + shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, m_confirm.cancelLabel);

        const ImVec4 accent = m_confirm.destructive ? Red : Amber;
        const float width = std::clamp(std::max(ShortcutTextWidth(actions), ImGui::CalcTextSize(m_confirm.message.c_str()).x) + 80.0f,
                                       340.0f, 560.0f);
        const ImVec2 size(width, 132.0f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f, (ImGui::GetWindowHeight() - size.y) * 0.42f));
        BeginCard("ConfirmOverlay", size, accent, Tokens::SpaceXL);
        CardTitle(m_confirm.destructive ? "careful" : "confirm", accent);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(Primary, "%s", m_confirm.message.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(1.0f, 8.0f));
        DrawShortcutText(actions, accent);
        EndCard();

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

        const ImVec2 windowPos = ImGui::GetWindowPos();
        const float windowWidth = ImGui::GetWindowWidth();
        const float windowHeight = ImGui::GetWindowHeight();
        const float padding = ImGui::GetStyle().WindowPadding.x;
        auto* drawList = ImGui::GetWindowDrawList();

        // Key hints for whatever has focus
        ImGui::SetCursorPos(ImVec2(padding, windowHeight - Tokens::FooterHeight + 4.0f));
        DrawShortcutText(HelperText(context));

        // Status bar: logo, mode, workspace, message ... plugin items, clock
        const ImVec2 barMin(windowPos.x, windowPos.y + windowHeight - Tokens::StatusBarHeight);
        const ImVec2 barMax(windowPos.x + windowWidth, windowPos.y + windowHeight);
        drawList->AddRectFilled(barMin, barMax, U32(Surface(1)));
        drawList->AddLine(barMin, ImVec2(barMax.x, barMin.y), U32(Border(0.9f)), 1.0f);
        const float textY = barMin.y + (Tokens::StatusBarHeight - ImGui::GetTextLineHeight()) * 0.5f;
        const float barTop = barMin.y + 1.0f;
        const float barHeight = Tokens::StatusBarHeight - 1.0f;
        float x = barMin.x;

        const std::string badge = StatusBadge(context);
        ImVec4 badgeColor = Cyan;
        if (badge == "NOTE" || badge == "EDIT")
        {
            badgeColor = Green;
        }
        else if (badge == "READ")
        {
            badgeColor = MarkdownLink;
        }
        else if (badge == "FILES")
        {
            badgeColor = Amber;
        }
        else if (badge == "CONFIG")
        {
            badgeColor = Mix(Muted, Primary, 0.4f);
        }
        else if (badge != "HOME" && badge != "SETUP")
        {
            badgeColor = MarkdownImage;
        }
        // Powerline segments: mode, then workspace, then the open note as plain text
        const float arrow = barHeight * 0.42f;
        const std::string modeText = "    " + badge;
        const float modeStart = x;
        auto drawMode = [&]() {
            PowerlineSegment(drawList, modeStart, barTop, barHeight, modeText, badgeColor, Background);
            DrawLogoMark(drawList, ImVec2(modeStart + 17.0f, barTop + barHeight * 0.5f), 14.0f);
        };
        x = modeStart + PowerlineWidth(modeText, barHeight);

        const auto& workspace = WorkspaceContext(context);
        if (!workspace.HasWorkspaceLoaded())
        {
            drawMode();
            x += 8.0f;
        }
        else
        {
            std::string name = workspace.Workspace().Root().filename().u8string();
            if (const auto* vault = workspace.WorkspaceRegistry().ActiveVault())
            {
                name = vault->emoji + " " + vault->title;
            }
            // The workspace segment tucks under the mode arrow
            x = PowerlineSegment(drawList, x - arrow, barTop, barHeight, "  " + name, Mix(Surface(3), badgeColor, 0.12f), Primary) + 8.0f;
            drawMode();
            if (const auto* document = workspace.Documents().Active(); document && badge != "HOME")
            {
                const std::string path = document->relativePath.generic_u8string();
                drawList->AddText(ImVec2(x, textY), U32(TextSecondary()), path.c_str());
                x += ImGui::CalcTextSize(path.c_str()).x;
                if (document->dirty)
                {
                    drawList->AddCircleFilled(ImVec2(x + 9.0f, textY + ImGui::GetTextLineHeight() * 0.55f), 3.5f, U32(Amber), 12);
                    x += 14.0f;
                }
            }
        }

        ImVec4 statusColor = m_statusIsError ? Red : Muted;
        if (!m_statusIsError)
        {
            const float age = static_cast<float>(std::max(0.0, m_nowSeconds - m_statusSeconds));
            statusColor.w = std::clamp(1.0f - age / 5.0f, 0.0f, 1.0f);
        }

        // Right side: plugin items, then the clock segment at the edge
        std::string clockText;
        if (AppSettings(context).statusClock)
        {
            char clock[16] = {};
            const std::time_t now = std::time(nullptr);
            std::tm local{};
#if defined(_WIN32)
            localtime_s(&local, &now);
#else
            localtime_r(&now, &local);
#endif
            std::strftime(clock, sizeof(clock), "%H:%M", &local);
            clockText = clock;
        }
        std::string items;
        if (const auto registry = context.services.Resolve<Software::Slate::SlateExtensionRegistry>())
        {
            for (const auto& item : registry->StatusItems())
            {
                items += (items.empty() ? "" : "  \xc2\xb7  ") + item.text;
            }
        }
        const float clockStart = clockText.empty() ? barMax.x : barMax.x - PowerlineWidth(clockText, barHeight);
        float rightX = clockStart;
        if (!items.empty())
        {
            rightX = PowerlineSegment(drawList, clockText.empty() ? barMax.x : clockStart + arrow, barTop, barHeight, items, Surface(3),
                                      TextSecondary(), false);
        }
        if (!clockText.empty())
        {
            PowerlineSegment(drawList, barMax.x, barTop, barHeight, clockText, Mix(Surface(3), Cyan, 0.22f), Primary, false);
        }

        if (statusColor.w > 0.01f && !m_status.empty())
        {
            const float messageX = x + 18.0f;
            drawList->PushClipRect(ImVec2(messageX, barMin.y), ImVec2(rightX - 18.0f, barMax.y), true);
            drawList->AddText(ImVec2(messageX, textY), U32(statusColor), m_status.c_str());
            drawList->PopClipRect();
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
        const int visible = std::min(count, 9);
        const int first = std::clamp(m_prompt.selected - visible + 1, 0, count - visible);
        const float rowHeight = ImGui::GetTextLineHeight() + 10.0f;
        const float height = rowHeight * static_cast<float>(visible) + Tokens::SpaceM * 2.0f;

        // Key chords bound through plugins are shown next to their commands
        std::vector<std::pair<std::string, std::string>> chords;
        if (const auto registry = context.services.Resolve<Software::Slate::SlateExtensionRegistry>())
        {
            for (const auto& binding : registry->KeyBindings())
            {
                chords.emplace_back(binding.command, Software::Slate::ShortcutService::SerializeBinding(binding.binding));
            }
        }

        ImGui::SetCursorPos(ImVec2(promptPos.x, promptPos.y - height - 8.0f));
        BeginCard("CommandSuggestions", ImVec2(width, height), Muted, Tokens::SpaceM);
        float tokenWidth = 0.0f;
        for (int index = first; index < first + visible; ++index)
        {
            tokenWidth = std::max(tokenWidth, ImGui::CalcTextSize(CommandToken(matches[static_cast<std::size_t>(index)].definition).c_str()).x);
        }
        tokenWidth += 34.0f;
        auto* drawList = ImGui::GetWindowDrawList();
        for (int index = first; index < first + visible; ++index)
        {
            const auto& command = matches[static_cast<std::size_t>(index)].definition;
            const bool selected = index == m_prompt.selected;
            ImGui::PushID(index);
            ImVec2 rowMin;
            ImVec2 rowMax;
            if (ListRow("##command", selected, rowHeight, Green, &rowMin, &rowMax))
            {
                m_prompt.selected = index;
                m_prompt.buffer = CommandToken(command);
            }
            ImGui::PopID();
            const float textY = rowMin.y + 5.0f;
            drawList->AddText(ImVec2(rowMin.x + 12.0f, textY), U32(selected ? Green : Amber), CommandToken(command).c_str());
            drawList->AddText(ImVec2(rowMin.x + tokenWidth, textY), U32(selected ? Primary : Alpha(Primary, 0.75f)), command.label.c_str());

            bool hasChord = false;
            for (const auto& [chordCommand, chord] : chords)
            {
                if (chordCommand == command.id || chordCommand == CommandToken(command))
                {
                    DrawKeycap(drawList, ImVec2(rowMax.x - KeycapWidth(chord) - 10.0f, rowMin.y + (rowHeight - KeycapHeight()) * 0.5f), chord, Amber);
                    hasChord = true;
                }
            }
            if (!hasChord)
            {
                const float hintWidth = ImGui::CalcTextSize(command.category.c_str()).x;
                drawList->AddText(ImVec2(rowMax.x - hintWidth - 10.0f, textY), U32(TextFaint()), command.category.c_str());
            }
        }
        EndCard();
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

    std::string SlateModeBase::StatusBadge(const Software::Core::Runtime::AppContext&) const
    {
        const std::string_view mode = ModeName();
        if (mode == Software::Slate::ModeIds::Home) return "HOME";
        if (mode == Software::Slate::ModeIds::Editor) return "NOTE";
        if (mode == Software::Slate::ModeIds::Browser) return "FILES";
        if (mode == Software::Slate::ModeIds::Settings) return "CONFIG";
        if (mode == Software::Slate::ModeIds::Workspace) return "SETUP";
        return "VIEW";
    }

    void SlateModeBase::DrawBootSequence(Software::Core::Runtime::AppContext& context, double age)
    {
        const float fade = static_cast<float>(std::clamp((BootSeconds - age) / BootFadeSeconds, 0.0, 1.0));
        const ImVec2 cursor = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        ImGui::BeginChild("##Boot", ImGui::GetWindowSize(), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        auto* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        drawList->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), U32(Alpha(Background, fade)));

        // Boot log lines, each with a status tag, revealed one after another
        const auto& workspace = WorkspaceContext(context);
        std::vector<std::pair<std::string, std::string>> lines;
        lines.emplace_back("config", FriendlyPath(Software::Slate::AppPaths::ConfigDirectory().generic_u8string()));
        if (workspace.HasWorkspaceLoaded())
        {
            std::string name = workspace.Workspace().Root().filename().u8string();
            if (const auto* vault = workspace.WorkspaceRegistry().ActiveVault())
            {
                name = vault->emoji + " " + vault->title;
            }
            lines.emplace_back("workspace", name);
            lines.emplace_back("index", std::to_string(workspace.Workspace().MarkdownFiles().size()) + " notes");
        }
        else
        {
            lines.emplace_back("workspace", "none yet");
        }
        if (const auto plugins = context.services.Resolve<Software::Slate::PluginManager>())
        {
            int loaded = 0;
            for (const auto& plugin : plugins->Plugins())
            {
                loaded += plugin.status == Software::Slate::PluginStatus::Loaded ? 1 : 0;
            }
            lines.emplace_back("plugins", std::to_string(loaded) + " loaded");
        }
        lines.emplace_back("keys", std::to_string(workspace.Shortcuts().Definitions().size()) + " bindings");

        const float cell = std::clamp(size.x / 70.0f, 8.0f, 14.0f);
        const ImVec2 logoSize = BlockLogoSize(cell);
        const float lineHeight = ImGui::GetTextLineHeightWithSpacing() + 2.0f;
        const float blockHeight = logoSize.y + 36.0f + lineHeight * static_cast<float>(lines.size() + 2);
        const float top = min.y + std::max(24.0f, (size.y - blockHeight) * 0.42f);
        const ImVec2 logoPos(min.x + (size.x - logoSize.x) * 0.5f, top);

        // The logo types itself in from left to right
        const float reveal = static_cast<float>(std::clamp(age / 0.45, 0.0, 1.0));
        drawList->PushClipRect(logoPos, ImVec2(logoPos.x + logoSize.x * reveal + 1.0f, logoPos.y + logoSize.y), true);
        DrawBlockLogo(drawList, logoPos, cell, m_nowSeconds, fade);
        drawList->PopClipRect();

        const float columnWidth = std::min(size.x - 48.0f, 460.0f);
        const float left = min.x + (size.x - columnWidth) * 0.5f;
        float y = top + logoSize.y + 28.0f;
        const std::string version = std::string("slate ") + SOFTWARE_APP_VERSION + "  \xc2\xb7  plain-text notes, todos and journal";
        drawList->AddText(ImVec2(left, y), U32(Alpha(TextSecondary(), fade)), version.c_str());
        y += lineHeight + 6.0f;

        const float tagWidth = ImGui::CalcTextSize("[ ok ] ").x;
        const float labelWidth = ImGui::CalcTextSize("workspace  ").x;
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const double shownAt = 0.4 + 0.16 * static_cast<double>(i);
            if (age < shownAt)
            {
                break;
            }
            drawList->AddText(ImVec2(left, y), U32(Alpha(TextFaint(), fade)), "[    ]");
            drawList->AddText(ImVec2(left + ImGui::CalcTextSize("[ ").x, y), U32(Alpha(Green, fade)), "ok");
            drawList->AddText(ImVec2(left + tagWidth, y), U32(Alpha(Primary, fade)), lines[i].first.c_str());
            const std::string detail = Ellipsize(lines[i].second, columnWidth - tagWidth - labelWidth);
            drawList->AddText(ImVec2(left + tagWidth + labelWidth, y), U32(Alpha(TextSecondary(), fade)), detail.c_str());
            y += lineHeight;
        }
        if (age >= 0.4 + 0.16 * static_cast<double>(lines.size()))
        {
            y += 6.0f;
            drawList->AddText(ImVec2(left, y), U32(Alpha(Cyan, fade)), "ready");
            if (CursorVisible(m_nowSeconds))
            {
                const float x = left + ImGui::CalcTextSize("ready ").x;
                drawList->AddRectFilled(ImVec2(x, y + 2.0f), ImVec2(x + ImGui::CalcTextSize("_").x, y + ImGui::GetTextLineHeight()),
                                        U32(Alpha(Amber, fade)));
            }
        }
        if (fade > 0.6f)
        {
            const char* skip = "any key skips";
            drawList->AddText(ImVec2(min.x + (size.x - ImGui::CalcTextSize(skip).x) * 0.5f, min.y + size.y - 40.0f),
                              U32(Alpha(TextFaint(), fade * 0.8f)), skip);
        }
        ImGui::EndChild();
        ImGui::SetCursorPos(cursor);
    }

    void SlateModeBase::SyncWindowChrome(Software::Core::Runtime::AppContext& context)
    {
        // The native title bar follows the palette so the window reads as one surface
        const ImVec4 caption = Background;
        if (caption.x == m_chromeColor.x && caption.y == m_chromeColor.y && caption.z == m_chromeColor.z)
        {
            return;
        }
        m_chromeColor = caption;
        if (const auto window = context.services.Resolve<Software::Platform::PlatformWindowService>())
        {
            window->SetChromeColors(caption.x, caption.y, caption.z, Primary.x, Primary.y, Primary.z);
        }
    }
}
