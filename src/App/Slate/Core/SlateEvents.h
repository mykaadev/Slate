#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include <string>

namespace Software::Slate::Events
{
    // Published once every module has started and the first route is active
    struct AppReady
    {
    };

    // Published after a workspace root was opened
    struct WorkspaceOpened
    {
        // Stores the absolute workspace root
        fs::path root;
    };

    // Published after a document was loaded into the editor
    struct DocumentOpened
    {
        // Stores the workspace relative path
        fs::path relativePath;
    };

    // Published right before the active document is written; listeners may rewrite the text
    struct DocumentWillSave
    {
        // Stores the workspace relative path
        fs::path relativePath;
        // Points at the text that is about to be written
        std::string* text = nullptr;
    };

    // Published after the active document was written to disk
    struct DocumentSaved
    {
        // Stores the workspace relative path
        fs::path relativePath;
    };
}
