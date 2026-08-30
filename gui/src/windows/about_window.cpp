#include "about_window.h"

#include <cstdio>
#include <cstdlib>

#include <SDL3/SDL.h>

void AboutWindow::SetOpen(bool open) {
    open_ = open;
}

bool AboutWindow::IsOpen() const {
    return open_;
}

AboutWindow::AboutWindowResult AboutWindow::Render(SDL_Window* parent_window) {
    AboutWindowResult result;
    if (!open_) {
        return result;
    }

    bool open_state = open_;
    ImGui::SetNextWindowSize(ImVec2(380.0f, 170.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("About", &open_state, ImGuiWindowFlags_NoDocking);

    const mtcad_kernel_version ver = mtcad_kernel_get_version();
    ImGui::Text("Kernel %d.%d.%d", ver.major, ver.minor, ver.patch);

    ImGui::End();
    open_ = open_state;
    return result;
}