#ifndef MTCAD_GUI_APP_ICON_BOOTSTRAP_H
#define MTCAD_GUI_APP_ICON_BOOTSTRAP_H

#include <string>

struct SDL_Window;

namespace mtcad {
namespace runtime {

std::string ResolveIconPath(const char* icon_name);
void TrySetSdlWindowIcon(SDL_Window* window);

} // namespace runtime
} // namespace mtcad

#endif
