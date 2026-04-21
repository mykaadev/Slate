#include "Modes/Slate/SlateModeBase.h"

#include "App/Slate/PathUtils.h"
#include "App/Slate/SlateEditorContext.h"
#include "App/Slate/SlateModeIds.h"
#include "App/Slate/SlateUiState.h"
#include "App/Slate/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "Core/Runtime/ToolRegistry.h"
#include "imgui.h"

#include <algorithm>
#include <utility>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    void SlateModeBase::OnExit(Software::Core::Runtime::AppContext& context)
    {
        (void)context;
        m_prompt = {};
        m_confirm = {};
        m_searchOverlayOpen = false;
        m_searchNavigation.Reset();
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

        DrawRootBegin();

        const bool handleInput = !m_helpOpen && !m_prompt.open && !m_confirm.open && !m_searchOverlayOpen;
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
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(5);
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
        TextLine("l", "library");
        TextLine("t", "theme");
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
            SetStatus("saved");
            return true;
        }

        SetError(error);
        return false;
    }

    bool SlateModeBase::OpenDocument(const Software::Slate::fs::path& relativePath,
                                     Software::Core::Runtime::AppContext& context)
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
        auto& ui = UiState(context);
        ui.workspaceView = Software::Slate::SlateWorkspaceView::Switcher;
        ui.workspaceNavigation.SetCount(WorkspaceContext(context).WorkspaceRegistry().Vaults().size());
        ui.workspaceNavigation.Reset();
        ActivateMode(Software::Slate::ModeIds::Workspace, context);
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
        else if (trimmed == "t" || trimmed == "theme" || trimmed == "settings")
        {
            ShowSettings(context);
        }
        else if (trimmed == "library" || trimmed == "l" || trimmed == "docs")
        {
            ShowBrowser(Software::Slate::SlateBrowserView::Library, context);
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

    void SlateModeBase::DrawRootBegin()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Background);
        ImGui::PushStyleColor(ImGuiCol_Text, Primary);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Panel);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Panel);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
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
            m_visibleSearchResults = workspace.Search().Query(m_searchBuffer, m_searchMode, 100);
        }
        m_searchNavigation.SetCount(m_visibleSearchResults.size());

        if (m_searchOverlayScope == SearchOverlayScope::Workspace && IsKeyPressed(ImGuiKey_Tab))
        {
            m_searchMode = m_searchMode == Software::Slate::SearchMode::FileNames
                               ? Software::Slate::SearchMode::FullText
                               : Software::Slate::SearchMode::FileNames;
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

        const ImVec2 size(ImGui::GetWindowWidth() * 0.72f, ImGui::GetWindowHeight() * 0.58f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   (ImGui::GetWindowHeight() - size.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::BeginChild("SearchOverlay", size, true);
        const bool documentFind = m_searchOverlayScope == SearchOverlayScope::Document;
        ImGui::TextColored(Cyan, "%s", documentFind ? "find in file" : "search");
        if (!documentFind)
        {
            ImGui::SameLine();
            ImGui::TextColored(Muted, "%s",
                               m_searchMode == Software::Slate::SearchMode::FileNames ? "files" : "full text");
        }
        ImGui::SetNextItemWidth(-1.0f);
        if (m_focusSearch)
        {
            ImGui::SetKeyboardFocusHere();
            m_focusSearch = false;
        }
        InputTextString("##SearchQuery", m_searchBuffer, 0);
        ImGui::Separator();

        if (m_searchBuffer.empty())
        {
            ImGui::TextColored(Muted, "%s", documentFind ? "type to find in this file"
                                                         : "type to search filenames. Tab toggles full text");
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
        ImGui::PopStyleColor();
    }

    void SlateModeBase::DrawPromptOverlay(Software::Core::Runtime::AppContext& context)
    {
        const ImVec2 size(ImGui::GetWindowWidth() * 0.72f, 92.0f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f, ImGui::GetWindowHeight() - 150.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
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
        ImGui::PopStyleColor();

        if (IsKeyPressed(ImGuiKey_Escape))
        {
            m_prompt = {};
        }
    }

    void SlateModeBase::DrawConfirmOverlay(Software::Core::Runtime::AppContext& context)
    {
        const ImVec2 size(ImGui::GetWindowWidth() * 0.62f, 118.0f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   (ImGui::GetWindowHeight() - size.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
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
        ImGui::PopStyleColor();

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
            return m_searchMode == Software::Slate::SearchMode::FileNames
                       ? "(tab) full text   (up/down) move   (enter) open   (esc) close"
                       : "(tab) files   (up/down) move   (enter) open   (esc) close";
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
        if (IsCtrlPressed(ImGuiKey_S))
        {
            SaveActiveDocument(context);
        }

        if (m_prompt.open || m_confirm.open || m_searchOverlayOpen)
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

        const ImGuiIO& io = ImGui::GetIO();
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
}


