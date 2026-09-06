#ifndef MTCAD_SHORTCUT_MANAGER_H
#define MTCAD_SHORTCUT_MANAGER_H

#include <string>
#include <vector>

struct ShortcutChord {
	std::vector<int> scancodes;
};

struct ShortcutAssignmentState {
	bool capturing = false;
	std::vector<int> captured_scancodes;
};

bool IsShortcutEmpty(const ShortcutChord& shortcut);
bool IsShortcutDown(const ShortcutChord& shortcut);
std::string ShortcutToDisplayString(const ShortcutChord& shortcut);
std::string ShortcutToStorageString(const ShortcutChord& shortcut);
bool ShortcutFromStorageString(const std::string& value, ShortcutChord* out_shortcut);
bool DrawShortcutAssignmentWidget(const char* label, ShortcutChord* shortcut, ShortcutAssignmentState* state);
bool IsShortcutAssignmentActive(const ShortcutAssignmentState& state);


#endif