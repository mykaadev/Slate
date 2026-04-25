#include "App/Slate/Core/AppPaths.h"

#include <cstdlib>
#include <string>

namespace Software::Slate::AppPaths
{
    fs::path EnvPath(const char* name)
    {
        const char* value = std::getenv(name);
        return value ? fs::path(value) : fs::path();
    }

    fs::path UserHomeDirectory()
    {
        fs::path home = EnvPath("USERPROFILE");
        if (home.empty())
        {
            home = EnvPath("HOME");
        }
        return home;
    }

    fs::path RootDirectory()
    {
        fs::path base = EnvPath("APPDATA");
        if (base.empty())
        {
            base = UserHomeDirectory();
        }
        if (base.empty())
        {
            base = fs::current_path();
        }
        return base / "Slate";
    }

    fs::path ConfigDirectory()
    {
        return RootDirectory() / "config";
    }

    fs::path ConfigDirectory(std::string_view category)
    {
        return ConfigDirectory() / fs::path(std::string(category));
    }

    fs::path ConfigFile(std::string_view filename)
    {
        return ConfigDirectory() / fs::path(std::string(filename));
    }

    fs::path ConfigFile(std::string_view category, std::string_view filename)
    {
        return ConfigDirectory(category) / fs::path(std::string(filename));
    }

    fs::path LegacyConfigFile(std::string_view filename)
    {
        return RootDirectory() / fs::path(std::string(filename));
    }
}
