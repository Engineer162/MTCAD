#include "shortcut_manager.h"

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <sstream>

#include <SDL3/SDL.h>
#include "imgui.h"

namespace {

static void normalize_scancodes(std::vector<int>* scancodes) {
	if (scancodes == nullptr) {
		return;
	}

	std::vector<int>& values = *scancodes;
	values.erase(std::remove_if(values.begin(), values.end(), [](int scancode) {
		return scancode <= 0 || scancode >= SDL_SCANCODE_COUNT || scancode == SDL_SCANCODE_ESCAPE;
	}), values.end());

	std::sort(values.begin(), values.end());
	values.erase(std::unique(values.begin(), values.end()), values.end());
}

static std::string trim_copy(const std::string& value) {
	const std::string ws = " \t\r\n";
	const size_t start = value.find_first_not_of(ws);
	if (start == std::string::npos) {
		return std::string();
	}
	const size_t end = value.find_last_not_of(ws);
	return value.substr(start, end - start + 1);
}

static bool is_modifier_scancode(int scancode) {
	return scancode == SDL_SCANCODE_LCTRL || scancode == SDL_SCANCODE_RCTRL ||
		scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT ||
		scancode == SDL_SCANCODE_LALT || scancode == SDL_SCANCODE_RALT ||
		scancode == SDL_SCANCODE_LGUI || scancode == SDL_SCANCODE_RGUI;
}

static bool is_scancode_pressed_with_modifier_pairs(int scancode, const bool* keys, int key_count) {
	if (keys == nullptr || key_count <= 0) {
		return false;
	}

	auto is_down = [&](int s) {
		return s > 0 && s < key_count && keys[s];
	};

	if (scancode == SDL_SCANCODE_LCTRL || scancode == SDL_SCANCODE_RCTRL) {
		return is_down(SDL_SCANCODE_LCTRL) || is_down(SDL_SCANCODE_RCTRL);
	}
	if (scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT) {
		return is_down(SDL_SCANCODE_LSHIFT) || is_down(SDL_SCANCODE_RSHIFT);
	}
	if (scancode == SDL_SCANCODE_LALT || scancode == SDL_SCANCODE_RALT) {
		return is_down(SDL_SCANCODE_LALT) || is_down(SDL_SCANCODE_RALT);
	}
	if (scancode == SDL_SCANCODE_LGUI || scancode == SDL_SCANCODE_RGUI) {
		return is_down(SDL_SCANCODE_LGUI) || is_down(SDL_SCANCODE_RGUI);
	}
	return is_down(scancode);
}

} // namespace

bool IsShortcutEmpty(const ShortcutChord& shortcut) {
	return shortcut.scancodes.empty();
}

bool IsShortcutDown(const ShortcutChord& shortcut) {
	if (shortcut.scancodes.empty()) {
		return false;
	}

	int key_count = 0;
	const bool* keys = SDL_GetKeyboardState(&key_count);
	if (keys == nullptr || key_count <= 0) {
		return false;
	}

	for (int scancode : shortcut.scancodes) {
		if (scancode <= 0 || scancode >= key_count || !is_scancode_pressed_with_modifier_pairs(scancode, keys, key_count)) {
			return false;
		}
	}

	for (int scancode = 1; scancode < key_count; ++scancode) {
		if (!keys[scancode] || scancode == SDL_SCANCODE_ESCAPE) {
			continue;
		}

		bool expected = false;
		for (int expected_scancode : shortcut.scancodes) {
			if (expected_scancode == scancode) {
				expected = true;
				break;
			}
			if (is_modifier_scancode(expected_scancode) && is_modifier_scancode(scancode)) {
				if ((expected_scancode == SDL_SCANCODE_LCTRL || expected_scancode == SDL_SCANCODE_RCTRL) &&
					(scancode == SDL_SCANCODE_LCTRL || scancode == SDL_SCANCODE_RCTRL)) {
					expected = true;
					break;
				}
				if ((expected_scancode == SDL_SCANCODE_LSHIFT || expected_scancode == SDL_SCANCODE_RSHIFT) &&
					(scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT)) {
					expected = true;
					break;
				}
				if ((expected_scancode == SDL_SCANCODE_LALT || expected_scancode == SDL_SCANCODE_RALT) &&
					(scancode == SDL_SCANCODE_LALT || scancode == SDL_SCANCODE_RALT)) {
					expected = true;
					break;
				}
				if ((expected_scancode == SDL_SCANCODE_LGUI || expected_scancode == SDL_SCANCODE_RGUI) &&
					(scancode == SDL_SCANCODE_LGUI || scancode == SDL_SCANCODE_RGUI)) {
					expected = true;
					break;
				}
			}
		}

		if (!expected) {
			return false;
		}
	}

	return true;
}

std::string ShortcutToDisplayString(const ShortcutChord& shortcut) {
	if (shortcut.scancodes.empty()) {
		return "Unassigned";
	}

	std::ostringstream out;
	bool first = true;
	for (int scancode : shortcut.scancodes) {
		const SDL_Scancode s = static_cast<SDL_Scancode>(scancode);
		const char* key_name = SDL_GetScancodeName(s);
		const std::string name = (key_name != nullptr && key_name[0] != '\0') ? key_name : "Unknown";
		if (!first) {
			out << " + ";
		}
		out << name;
		first = false;
	}
	return out.str();
}

std::string ShortcutToStorageString(const ShortcutChord& shortcut) {
	if (shortcut.scancodes.empty()) {
		return std::string();
	}

	std::ostringstream out;
	for (size_t i = 0; i < shortcut.scancodes.size(); ++i) {
		if (i > 0) {
			out << ',';
		}
		out << shortcut.scancodes[i];
	}
	return out.str();
}

bool ShortcutFromStorageString(const std::string& value, ShortcutChord* out_shortcut) {
	if (out_shortcut == nullptr) {
		return false;
	}

	ShortcutChord parsed;
	const std::string trimmed = trim_copy(value);
	if (trimmed.empty()) {
		out_shortcut->scancodes.clear();
		return true;
	}

	std::istringstream stream(trimmed);
	std::string token;
	while (std::getline(stream, token, ',')) {
		token = trim_copy(token);
		if (token.empty()) {
			continue;
		}

		try {
			const int scancode = std::stoi(token);
			parsed.scancodes.push_back(scancode);
		} catch (...) {
			return false;
		}
	}

	normalize_scancodes(&parsed.scancodes);
	*out_shortcut = std::move(parsed);
	return true;
}

bool DrawShortcutAssignmentWidget(const char* label, ShortcutChord* shortcut, ShortcutAssignmentState* state) {
	if (label == nullptr || shortcut == nullptr || state == nullptr) {
		return false;
	}

	normalize_scancodes(&shortcut->scancodes);
	normalize_scancodes(&state->captured_scancodes);

	bool changed = false;
	const std::string display = state->capturing ? "Press keys (Esc cancels)" : ShortcutToDisplayString(*shortcut);
	ImGui::PushID(label);
	std::string button_label = display + "##shortcut_assign";
	if (ImGui::Button(button_label.c_str(), ImVec2(-FLT_MIN, 0.0f))) {
		state->capturing = true;
		state->captured_scancodes.clear();
	}

	if (!state->capturing) {
		ImGui::PopID();
		return changed;
	}

	int key_count = 0;
	const bool* keys = SDL_GetKeyboardState(&key_count);
	if (keys == nullptr || key_count <= 0) {
		ImGui::PopID();
		return changed;
	}

	if (SDL_SCANCODE_ESCAPE < key_count && keys[SDL_SCANCODE_ESCAPE]) {
		state->capturing = false;
		state->captured_scancodes.clear();
		ImGui::PopID();
		return changed;
	}

	if (SDL_SCANCODE_BACKSPACE < key_count && keys[SDL_SCANCODE_BACKSPACE]) {
		if (!shortcut->scancodes.empty()) {
			shortcut->scancodes.clear();
			changed = true;
		}
		state->capturing = false;
		state->captured_scancodes.clear();
		ImGui::PopID();
		return changed;
	}

	bool has_key_currently_down = false;
	for (int scancode = 1; scancode < key_count; ++scancode) {
		if (!keys[scancode]) {
			continue;
		}
		if (scancode == SDL_SCANCODE_ESCAPE) {
			continue;
		}
		has_key_currently_down = true;
		state->captured_scancodes.push_back(scancode);
	}
	normalize_scancodes(&state->captured_scancodes);

	if (!has_key_currently_down && !state->captured_scancodes.empty()) {
		shortcut->scancodes = state->captured_scancodes;
		normalize_scancodes(&shortcut->scancodes);
		state->capturing = false;
		state->captured_scancodes.clear();
		changed = true;
	}

	ImGui::PopID();
	return changed;
}

bool IsShortcutAssignmentActive(const ShortcutAssignmentState& state) {
	return state.capturing;
}