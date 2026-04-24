#pragma once

#include "App/Slate/Core/NavigationController.h"
#include "App/Slate/Core/SlateTypes.h"
#include "App/Slate/State/SlateUiState.h"
#include "Core/Runtime/Mode.h"

#include "imgui.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    class SlateEditorContext;
    class SlateWorkspaceContext;
}

namespace Software::Modes::Slate
{
    // Splits search overlay behavior between workspace and document search
    enum class SearchOverlayScope
    {
        Workspace,
        Document
    };

    // Provides shared overlays shortcuts and helpers for all Slate modes
    class SlateModeBase : public Software::Core::Runtime::Mode
    {
    protected:
        // Handles prompt acceptance
        using PromptCallback = std::function<void(const std::string&, Software::Core::Runtime::AppContext&)>;
        // Handles confirm acceptance or decline
        using ConfirmCallback = std::function<void(bool, Software::Core::Runtime::AppContext&)>;

        // Resets transient overlay state when the mode exits
        void OnExit(Software::Core::Runtime::AppContext& context) override;
        // Draws the mode plus shared overlays
        void DrawUI(Software::Core::Runtime::AppContext& context) override final;

        // Returns the mode id string
        virtual const char* ModeName() const = 0;
        // Draws the mode specific content
        virtual void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) = 0;
        // Returns the mode helper text when no overlay is open
        virtual std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const = 0;
        // Reports whether the native editor should be visible for this mode
        virtual bool WantsNativeEditorVisible(const Software::Core::Runtime::AppContext& context) const;
        // Draws the help overlay body
        virtual void DrawHelp() const;
        // Draws extra footer content for a mode
        virtual void DrawFooterControls(Software::Core::Runtime::AppContext& context);

        // Returns the shared workspace context
        Software::Slate::SlateWorkspaceContext& WorkspaceContext(Software::Core::Runtime::AppContext& context) const;
        // Returns the shared editor context
        Software::Slate::SlateEditorContext& EditorContext(Software::Core::Runtime::AppContext& context) const;
        // Returns the shared UI state
        Software::Slate::SlateUiState& UiState(Software::Core::Runtime::AppContext& context) const;

        // Activates another mode by id
        void ActivateMode(const char* modeName, Software::Core::Runtime::AppContext& context) const;

        // Sets a non error status message
        void SetStatus(std::string message);
        // Sets an error status message
        void SetError(std::string message);

        // Opens the command prompt
        void BeginCommandPrompt();
        // Opens a generic text prompt
        void BeginPrompt(std::string title, std::string initialValue, PromptCallback callback);
        // Opens a generic confirm dialog
        void BeginConfirm(std::string message, bool destructive, ConfirmCallback callback,
                          std::string acceptLabel = "yes", std::string declineLabel = {},
                          std::string cancelLabel = "cancel");
        // Opens the quit confirm dialog
        void BeginQuitConfirm();
        // Opens workspace or document search
        void BeginSearchOverlay(bool clearQuery, Software::Core::Runtime::AppContext& context,
                                SearchOverlayScope scope = SearchOverlayScope::Workspace);
        // Closes the search overlay
        void CloseSearchOverlay();

        // Saves the active document if one is open
        bool SaveActiveDocument(Software::Core::Runtime::AppContext& context);
        // Opens a document and updates history
        bool OpenDocument(const Software::Slate::fs::path& relativePath, Software::Core::Runtime::AppContext& context,
                          bool recordHistory = true);
        // Moves backward or forward through document history
        bool NavigateDocumentHistory(Software::Core::Runtime::AppContext& context, int delta);
        // Opens a workspace root
        bool OpenWorkspace(const Software::Slate::fs::path& root, Software::Core::Runtime::AppContext& context);
        // Opens a vault entry
        bool OpenVault(const Software::Slate::WorkspaceVault& vault, Software::Core::Runtime::AppContext& context);
        // Opens the daily journal
        bool OpenTodayJournal(Software::Core::Runtime::AppContext& context);

        // Switches to one browser view
        void ShowBrowser(Software::Slate::SlateBrowserView view, Software::Core::Runtime::AppContext& context,
                         bool clearFilter = true);
        // Starts the new note flow
        void BeginNewNoteFlow(Software::Core::Runtime::AppContext& context);
        // Starts the new folder flow
        void BeginFolderCreateFlow(Software::Core::Runtime::AppContext& context);
        // Opens the workspace switcher overlay
        void ShowWorkspaceSwitcher(Software::Core::Runtime::AppContext& context);
        // Opens the workspace setup flow
        void ShowWorkspaceSetup(Software::Core::Runtime::AppContext& context);
        // Opens the settings mode
        void ShowSettings(Software::Core::Runtime::AppContext& context);
        // Opens the todo overlay
        void ShowTodos(Software::Core::Runtime::AppContext& context);
        // Starts todo creation from the editor
        void BeginTodoCreate(Software::Core::Runtime::AppContext& context, bool fromSlashCommand = false);
        // Returns to the home mode
        void ShowHome(Software::Core::Runtime::AppContext& context);
        // Handles command prompt input
        void HandleCommand(const std::string& command, Software::Core::Runtime::AppContext& context);

        // Picks the selected folder path for create and move flows
        Software::Slate::fs::path SelectedTreeFolderPath(const Software::Slate::SlateUiState& ui) const;

        // Tests whether one key was pressed this frame
        bool IsKeyPressed(ImGuiKey key) const;
        // Tests whether control plus key was pressed this frame
        bool IsCtrlPressed(ImGuiKey key) const;

        // Reports whether help is open
        bool HelpOpen() const;
        // Reports whether search is open
        bool SearchOpen() const;
        // Reports whether a prompt is open
        bool PromptOpen() const;
        // Reports whether a confirm dialog is open
        bool ConfirmOpen() const;
        // Returns the current search scope
        SearchOverlayScope SearchScopeValue() const;
        // Returns the current workspace search mode
        Software::Slate::SearchMode SearchModeValue() const;

    private:
        // Stores the generic prompt state
        struct PromptState
        {
            // Marks whether the prompt is open
            bool open = false;
            // Stores the prompt title
            std::string title;
            // Stores the editable prompt text
            std::string buffer;
            // Requests focus for the prompt input
            bool focus = false;
            // Stores the accept callback
            PromptCallback callback;
        };

        // Stores the generic confirm state
        struct ConfirmState
        {
            // Marks whether the confirm is open
            bool open = false;
            // Stores the message shown to the user
            std::string message;
            // Marks destructive confirms
            bool destructive = false;
            // Stores the accept label
            std::string acceptLabel = "yes";
            // Stores the decline label when present
            std::string declineLabel;
            // Stores the cancel label
            std::string cancelLabel = "cancel";
            // Stores the confirm callback
            ConfirmCallback callback;
        };

        // Tracks whether the todo form is creating or editing
        enum class TodoFormMode
        {
            Create,
            Edit
        };

        // Tracks which todo form text field should regain keyboard focus
        enum class TodoFormFocusField
        {
            Title,
            Description
        };

        // Stores the todo form state
        struct TodoFormState
        {
            // Marks whether the form is open
            bool open = false;
            // Requests focus for the last active text input
            bool requestTextFocus = false;
            // Stores the text field that should regain keyboard focus
            TodoFormFocusField focusField = TodoFormFocusField::Title;
            // Marks whether the current editor line should be replaced
            bool replaceActiveLine = false;
            // Stores the source document for a pending slash command
            Software::Slate::fs::path pendingCommandPath;
            // Stores the 1-based source line for a pending slash command
            std::size_t pendingCommandLine = 0;
            // Stores whether the form is creating or editing
            TodoFormMode mode = TodoFormMode::Create;
            // Stores the ticket being edited
            Software::Slate::TodoTicket ticket;
            // Stores the selected todo state
            Software::Slate::TodoState state = Software::Slate::TodoState::Open;
            // Stores the todo title text
            std::string title;
            // Stores the todo description text
            std::string description;
        };

        // Draws the shared root window shell
        void DrawRootBegin(Software::Core::Runtime::AppContext& context);
        // Draws the search overlay
        void DrawSearchOverlay(Software::Core::Runtime::AppContext& context);
        // Draws the workspace overlay
        void DrawWorkspaceOverlay(Software::Core::Runtime::AppContext& context);
        // Draws the todo overlay
        void DrawTodoOverlay(Software::Core::Runtime::AppContext& context);
        // Draws the todo edit form
        void DrawTodoFormOverlay(Software::Core::Runtime::AppContext& context);
        // Draws the prompt overlay
        void DrawPromptOverlay(Software::Core::Runtime::AppContext& context);
        // Draws the confirm overlay
        void DrawConfirmOverlay(Software::Core::Runtime::AppContext& context);
        // Draws the footer and status line
        void DrawStatusLine(Software::Core::Runtime::AppContext& context);
        // Returns helper text based on the current overlay state
        std::string HelperText(const Software::Core::Runtime::AppContext& context) const;
        // Handles shared global shortcuts
        void HandleGlobalKeys(Software::Core::Runtime::AppContext& context);
        // Opens the selected search result
        void OpenSelectedSearchResult(Software::Core::Runtime::AppContext& context);
        // Starts editing one todo
        void BeginTodoEdit(const Software::Slate::TodoTicket& ticket);
        // Cancels the current todo form
        void CancelTodoForm(Software::Core::Runtime::AppContext& context);
        // Removes a pending slash todo command line
        bool RemovePendingTodoCommand(Software::Core::Runtime::AppContext& context);

        public:

        // Tests whether text is exactly the slash command that creates a todo
        static bool IsTodoSlashCommand(std::string_view text);
        // Replaces a pending slash todo command line with a todo block
        bool ReplacePendingTodoCommand(Software::Core::Runtime::AppContext& context, const std::string& block);
        // Saves the current todo form
        bool AcceptTodoForm(Software::Core::Runtime::AppContext& context);
        // Rewrites one todo inside its source document
        bool UpdateTodoTicket(Software::Core::Runtime::AppContext& context,
                              const Software::Slate::TodoTicket& ticket,
                              Software::Slate::TodoState state,
                              std::string_view title,
                              std::string_view description);

        // Stores the current prompt state
        PromptState m_prompt;
        // Stores the current confirm state
        ConfirmState m_confirm;
        // Stores the current todo form state
        TodoFormState m_todoForm;
        // Tracks selection in search results
        Software::Slate::NavigationController m_searchNavigation;
        // Tracks selection in todo results
        Software::Slate::NavigationController m_todoNavigation;
        // Stores the currently visible search results
        std::vector<Software::Slate::SearchResult> m_visibleSearchResults;
        // Stores the currently visible todos
        std::vector<Software::Slate::TodoTicket> m_visibleTodos;
        // Stores the search query text
        std::string m_searchBuffer;
        // Stores the todo filter query text
        std::string m_todoSearchBuffer;
        // Stores the footer status text
        std::string m_status = "ready";
        // Stores when the current status was set
        double m_statusSeconds = 0.0;
        // Stores the current frame time in seconds
        double m_nowSeconds = 0.0;
        // Marks whether the current status is an error
        bool m_statusIsError = false;
        // Marks whether search is open
        bool m_searchOverlayOpen = false;
        // Marks whether the workspace overlay is open
        bool m_workspaceOverlayOpen = false;
        // Marks whether the todo overlay is open
        bool m_todoOverlayOpen = false;
        // Requests focus for the search input
        bool m_focusSearch = false;
        // Requests focus for the todo search input
        bool m_focusTodoSearch = false;
        // Marks whether help is open
        bool m_helpOpen = false;
        // Stores the active search scope
        SearchOverlayScope m_searchOverlayScope = SearchOverlayScope::Workspace;
        // Stores the active workspace search mode
        Software::Slate::SearchMode m_searchMode = Software::Slate::SearchMode::FileNames;
        // Stores the active todo state filter
        int m_todoStateFilter = 0;
    };
}
