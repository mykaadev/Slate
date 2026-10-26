#pragma once

#include "Modes/Slate/SlateModeBase.h"
#include "App/Slate/Input/ShortcutService.h"

#include "imgui.h"

#include <functional>
#include <string>
#include <vector>

namespace Software::Modes::Slate
{
    // Draws the settings screen: categories on the left, rows in the middle, context on the right
    class SlateSettingsMode final : public SlateModeBase
    {
    public:
        // Settings categories, in tab order
        enum class Category
        {
            General = 0,
            Appearance,
            Editor,
            Keys,
            Plugins,
            Count
        };

    private:
        // One row of the settings list
        struct Row
        {
            enum class Kind
            {
                Header,
                Toggle,
                Choice,
                Keycap,
                Action,
                Plugin,
                Info
            };

            Kind kind = Kind::Info;
            std::string label;
            std::string value;
            std::string hint;
            bool on = false;
            ImVec4 accent{};
            // Called with -1 or +1 for left and right, and 0 for enter or a click
            std::function<void(int)> change;
        };

        // Returns the mode id
        const char* ModeName() const override;
        // Draws the settings mode
        void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) override;
        // Returns helper text for the settings mode
        std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const override;
        std::string StatusBadge(const Software::Core::Runtime::AppContext& context) const override;

        // Builds the rows for the current category or plugin page
        std::vector<Row> BuildRows(Software::Core::Runtime::AppContext& context);
        void BuildGeneralRows(Software::Core::Runtime::AppContext& context, std::vector<Row>& rows);
        void BuildAppearanceRows(Software::Core::Runtime::AppContext& context, std::vector<Row>& rows);
        void BuildEditorRows(Software::Core::Runtime::AppContext& context, std::vector<Row>& rows);
        void BuildKeyRows(Software::Core::Runtime::AppContext& context, std::vector<Row>& rows);
        void BuildPluginRows(Software::Core::Runtime::AppContext& context, std::vector<Row>& rows);
        void BuildPluginPageRows(Software::Core::Runtime::AppContext& context, std::vector<Row>& rows);

        void DrawCategories(float width, float height);
        void DrawRows(std::vector<Row>& rows, float width, float height);
        void DrawContextCard(Software::Core::Runtime::AppContext& context, const std::vector<Row>& rows, float width, float height);
        void DrawShortcutCapture(Software::Core::Runtime::AppContext& context);
        void SetCategory(Category category);

        Category m_category = Category::General;
        // Non-empty while a plugin's own settings page is open
        std::string m_pluginPage;
        // Selected row among the selectable rows
        std::size_t m_selected = 0;
        std::size_t m_selectableCount = 0;
        bool m_scrollToSelection = false;
        // Plugin id under the cursor in the plugin list, for the context card
        std::string m_hoveredPlugin;

        bool m_shortcutCaptureOpen = false;
        bool m_shortcutCaptureArmed = false;
        Software::Slate::ShortcutAction m_shortcutCaptureAction = Software::Slate::ShortcutAction::Count;
        std::string m_shortcutCaptureLabel;
    };
}
