#ifndef MTCAD_GUI_APP_SETTINGS_IO_H
#define MTCAD_GUI_APP_SETTINGS_IO_H

#include <filesystem>

#include "../windows/settings_window.h"

namespace mtcad {
namespace runtime {

void ApplyDefaultShortcuts(UserSettings* settings);
void EnsureDefaultImGuiIni(const std::filesystem::path& settings_dir);
bool LoadUserSettingsIni(const char* file_path, UserSettings* out_settings);
bool SaveUserSettingsIni(const char* file_path, const UserSettings& settings);

} // namespace runtime
} // namespace mtcad

#endif
