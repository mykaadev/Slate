#pragma once

#include "App/Slate/Core/NavigationController.h"
#include "App/Slate/Core/SlateTypes.h"
#include "Core/Runtime/AppContext.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    // Callback surface used by the shell/mode for navigation and shell-level side effects.
    struct TodoOverlayCallbacks
    {
        std::function<bool(const fs::path& relativePath)> openDocument;
        std::function<void()> restoreEditorFocus;
        std::function<void(std::string message)> setStatus;
        std::function<void(std::string message)> setError;
    };

    // Owns todo list state, todo form state, todo rendering, and markdown todo mutations.
    class SlateTodoOverlayController
    {
    public:
        void OpenList(Software::Core::Runtime::AppContext& context);
        void BeginCreate(Software::Core::Runtime::AppContext& context, bool fromSlashCommand = false);
        void Reset();

        bool IsListOpen() const;
        bool IsFormOpen() const;
        bool IsAnyOpen() const;
        std::string HelperText() const;

        static bool IsSlashCommand(std::string_view text);

        void Draw(Software::Core::Runtime::AppContext& context, const TodoOverlayCallbacks& callbacks);

    private:
        enum class TodoFormMode
        {
            Create,
            Edit
        };

        enum class TodoFormFocusField
        {
            Title,
            Description
        };

        struct TodoFormState
        {
            bool open = false;
            bool requestTextFocus = false;
            TodoFormFocusField focusField = TodoFormFocusField::Title;
            bool replaceActiveLine = false;
            fs::path pendingCommandPath;
            std::size_t pendingCommandLine = 0;
            TodoFormMode mode = TodoFormMode::Create;
            TodoTicket ticket;
            TodoState state = TodoState::Open;
            std::string title;
            std::string description;
        };

        void BeginEdit(const TodoTicket& ticket);
        void DrawList(Software::Core::Runtime::AppContext& context, const TodoOverlayCallbacks& callbacks);
        void DrawForm(Software::Core::Runtime::AppContext& context, const TodoOverlayCallbacks& callbacks);
        void CancelForm(Software::Core::Runtime::AppContext& context, const TodoOverlayCallbacks& callbacks);
        bool AcceptForm(Software::Core::Runtime::AppContext& context, const TodoOverlayCallbacks& callbacks);
        bool RemovePendingCommand(Software::Core::Runtime::AppContext& context);
        bool ReplacePendingCommand(Software::Core::Runtime::AppContext& context, const std::string& block);
        bool UpdateTicket(Software::Core::Runtime::AppContext& context,
                          const TodoTicket& ticket,
                          TodoState state,
                          std::string_view title,
                          std::string_view description,
                          const TodoOverlayCallbacks& callbacks);

        NavigationController m_navigation;
        std::vector<TodoTicket> m_visibleTodos;
        std::string m_searchBuffer;
        TodoFormState m_form;
        bool m_listOpen = false;
        bool m_focusSearch = false;
        int m_stateFilter = 0;
    };
}
