#ifndef OPENSTAGE_GUI_WINDOWS_ABOUT_WINDOW_H
#define OPENSTAGE_GUI_WINDOWS_ABOUT_WINDOW_H

#include <string>

#include "mtkernel/kernel.h"

#include "imgui.h"

struct SDL_Window;

class AboutWindow {
public:
    struct AboutWindowResult {};

    void SetOpen(bool open);
    bool IsOpen() const;
    AboutWindowResult Render(SDL_Window* parent_window);

private:
    bool open_ = false;
};

#endif