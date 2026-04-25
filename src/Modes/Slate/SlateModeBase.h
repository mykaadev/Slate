#pragma once

#include "App/Slate/Core/SlateTypes.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/Overlays/SlateSearchOverlayController.h"
#include "App/Slate/Overlays/SlateWorkspaceSwitcherOverlay.h"
#include "App/Slate/Overlays/SlateTodoOverlayController.h"
#include "Core/Runtime/Mode.h"

#include "imgui.h"

#include <functional>
#include <string>
#include <string_view>

namespace Software::Slate
{
    class SlateEditorContext;
    class SlateWorkspaceContext;
}

namespace Software::Modes::Slate
{
    using SearchOverlayScope = Software::Slate::SearchOverlayScope;

    // Provides shared shell, shortcuts, and overlay adapters for all Slate modes.
    class SlateModeBase : public Software::Core::Runtime::Mode
    {
    protected:
        using PromptCallback = std::function<void(const std::string&, Software::Core::Runtime::AppContext&)>;
        using ConfirmCallback = std::function<void(bool, Software::Core::Runtime::AppContext&)>;

        void OnExit(Software::Core::Runtime::AppContext& context) override;
        void DrawUI(Software::Core::Runtime::AppContext& context) override final;

        virtual const char* ModeName() const = 0;
        virtual void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) = 0;
        virtual std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const = 0;
        virtual bool WantsNativeEditorVisible(const Software::Core::Runtime::AppContext& context) const;
        virtual void DrawHelp(Software::Core::Runtime::AppContext& context) const;
        virtual void DrawFooterControls(Software::Core::Runtime::AppContext& context);

        Software::Slate::SlateWorkspaceContext& WorkspaceContext(Software::Core::Runtime::AppContext& context) const;
        Software::Slate::SlateEditorContext& EditorContext(Software::Core::Runtime::AppContext& context) const;
        Software::Slate::SlateUiState& UiState(Software::Core::Runtime::AppContext& context) const;
        Software::Slate::SlateSearchOverlayController* SearchOverlay(Software::Core::Runtime::AppContext& context) const;
        Software::Slate::SlateWorkspaceSwitcherOverlay* WorkspaceSwitcherOverlay(Software::Core::Runtime::AppContext& context) const;
        Software::Slate::SlateTodoOverlayController* TodoOverlay(Software::Core::Runtime::AppContext& context) const;

        void ActivateMode(const char* modeName, Software::Core::Runtime::AppContext& context) const;

        void SetStatus(std::string message);
        void SetError(std::string message);

        void BeginCommandPrompt();
        void BeginPrompt(std::string title, std::string initialValue, PromptCallback callback);
        void BeginConfirm(std::string message, bool destructive, ConfirmCallback callback,
                          std::string acceptLabel = "yes", std::string declineLabel = {},
                          std::string cancelLabel = "cancel");
        void BeginQuitConfirm();
        void BeginSearchOverlay(bool clearQuery, Software::Core::Runtime::AppContext& context,
                                SearchOverlayScope scope = SearchOverlayScope::Workspace);
        void CloseSearchOverlay(Software::Core::Runtime::AppContext& context);

        bool SaveActiveDocument(Software::Core::Runtime::AppContext& context);
        bool OpenDocument(const Software::Slate::fs::path& relativePath, Software::Core::Runtime::AppContext& context,
                          bool recordHistory = true);
        bool NavigateDocumentHistory(Software::Core::Runtime::AppContext& context, int delta);
        bool OpenWorkspace(const Software::Slate::fs::path& root, Software::Core::Runtime::AppContext& context);
        bool OpenVault(const Software::Slate::WorkspaceVault& vault, Software::Core::Runtime::AppContext& context);
        bool OpenTodayJournal(Software::Core::Runtime::AppContext& context);

        void ShowBrowser(Software::Slate::SlateBrowserView view, Software::Core::Runtime::AppContext& context,
                         bool clearFilter = true);
        void BeginNewNoteFlow(Software::Core::Runtime::AppContext& context);
        void BeginFolderCreateFlow(Software::Core::Runtime::AppContext& context);
        void ShowWorkspaceSwitcher(Software::Core::Runtime::AppContext& context);
        void ShowWorkspaceSetup(Software::Core::Runtime::AppContext& context);
        void ShowSettings(Software::Core::Runtime::AppContext& context);
        void ShowTodos(Software::Core::Runtime::AppContext& context);
        void BeginTodoCreate(Software::Core::Runtime::AppContext& context, bool fromSlashCommand = false);
        void ShowHome(Software::Core::Runtime::AppContext& context);
        void HandleCommand(const std::string& command, Software::Core::Runtime::AppContext& context);

        Software::Slate::fs::path SelectedTreeFolderPath(const Software::Slate::SlateUiState& ui) const;

        bool IsKeyPressed(ImGuiKey key) const;
        bool IsCtrlPressed(ImGuiKey key) const;

        bool HelpOpen() const;
        bool SearchOpen() const;
        bool PromptOpen() const;
        bool ConfirmOpen() const;
        SearchOverlayScope SearchScopeValue() const;
        Software::Slate::SearchMode SearchModeValue() const;
        static bool IsTodoSlashCommand(std::string_view text);

    private:
        struct PromptState
        {
            bool open = false;
            std::string title;
            std::string buffer;
            bool focus = false;
            PromptCallback callback;
        };

        struct ConfirmState
        {
            bool open = false;
            std::string message;
            bool destructive = false;
            std::string acceptLabel = "yes";
            std::string declineLabel;
            std::string cancelLabel = "cancel";
            ConfirmCallback callback;
        };

        void DrawRootBegin(Software::Core::Runtime::AppContext& context);
        void DrawSearchOverlay(Software::Core::Runtime::AppContext& context);
        void DrawWorkspaceOverlay(Software::Core::Runtime::AppContext& context);
        void DrawTodoOverlay(Software::Core::Runtime::AppContext& context);
        void DrawPromptOverlay(Software::Core::Runtime::AppContext& context);
        void DrawConfirmOverlay(Software::Core::Runtime::AppContext& context);
        void DrawStatusLine(Software::Core::Runtime::AppContext& context);
        std::string HelperText(const Software::Core::Runtime::AppContext& context) const;
        void HandleGlobalKeys(Software::Core::Runtime::AppContext& context);
        void OpenSearchResult(const Software::Slate::SearchOverlaySelection& selection, Software::Core::Runtime::AppContext& context);
        void RestoreEditorFocusIfWanted(Software::Core::Runtime::AppContext& context);

        PromptState m_prompt;
        ConfirmState m_confirm;
        std::string m_status = "ready";
        double m_statusSeconds = 0.0;
        double m_nowSeconds = 0.0;
        bool m_statusIsError = false;
        bool m_searchOverlayOpen = false;
        bool m_workspaceOverlayOpen = false;
        bool m_helpOpen = false;
        SearchOverlayScope m_searchOverlayScope = SearchOverlayScope::Workspace;
        Software::Slate::SearchMode m_searchMode = Software::Slate::SearchMode::FileNames;
    };
}
