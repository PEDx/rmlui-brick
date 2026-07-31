#include "prototype_render_interface.h"
#include "prototype_system_interface.h"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>
#include <SDL.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr int kWidth = 1024;
constexpr int kHeight = 768;
constexpr std::array<const char*, 5> kItemIds = {
    "item-recent", "item-games", "item-apps", "item-favorites", "item-settings"};
constexpr std::array<const char*, 5> kItemLabels = {
    "最近游玩", "游戏库", "应用", "收藏", "设置"};

struct Options {
    std::string assets = "assets";
    std::string font;
    std::string renderer;
    std::string screenshot;
    float dp_ratio = 2.0f;
    int exit_after_seconds = 0;
    bool fullscreen = false;
};

void PrintUsage(const char* program)
{
    std::fprintf(
        stderr,
        "Usage: %s --font FILE [--assets DIR] [--renderer NAME] [--dp-ratio N] [--fullscreen] [--seconds N] [--screenshot BMP]\n",
        program);
}

bool ParseOptions(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i) {
        const char* argument = argv[i];
        if (std::strcmp(argument, "--fullscreen") == 0) {
            options.fullscreen = true;
        } else if (std::strcmp(argument, "--assets") == 0 && i + 1 < argc) {
            options.assets = argv[++i];
        } else if (std::strcmp(argument, "--font") == 0 && i + 1 < argc) {
            options.font = argv[++i];
        } else if (std::strcmp(argument, "--renderer") == 0 && i + 1 < argc) {
            options.renderer = argv[++i];
        } else if (std::strcmp(argument, "--screenshot") == 0 && i + 1 < argc) {
            options.screenshot = argv[++i];
        } else if (std::strcmp(argument, "--dp-ratio") == 0 && i + 1 < argc) {
            char* end = nullptr;
            options.dp_ratio = std::strtof(argv[++i], &end);
            if (!end || *end != '\0' || options.dp_ratio < 0.5f || options.dp_ratio > 4.0f) {
                std::fprintf(stderr, "Invalid dp ratio; expected a number from 0.5 to 4.0\n");
                return false;
            }
        } else if (std::strcmp(argument, "--seconds") == 0 && i + 1 < argc) {
            options.exit_after_seconds = std::atoi(argv[++i]);
        } else if (std::strcmp(argument, "--help") == 0) {
            PrintUsage(argv[0]);
            return false;
        } else {
            std::fprintf(stderr, "Unknown or incomplete argument: %s\n", argument);
            PrintUsage(argv[0]);
            return false;
        }
    }
    return !options.font.empty();
}

void SetText(Rml::ElementDocument* document, const char* id, const Rml::String& value)
{
    if (Rml::Element* element = document->GetElementById(id))
        element->SetInnerRML(value);
}

class FocusController {
public:
    explicit FocusController(Rml::ElementDocument* document) : document(document) { Select(0); }

    void Move(int delta)
    {
        const int count = static_cast<int>(kItemIds.size());
        Select((selected + delta + count) % count);
        SetText(document, "status", Rml::CreateString("焦点移动：%s", kItemLabels[selected]));
    }

    void Activate()
    {
        Rml::Element* element = document->GetElementById(kItemIds[selected]);
        if (element)
            element->Click();
        SetText(document, "status", Rml::CreateString("A / Enter 已确认：%s", kItemLabels[selected]));
    }

private:
    void Select(int index)
    {
        selected = index;
        if (Rml::Element* element = document->GetElementById(kItemIds[selected]))
            element->Focus(true);
    }

    Rml::ElementDocument* document = nullptr;
    int selected = 0;
};

void LogJoystick(SDL_Joystick* joystick)
{
    if (!joystick)
        return;
    std::fprintf(
        stderr,
        "[input] joystick='%s' axes=%d buttons=%d hats=%d\n",
        SDL_JoystickName(joystick),
        SDL_JoystickNumAxes(joystick),
        SDL_JoystickNumButtons(joystick),
        SDL_JoystickNumHats(joystick));
}

bool SaveScreenshot(SDL_Renderer* renderer, const std::string& path)
{
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kWidth, kHeight, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        std::fprintf(stderr, "[capture] surface creation failed: %s\n", SDL_GetError());
        return false;
    }

    const bool success =
        SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) == 0 &&
        SDL_SaveBMP(surface, path.c_str()) == 0;
    std::fprintf(stderr, "[capture] %s: %s\n", success ? "saved" : "failed", path.c_str());
    if (!success)
        std::fprintf(stderr, "[capture] SDL error: %s\n", SDL_GetError());
    SDL_FreeSurface(surface);
    return success;
}

bool HandleControllerButton(Uint8 button, bool& running, FocusController& focus)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: focus.Move(-1); return true;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: focus.Move(1); return true;
    case SDL_CONTROLLER_BUTTON_A: focus.Activate(); return true;
    case SDL_CONTROLLER_BUTTON_B: running = false; return true;
    default: return false;
    }
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!ParseOptions(argc, argv, options))
        return 2;

    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    if (!options.renderer.empty())
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, options.renderer.c_str());

    const Uint32 sdl_flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER |
        SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER;
    if (SDL_Init(sdl_flags) != 0) {
        std::fprintf(stderr, "[fatal] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    const Uint32 window_flags = options.fullscreen ? SDL_WINDOW_FULLSCREEN : 0;
    SDL_Window* window = SDL_CreateWindow(
        "RmlUi on TrimUI Brick",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        kWidth,
        kHeight,
        window_flags);
    if (!window) {
        std::fprintf(stderr, "[fatal] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::fprintf(stderr, "[warn] accelerated renderer failed: %s; trying software\n", SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        std::fprintf(stderr, "[fatal] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_RenderSetLogicalSize(renderer, kWidth, kHeight);
    SDL_ShowCursor(SDL_DISABLE);

    PrototypeSystemInterface system_interface;
    PrototypeRenderInterface render_interface(renderer);
    Rml::SetSystemInterface(&system_interface);
    Rml::SetRenderInterface(&render_interface);

    int exit_code = 1;
    Rml::Context* context = nullptr;
    Rml::ElementDocument* document = nullptr;
    SDL_GameController* controller = nullptr;
    SDL_Joystick* joystick = nullptr;

    if (!Rml::Initialise()) {
        std::fprintf(stderr, "[fatal] RmlUi initialisation failed\n");
        goto shutdown_sdl;
    }

    context = Rml::CreateContext("brick", {kWidth, kHeight});
    if (!context) {
        std::fprintf(stderr, "[fatal] RmlUi context creation failed\n");
        goto shutdown_rmlui;
    }
    context->SetDensityIndependentPixelRatio(options.dp_ratio);
    std::fprintf(
        stderr,
        "[ui] context=%dx%d dp_ratio=%.2f logical=%.0fx%.0f dp\n",
        kWidth,
        kHeight,
        options.dp_ratio,
        kWidth / options.dp_ratio,
        kHeight / options.dp_ratio);

    if (!Rml::LoadFontFace(options.font)) {
        std::fprintf(stderr, "[fatal] cannot load font: %s\n", options.font.c_str());
        goto shutdown_rmlui;
    }

    Rml::Debugger::Initialise(context);
    document = context->LoadDocument(options.assets + "/main.rml");
    if (!document) {
        std::fprintf(stderr, "[fatal] cannot load %s/main.rml\n", options.assets.c_str());
        goto shutdown_rmlui;
    }
    document->Show();
    SetText(document, "density", Rml::CreateString("%.2g× DP", options.dp_ratio));
    SetText(document, "renderer", render_interface.GetDiagnostics());
    std::fprintf(stderr, "[render] %s\n", render_interface.GetDiagnostics().c_str());

    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
        std::fprintf(
            stderr,
            "[input] device %d: %s controller=%s\n",
            index,
            SDL_JoystickNameForIndex(index),
            SDL_IsGameController(index) ? "yes" : "no");
    }
    if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
        controller = SDL_GameControllerOpen(0);
        joystick = controller ? SDL_GameControllerGetJoystick(controller) : nullptr;
    } else if (SDL_NumJoysticks() > 0) {
        joystick = SDL_JoystickOpen(0);
    }
    LogJoystick(joystick);

    {
        FocusController focus(document);
        bool running = true;
        bool axis_up = false;
        bool axis_down = false;
        Uint64 frame_count = 0;
        Uint64 total_frames = 0;
        bool screenshot_saved = false;
        Uint64 fps_frame_start = SDL_GetPerformanceCounter();
        const Uint64 app_start = fps_frame_start;
        const Uint64 frequency = SDL_GetPerformanceFrequency();

        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT: running = false; break;
                case SDL_KEYDOWN:
                    if (event.key.repeat)
                        break;
                    if (event.key.keysym.sym == SDLK_UP)
                        focus.Move(-1);
                    else if (event.key.keysym.sym == SDLK_DOWN)
                        focus.Move(1);
                    else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE)
                        focus.Activate();
                    else if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q)
                        running = false;
                    else if (event.key.keysym.sym == SDLK_F8)
                        Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    std::fprintf(stderr, "[input] controller button=%u\n", event.cbutton.button);
                    HandleControllerButton(event.cbutton.button, running, focus);
                    break;
                case SDL_JOYBUTTONDOWN:
                    std::fprintf(stderr, "[input] joystick button=%u\n", event.jbutton.button);
                    if (!controller) {
                        if (event.jbutton.button == 0)
                            focus.Activate();
                        else if (event.jbutton.button == 1)
                            running = false;
                    }
                    break;
                case SDL_JOYHATMOTION:
                    std::fprintf(stderr, "[input] hat=%u value=%u\n", event.jhat.hat, event.jhat.value);
                    if (!controller) {
                        if (event.jhat.value & SDL_HAT_UP)
                            focus.Move(-1);
                        else if (event.jhat.value & SDL_HAT_DOWN)
                            focus.Move(1);
                    }
                    break;
                case SDL_JOYAXISMOTION:
                    if (!controller && event.jaxis.axis == 1) {
                        const bool new_up = event.jaxis.value < -16000;
                        const bool new_down = event.jaxis.value > 16000;
                        if (new_up && !axis_up)
                            focus.Move(-1);
                        if (new_down && !axis_down)
                            focus.Move(1);
                        axis_up = new_up;
                        axis_down = new_down;
                    }
                    break;
                default: break;
                }
            }

            context->Update();
            render_interface.BeginFrame();
            context->Render();
            if (!options.screenshot.empty() && !screenshot_saved && total_frames >= 75) {
                screenshot_saved = SaveScreenshot(renderer, options.screenshot);
            }
            render_interface.EndFrame();
            ++frame_count;
            ++total_frames;

            const Uint64 now = SDL_GetPerformanceCounter();
            const double sample_seconds = static_cast<double>(now - fps_frame_start) / frequency;
            if (sample_seconds >= 0.5) {
                const double fps = static_cast<double>(frame_count) / sample_seconds;
                SetText(document, "fps", Rml::CreateString("%.1f FPS", fps));
                std::fprintf(stderr, "[frame] %.1f FPS renderer_ok=%s\n", fps, render_interface.IsHealthy() ? "yes" : "no");
                frame_count = 0;
                fps_frame_start = now;
            }

            if (options.exit_after_seconds > 0 &&
                static_cast<double>(now - app_start) / frequency >= options.exit_after_seconds)
                running = false;
        }
    }

    exit_code = render_interface.IsHealthy() ? 0 : 3;

shutdown_rmlui:
    if (controller)
        SDL_GameControllerClose(controller);
    else if (joystick)
        SDL_JoystickClose(joystick);
    Rml::Shutdown();

shutdown_sdl:
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exit_code;
}
