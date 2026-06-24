#include "theme_manager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

enum class BaseStyle {
    kDark,
    kLight,
    kClassic,
};

struct ThemeDefinition {
    std::string name;
    void (*builtin_apply)() = nullptr;
    BaseStyle base_style = BaseStyle::kDark;
    bool has_icon_tint = false;
    ImVec4 icon_tint = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    std::array<bool, ImGuiCol_COUNT> has_color = {};
    std::array<ImVec4, ImGuiCol_COUNT> colors = {};
};

std::vector<ThemeDefinition> g_themes;

std::string trim_copy(const std::string& value) {
    const std::string ws = " \t\r\n";
    const size_t start = value.find_first_not_of(ws);
    if (start == std::string::npos) {
        return std::string();
    }
    const size_t end = value.find_last_not_of(ws);
    return value.substr(start, end - start + 1);
}

std::string to_lower_copy(const std::string& value) {
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowered;
}

bool parse_float_token(std::string token, float* out_value) {
    if (out_value == nullptr) {
        return false;
    }
    token = trim_copy(token);
    if (token.empty()) {
        return false;
    }
    if (!token.empty() && (token.back() == 'f' || token.back() == 'F')) {
        token.pop_back();
    }

    try {
        *out_value = std::stof(token);
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_vec4(const std::string& value, ImVec4* out_color) {
    if (out_color == nullptr) {
        return false;
    }

    std::string cleaned = value;
    for (char& c : cleaned) {
        if (c == ',' || c == ';') {
            c = ' ';
        }
    }

    std::vector<std::string> parts;
    std::string current;
    for (char c : cleaned) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }

    if (parts.size() < 4) {
        return false;
    }

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
    if (!parse_float_token(parts[0], &r) || !parse_float_token(parts[1], &g) || !parse_float_token(parts[2], &b) || !parse_float_token(parts[3], &a)) {
        return false;
    }

    *out_color = ImVec4(r, g, b, a);
    return true;
}

BaseStyle infer_base_style_from_window_bg(const ThemeDefinition& theme) {
    if (!theme.has_color[ImGuiCol_WindowBg]) {
        return BaseStyle::kDark;
    }

    const ImVec4 c = theme.colors[ImGuiCol_WindowBg];
    const float luminance = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
    return (luminance >= 0.70f) ? BaseStyle::kLight : BaseStyle::kDark;
}

void apply_base_style(BaseStyle style) {
    switch (style) {
        case BaseStyle::kLight:
            ImGui::StyleColorsLight();
            break;
        case BaseStyle::kClassic:
            ImGui::StyleColorsClassic();
            break;
        case BaseStyle::kDark:
        default:
            ImGui::StyleColorsDark();
            break;
    }
}

void apply_custom_theme(const ThemeDefinition& theme) {
    apply_base_style(theme.base_style);
    ImGuiStyle& style = ImGui::GetStyle();
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        if (theme.has_color[i]) {
            style.Colors[i] = theme.colors[i];
        }
    }
}

std::unordered_map<std::string, ImGuiCol> make_color_name_map() {
    std::unordered_map<std::string, ImGuiCol> map;
    map["text"] = ImGuiCol_Text;
    map["textdisabled"] = ImGuiCol_TextDisabled;
    map["windowbg"] = ImGuiCol_WindowBg;
    map["childbg"] = ImGuiCol_ChildBg;
    map["popupbg"] = ImGuiCol_PopupBg;
    map["border"] = ImGuiCol_Border;
    map["bordershadow"] = ImGuiCol_BorderShadow;
    map["framebg"] = ImGuiCol_FrameBg;
    map["framebghovered"] = ImGuiCol_FrameBgHovered;
    map["framebgactive"] = ImGuiCol_FrameBgActive;
    map["titlebg"] = ImGuiCol_TitleBg;
    map["titlebgactive"] = ImGuiCol_TitleBgActive;
    map["titlebgcollapsed"] = ImGuiCol_TitleBgCollapsed;
    map["menubarbg"] = ImGuiCol_MenuBarBg;
    map["scrollbarbg"] = ImGuiCol_ScrollbarBg;
    map["scrollbargrab"] = ImGuiCol_ScrollbarGrab;
    map["scrollbargrabhovered"] = ImGuiCol_ScrollbarGrabHovered;
    map["scrollbargrabactive"] = ImGuiCol_ScrollbarGrabActive;
    map["checkmark"] = ImGuiCol_CheckMark;
    map["slidergrab"] = ImGuiCol_SliderGrab;
    map["slidergrabactive"] = ImGuiCol_SliderGrabActive;
    map["button"] = ImGuiCol_Button;
    map["buttonhovered"] = ImGuiCol_ButtonHovered;
    map["buttonactive"] = ImGuiCol_ButtonActive;
    map["header"] = ImGuiCol_Header;
    map["headerhovered"] = ImGuiCol_HeaderHovered;
    map["headeractive"] = ImGuiCol_HeaderActive;
    map["separator"] = ImGuiCol_Separator;
    map["separatorhovered"] = ImGuiCol_SeparatorHovered;
    map["separatoractive"] = ImGuiCol_SeparatorActive;
    map["resizegrip"] = ImGuiCol_ResizeGrip;
    map["resizegriphovered"] = ImGuiCol_ResizeGripHovered;
    map["resizegripactive"] = ImGuiCol_ResizeGripActive;
    map["tab"] = ImGuiCol_Tab;
    map["tabhovered"] = ImGuiCol_TabHovered;
    map["tabselected"] = ImGuiCol_TabSelected;
    map["tabselectedoverline"] = ImGuiCol_TabSelectedOverline;
    map["tabdimmed"] = ImGuiCol_TabDimmed;
    map["tabdimmedselected"] = ImGuiCol_TabDimmedSelected;
    map["tabdimmedselectedoverline"] = ImGuiCol_TabDimmedSelectedOverline;
    map["plotlines"] = ImGuiCol_PlotLines;
    map["plotlineshovered"] = ImGuiCol_PlotLinesHovered;
    map["plothistogram"] = ImGuiCol_PlotHistogram;
    map["plothistogramhovered"] = ImGuiCol_PlotHistogramHovered;
    map["tableheaderbg"] = ImGuiCol_TableHeaderBg;
    map["tableborderstrong"] = ImGuiCol_TableBorderStrong;
    map["tableborderlight"] = ImGuiCol_TableBorderLight;
    map["tablerowbg"] = ImGuiCol_TableRowBg;
    map["tablerowbgalt"] = ImGuiCol_TableRowBgAlt;
    map["texthighlight"] = ImGuiCol_TextLink;
    map["textselectedbg"] = ImGuiCol_TextSelectedBg;
    map["dragdroptarget"] = ImGuiCol_DragDropTarget;
    map["navcursor"] = ImGuiCol_NavCursor;
    map["navwindowinghighlight"] = ImGuiCol_NavWindowingHighlight;
    map["navwindowingdimbg"] = ImGuiCol_NavWindowingDimBg;
    map["modalwindowdimbg"] = ImGuiCol_ModalWindowDimBg;
    map["dockingpreview"] = ImGuiCol_DockingPreview;
    map["dockingemptybg"] = ImGuiCol_DockingEmptyBg;
    return map;
}

bool parse_base_style(const std::string& value, BaseStyle* out_style) {
    if (out_style == nullptr) {
        return false;
    }
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered == "dark") {
        *out_style = BaseStyle::kDark;
        return true;
    }
    if (lowered == "light") {
        *out_style = BaseStyle::kLight;
        return true;
    }
    if (lowered == "classic") {
        *out_style = BaseStyle::kClassic;
        return true;
    }
    return false;
}

bool parse_theme_file(const std::filesystem::path& file_path, ThemeDefinition* out_theme) {
    if (out_theme == nullptr) {
        return false;
    }

    std::ifstream in(file_path);
    if (!in.is_open()) {
        return false;
    }

    static const std::unordered_map<std::string, ImGuiCol> color_name_map = make_color_name_map();

    ThemeDefinition theme;
    theme.name = file_path.stem().string();

    bool any_setting = false;
    bool has_explicit_base_style = false;
    std::string line;
    while (std::getline(in, line)) {
        std::string trimmed = trim_copy(line);
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed.rfind("//", 0) == 0 || trimmed[0] == '#') {
            continue;
        }

        const size_t equals_pos = trimmed.find('=');
        if (equals_pos == std::string::npos) {
            continue;
        }

        std::string key = trim_copy(trimmed.substr(0, equals_pos));
        std::string value = trim_copy(trimmed.substr(equals_pos + 1));
        if (key.empty()) {
            continue;
        }
        if (!value.empty() && value.back() == ';') {
            value.pop_back();
            value = trim_copy(value);
        }

        const std::string lowered_key = to_lower_copy(key);
        if (lowered_key == "icon") {
            ImVec4 parsed_icon_tint;
            if (parse_vec4(value, &parsed_icon_tint)) {
                theme.icon_tint = parsed_icon_tint;
                theme.has_icon_tint = true;
                any_setting = true;
            }
            continue;
        }
        if (lowered_key == "basestyle" || lowered_key == "base") {
            BaseStyle parsed_style = BaseStyle::kDark;
            if (parse_base_style(value, &parsed_style)) {
                theme.base_style = parsed_style;
                has_explicit_base_style = true;
                any_setting = true;
            }
            continue;
        }

        const auto it = color_name_map.find(lowered_key);
        if (it == color_name_map.end()) {
            continue;
        }

        ImVec4 parsed_color;
        if (!parse_vec4(value, &parsed_color)) {
            continue;
        }

        theme.has_color[it->second] = true;
        theme.colors[it->second] = parsed_color;
        any_setting = true;
    }

    if (!any_setting) {
        return false;
    }

    if (!has_explicit_base_style) {
        theme.base_style = infer_base_style_from_window_bg(theme);
    }

    *out_theme = theme;
    return true;
}

void apply_dark_theme() {
    ImGui::StyleColorsDark();
}

void apply_light_theme() {
    ImGui::StyleColorsLight();
}

void apply_classic_theme() {
    ImGui::StyleColorsClassic();
}

const char* theme_combo_getter(void*, int idx) {
    if (idx < 0 || idx >= static_cast<int>(g_themes.size())) {
        return "";
    }
    return g_themes[(size_t)idx].name.c_str();
}

}  // namespace

bool initialize_theme_manager(const char* themes_directory) {
    g_themes.clear();

    ThemeDefinition dark;
    dark.name = "Dark";
    dark.builtin_apply = apply_dark_theme;
    dark.has_icon_tint = true;
    dark.icon_tint = ImVec4(0.82f, 0.84f, 0.88f, 1.00f);
    g_themes.push_back(dark);

    ThemeDefinition light;
    light.name = "Light";
    light.builtin_apply = apply_light_theme;
    g_themes.push_back(light);

    ThemeDefinition classic;
    classic.name = "Classic";
    classic.builtin_apply = apply_classic_theme;
    classic.has_icon_tint = true;
    classic.icon_tint = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    g_themes.push_back(classic);

    if (themes_directory == nullptr || themes_directory[0] == '\0') {
        return false;
    }

    int loaded_count = 0;
    std::error_code ec;
    const std::filesystem::path theme_root(themes_directory);
    if (!std::filesystem::exists(theme_root, ec) || !std::filesystem::is_directory(theme_root, ec)) {
        return false;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(theme_root, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string ext = to_lower_copy(entry.path().extension().string());
        if (ext != ".theme" && ext != ".themes") {
            continue;
        }

        ThemeDefinition parsed_theme;
        if (parse_theme_file(entry.path(), &parsed_theme)) {
            g_themes.push_back(parsed_theme);
            ++loaded_count;
        }
    }

    return loaded_count > 0;
}

int get_theme_count() {
    return static_cast<int>(g_themes.size());
}

const char* get_theme_name(int index) {
    if (g_themes.empty()) {
        return "Dark";
    }
    const int clamped = clamp_theme_index(index);
    return g_themes[(size_t)clamped].name.c_str();
}

int clamp_theme_index(int index) {
    if (g_themes.empty()) {
        return 0;
    }
    if (index < 0) {
        return 0;
    }
    const int max_index = get_theme_count() - 1;
    if (index > max_index) {
        return max_index;
    }
    return index;
}

bool get_theme_icon_tint(int index, ImVec4* out_icon_tint) {
    if (out_icon_tint == nullptr) {
        return false;
    }

    *out_icon_tint = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    if (g_themes.empty()) {
        return false;
    }

    const ThemeDefinition& theme = g_themes[(size_t)clamp_theme_index(index)];
    if (theme.has_icon_tint) {
        *out_icon_tint = theme.icon_tint;
    }
    return theme.has_icon_tint;
}

void apply_imgui_theme(int theme_index) {
    if (g_themes.empty()) {
        ImGui::StyleColorsDark();
        return;
    }

    const ThemeDefinition& theme = g_themes[(size_t)clamp_theme_index(theme_index)];
    if (theme.builtin_apply != nullptr) {
        theme.builtin_apply();
    } else {
        apply_custom_theme(theme);
    }
}

bool render_theme_combo(const char* label, int* selected_theme_index) {
    if (selected_theme_index == nullptr) {
        return false;
    }
    *selected_theme_index = clamp_theme_index(*selected_theme_index);
    return ImGui::Combo(label, selected_theme_index, theme_combo_getter, nullptr, get_theme_count());
}
