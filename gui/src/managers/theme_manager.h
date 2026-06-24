#ifndef MTCAD_GUI_THEME_MANAGER_H
#define MTCAD_GUI_THEME_MANAGER_H

#include "imgui.h"

bool initialize_theme_manager(const char* themes_directory);
int get_theme_count();
const char* get_theme_name(int index);
int clamp_theme_index(int index);
bool get_theme_icon_tint(int index, ImVec4* out_icon_tint);
void apply_imgui_theme(int theme_index);
bool render_theme_combo(const char* label, int* selected_theme_index);

#endif
