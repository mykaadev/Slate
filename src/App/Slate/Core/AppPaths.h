#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include <string_view>

namespace Software::Slate::AppPaths
{

    /** Returns the value of an environment variable as a file path. */
    fs::path EnvPath(const char* name);

    /** Returns the best available per-user home/profile directory for user-created default paths. */
    fs::path UserHomeDirectory();

    /** Returns the root Slate data directory, for example %APPDATA%/Slate on Windows. */
    fs::path RootDirectory();

    /** Returns the root directory used for app-owned configuration files. */
    fs::path ConfigDirectory();

    /** Returns a category subdirectory inside ConfigDirectory, such as input or editor. */
    fs::path ConfigDirectory(std::string_view category);

    /** Returns a file path inside ConfigDirectory. Kept for uncategorized legacy callers. */
    fs::path ConfigFile(std::string_view filename);

    /** Returns a file path inside a category subdirectory of ConfigDirectory. */
    fs::path ConfigFile(std::string_view category, std::string_view filename);

    /** Returns the old flat config path used by earlier builds for migration/fallback reads only. */
    fs::path LegacyConfigFile(std::string_view filename);
}
