#pragma once

#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Markdown/JournalService.h"
#include "Modes/Slate/SlateModeBase.h"

namespace Software::Modes::Slate
{
    // Draws the main editor screen and its companion panels
    class SlateEditorMode final : public SlateModeBase
    {
    private:
        // Returns the mode id
        const char* ModeName() const override;
        // Draws the editor mode
        void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) override;
        // Returns helper text for the editor mode
        std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const override;
        // Reports whether the native editor should be visible
        bool WantsNativeEditorVisible(const Software::Core::Runtime::AppContext& context) const override;

        // Draws the full editor layout
        void DrawEditor(Software::Core::Runtime::AppContext& context,
                        Software::Slate::DocumentService::Document& document);
        // Draws the outline content
        void DrawOutline(Software::Core::Runtime::AppContext& context);
        // Draws the outline panel shell
        void DrawOutlinePanel(Software::Core::Runtime::AppContext& context);
        // Draws the journal helper panel
        void DrawJournalCompanion(Software::Core::Runtime::AppContext& context,
                                  const Software::Slate::DocumentService::Document& document,
                                  float layoutWidth = 0.0f,
                                  float textInset = 0.0f,
                                  float textWidth = 0.0f);
        // Draws the month calendar view
        void DrawJournalCalendar(const Software::Slate::JournalMonthSummary& summary);
        // Draws the month activity graph
        void DrawJournalActivityGraph(const Software::Slate::JournalMonthSummary& summary);
        // Draws the fallback live markdown editor
        void DrawLiveMarkdownEditor(Software::Core::Runtime::AppContext& context,
                                    Software::Slate::DocumentService::Document& document);
        // Draws the markdown preview panel
        void DrawMarkdownPreview(Software::Core::Runtime::AppContext& context,
                                 const Software::Slate::DocumentService::Document& document);
        // Draws the preview body content
        void DrawMarkdownPreviewContent(Software::Core::Runtime::AppContext& context,
                                        const Software::Slate::DocumentService::Document& document,
                                        float contentWidth);
    };
}
