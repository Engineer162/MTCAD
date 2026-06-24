#include "path_manager.h"

#include <filesystem>

#if defined(_WIN32)
#include <shlobj.h>
#endif

std::filesystem::path get_app_data_directory()
{
    const char* env_dir = std::getenv("MTCAD_APP_DATA_DIR");
    if (env_dir != nullptr && env_dir[0] != '\0') {
        return std::filesystem::path(env_dir);
    }
    return std::filesystem::current_path() / "assets";
}

std::filesystem::path get_app_lib_directory()
{
    const char* env_dir = std::getenv("MTCAD_APP_LIB_DIR");
    if (env_dir != nullptr && env_dir[0] != '\0') {
        return std::filesystem::path(env_dir);
    }
    return std::filesystem::current_path();
}

std::filesystem::path get_settings_directory()
{
    std::filesystem::path result;

#if defined(_WIN32)
    PWSTR path = nullptr;

    HRESULT hr = SHGetKnownFolderPath(
        FOLDERID_RoamingAppData,
        0,
        nullptr,
        &path);

    if (SUCCEEDED(hr) && path != nullptr)
    {
        result = std::filesystem::path(path) / "MTCAD";
        CoTaskMemFree(path);
    }
#else
    const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config_home != nullptr && xdg_config_home[0] != '\0') {
        result = std::filesystem::path(xdg_config_home) / "MTCAD";
    } else {
        const char* home = std::getenv("HOME");
        if (home != nullptr && home[0] != '\0') {
            result = std::filesystem::path(home) / ".config" / "MTCAD";
        }
    }
#endif

    if (result.empty()) {
        // Fallback if we cannot resolve a writable user config directory.
        return std::filesystem::path(".") / "MTCAD";
    }

    std::error_code ec;
    std::filesystem::create_directories(result, ec);
    return result;
}