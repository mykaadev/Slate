#pragma once

#include "App/Slate/Core/NavigationController.h"
#include "App/Slate/Core/SlateTypes.h"
#include "App/Slate/Input/ShortcutService.h"
#include "App/Slate/Todos/TodoService.h"
#include "Core/Runtime/AppContext.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    /** Callback surface used by the shell/mode for navigation and shell-level side effects. */
    struct TodoOverlayCallbacks
    {
        /** Opens the document that owns a selected todo. */
        std::function<bool(const fs::path& relativePath)> openDocument;

        /** Returns input focus to the active editor after the overlay closes. */
        std::function<void()> restoreEditorFocus;

        /** Emits a success/status message in the shell. */
        std::function<void(std::string message)> setStatus;

        /** Emits an error message in the shell. */
        std::function<void(std::string message)> setError;
    };

    /** Owns todo list state, todo form state, todo rendering, and todo command UI. */
    class SlateTodoOverlayController
    {
    public:
        /** Opens the searchable todo list overlay. */
        void OpenList(Software::Core::Runtime::AppContext& context);

        /** Opens the create-todo form, optionally replacing a slash command line. */
        void BeginCreate(Software::Core::Runtime::AppContext& context, bool fromSlashCommand = false);

        /** Clears all overlay state and closes list/form windows. */
        void Reset();

        /** Returns whether the todo list overlay is visible. */
        bool IsListOpen() const;

        /** Returns whether the create/edit form is visible. */
        bool IsFormOpen() const;

        /** Returns whether any todo overlay is visible. */
        bool IsAnyOpen() const;

        /** Returns the helper text to show in the global status bar. */
        std::string HelperText(const ShortcutService& shortcuts) const;

        /** Returns whether a text line is the todo slash command. */
        static bool IsSlashCommand(std::string_view text);

        /** Draws all visible todo overlays and handles their local input. */
        void Draw(Software::Core::Runtime::AppContext& context, const TodoOverlayCallbacks& callbacks);

    private:
        /** Selects whether the form creates a new todo or edits an existing one. */
        enum class TodoFormMode
        {
            /** Form creates a new todo Markdown document. */
            Create,

            /** Form updates an existing todo Markdown document. */
            Edit
        };

        /** Tracks which text field should own keyboard focus. */
        enum class TodoFormFocusField
        {
            /** Title input field. */
            Title,

            /** Description input field. */
            Description
        };

        /** Mutable state for the todo create/edit form. */
        struct TodoFormState
        {
            /** True while the form overlay is visible. */
            bool open = false;

            /** True when the next draw should focus the selected text field. */
            bool requestTextFocus = false;

            /** Field that should receive text focus. */
            TodoFormFocusField focusField = TodoFormFocusField::Title;

            /** True when the form was launched from and should replace a slash-command line. */
            bool replaceActiveLine = false;

            /** Active document path captured when replacing a slash-command line. */
            fs::path pendingCommandPath;

            /** One-based slash-command line captured when creating from /todo. */
            std::size_t pendingCommandLine = 0;

            /** Current create/edit mode. */
            TodoFormMode mode = TodoFormMode::Create;

            /** Todo ticket being edited when mode is Edit. */
            TodoTicket ticket;

            /** Todo state selected by the form. */
            TodoState state = TodoState::Open;

            /** Todo priority selected by the form. */
            TodoPriority priority = TodoPriority::Normal;

            /** User-editable todo title. */
            std::string title;

            /** User-editable todo description. */
            std::string description;

            /** Current cursor position inside the multiline description field. */
            int descriptionCursor = 0;

            /** Requested cursor position for the description field after synthetic edits. */
            int requestedDescriptionCursor = -1;
        };

        /** Opens the form for an existing todo ticket. */
        void BeginEdit(const TodoTicket& ticket);

        /** Draws the searchable todo list overlay. */
        void DrawList(Software::Core::Runtime::AppContext& context, const TodoOverlayCallbacks& callbacks);

        /** Draws the create/edit todo form overlay. */
        void DrawForm(Software::Core::Runtime::AppContext& context, const TodoOverlayCallbacks& callbacks);

        /** Cancels the form and restores focus when appropriate. */
        void CancelForm(Software::Core::Runtime::AppContext& context, const TodoOverlayCallbacks& callbacks);

        /** Saves the form into a standalone todo Markdown document. */
        bool AcceptForm(Software::Core::Runtime::AppContext& context, const TodoOverlayCallbacks& callbacks);

        /** Opens the selected todo document from the list. */
        bool OpenSelected(Software::Core::Runtime::AppContext& context, const TodoOverlayCallbacks& callbacks);

        /** Deletes the selected visible todo from the list. */
        bool DeleteSelected(Software::Core::Runtime::AppContext& context, const TodoOverlayCallbacks& callbacks);

        /** Removes a pending /todo command line before opening the form. */
        bool RemovePendingCommand(Software::Core::Runtime::AppContext& context);

        /** Reloads and reveals editor content after a todo service mutation. */
        void RefreshEditorAfterMutation(Software::Core::Runtime::AppContext& context,
                                        const TodoMutationResult& result);

        /** Navigation state for the visible todo list. */
        NavigationController m_navigation;

        /** Filtered todos currently shown by the list overlay. */
        std::vector<TodoTicket> m_visibleTodos;

        /** Create/edit form state. */
        TodoFormState m_form;

        /** True while the list overlay is visible. */
        bool m_listOpen = false;

        /** Current state filter index, where zero means all states. */
        int m_stateFilter = 0;
    };
}
