#include "icon_bootstrap.h"

#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "../managers/icon_manager.h"
#include "../managers/path_manager.h"

namespace mtcad {
namespace runtime {

std::string ResolveIconPath(const char* icon_name) {
    if (icon_name == nullptr || icon_name[0] == '\0') {
        return std::string();
    }

    const std::filesystem::path icon_path = get_app_data_directory() / "icons" / (std::string(icon_name) + ".png");
    if (std::filesystem::exists(icon_path)) {
        return icon_path.string();
    }
    return std::string();
}

void TrySetSdlWindowIcon(SDL_Window* window) {
    if (window == nullptr) {
        return;
    }

    const std::filesystem::path app_icon_path = get_app_data_directory() / "icons" / "mtcad.png";
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    if (!load_icon_rgba_from_file(app_icon_path.string().c_str(), &pixels, &width, &height)) {
        return;
    }

    SDL_Surface* icon_surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels.data(), width * 4);
    if (icon_surface != nullptr) {
        SDL_SetWindowIcon(window, icon_surface);
        SDL_DestroySurface(icon_surface);
    }
}

} // namespace runtime
} // namespace mtcad
