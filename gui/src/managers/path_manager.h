#pragma once

#include <filesystem>

std::filesystem::path get_app_data_directory();
std::filesystem::path get_app_lib_directory();
std::filesystem::path get_settings_directory();
