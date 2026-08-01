#include "prototype_render_interface.h"
#include "prototype_system_interface.h"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>
#include <SDL.h>
#include <SDL_image.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>

namespace {

constexpr int kWidth = 1024;
constexpr int kHeight = 768;

struct ModelInfo {
    const char* id;
    const char* title;
    const char* subtitle;
    const char* image;
};

constexpr std::array<ModelInfo, 3> kModels = {{
    {"model-gb", "GAME BOY", "1989 · PLAY IT LOUD", "icons/gb-dmg-simple.png"},
    {"model-gbc", "GAME BOY COLOR", "1998 · COLOR IN YOUR HANDS", "icons/gbc-atomic-purple-simple.png"},
    {"model-gba", "GAME BOY ADVANCE", "2001 · ADVANCE YOUR PLAY", "icons/gba-indigo-simple.png"},
}};

struct Options {
    std::string assets = "assets";
    std::string font;
    std::string renderer;
    std::string screenshot;
    float dp_ratio = 1.0f;
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

int ReadBatteryPercent()
{
    std::ifstream stream("/sys/class/power_supply/axp2202-battery/capacity");
    int percent = -1;
    if (stream >> percent && percent >= 0 && percent <= 100)
        return percent;
    return -1;
}

void UpdateDeviceStatus(Rml::ElementDocument* document)
{
    const std::time_t now = std::time(nullptr);
    std::tm local_time = {};
#if defined(_WIN32)
    localtime_s(&local_time, &now);
#else
    localtime_r(&now, &local_time);
#endif
    char clock[16] = {};
    std::strftime(clock, sizeof(clock), "%H:%M", &local_time);
    SetText(document, "clock", clock);

    const int battery = ReadBatteryPercent();
    if (battery >= 0) {
        SetText(document, "battery-text", Rml::CreateString("%d%%", battery));
        if (Rml::Element* fill = document->GetElementById("battery-fill"))
            fill->SetProperty("width", Rml::CreateString("%d%%", battery));
    } else {
        SetText(document, "battery-text", "--%");
        if (Rml::Element* fill = document->GetElementById("battery-fill"))
            fill->SetProperty("width", "0%");
    }
}

class DesktopController {
public:
    explicit DesktopController(Rml::ElementDocument* document) : document(document) { UpdateCarousel(); }

    void Move(int delta)
    {
        if (detail_visible)
            return;
        const int count = static_cast<int>(kModels.size());
        selected = (selected + delta + count) % count;
        UpdateCarousel();
    }

    void Activate()
    {
        if (detail_visible)
            return;

        const ModelInfo& model = kModels[selected];
        SetText(document, "detail-title", model.title);
        SetText(document, "detail-subtitle", model.subtitle);
        if (Rml::Element* image = document->GetElementById("detail-image"))
            image->SetAttribute("src", model.image);
        SetDisplay("home-view", "none");
        SetDisplay("home-footer", "none");
        SetDisplay("detail-view", "flex");
        SetDisplay("detail-footer", "flex");
        detail_visible = true;
    }

    bool Back()
    {
        if (!detail_visible)
            return false;

        SetDisplay("detail-view", "none");
        SetDisplay("detail-footer", "none");
        SetDisplay("home-view", "flex");
        SetDisplay("home-footer", "flex");
        detail_visible = false;
        UpdateCarousel();
        return true;
    }

private:
    void SetDisplay(const char* id, const char* value)
    {
        if (Rml::Element* element = document->GetElementById(id))
            element->SetProperty("display", value);
    }

    void UpdateCarousel()
    {
        const int count = static_cast<int>(kModels.size());
        const int left = (selected + count - 1) % count;
        const int right = (selected + 1) % count;

        for (int index = 0; index < count; ++index) {
            if (Rml::Element* card = document->GetElementById(kModels[index].id)) {
                card->SetClass("slot-left", index == left);
                card->SetClass("slot-center", index == selected);
                card->SetClass("slot-right", index == right);
                card->SetClass("selected", index == selected);
                if (index == selected)
                    card->Focus(true);
            }
            if (Rml::Element* dot = document->GetElementById(Rml::CreateString("position-%d", index)))
                dot->SetClass("active", index == selected);
        }

        SetText(document, "selected-title", kModels[selected].title);
        SetText(document, "selected-subtitle", kModels[selected].subtitle);
    }

    Rml::ElementDocument* document = nullptr;
    int selected = 1;
    bool detail_visible = false;
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

bool HandleControllerButton(Uint8 button, bool& running, DesktopController& desktop)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: desktop.Move(-1); return true;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: desktop.Move(1); return true;
    case SDL_CONTROLLER_BUTTON_A: desktop.Activate(); return true;
    case SDL_CONTROLLER_BUTTON_B:
        if (!desktop.Back())
            running = false;
        return true;
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

    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        std::fprintf(stderr, "[fatal] SDL_image PNG initialisation failed: %s\n", IMG_GetError());
        SDL_Quit();
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
        IMG_Quit();
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
        IMG_Quit();
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
        "[ui] context=%dx%d dp_ratio=%.2f logical=%.2fx%.2f dp\n",
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
    UpdateDeviceStatus(document);
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
        DesktopController desktop(document);
        bool running = true;
        bool axis_left = false;
        bool axis_right = false;
        Uint64 frame_count = 0;
        Uint64 total_frames = 0;
        bool screenshot_saved = false;
        Uint64 fps_frame_start = SDL_GetPerformanceCounter();
        Uint64 status_update_start = fps_frame_start;
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
                    if (event.key.keysym.sym == SDLK_LEFT)
                        desktop.Move(-1);
                    else if (event.key.keysym.sym == SDLK_RIGHT)
                        desktop.Move(1);
                    else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE)
                        desktop.Activate();
                    else if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE) {
                        if (!desktop.Back())
                            running = false;
                    } else if (event.key.keysym.sym == SDLK_q)
                            running = false;
                    else if (event.key.keysym.sym == SDLK_F8)
                        Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    std::fprintf(stderr, "[input] controller button=%u\n", event.cbutton.button);
                    HandleControllerButton(event.cbutton.button, running, desktop);
                    break;
                case SDL_JOYBUTTONDOWN:
                    std::fprintf(stderr, "[input] joystick button=%u\n", event.jbutton.button);
                    if (!controller) {
                        if (event.jbutton.button == 0)
                            desktop.Activate();
                        else if (event.jbutton.button == 1 && !desktop.Back())
                                running = false;
                    }
                    break;
                case SDL_JOYHATMOTION:
                    std::fprintf(stderr, "[input] hat=%u value=%u\n", event.jhat.hat, event.jhat.value);
                    if (!controller) {
                        if (event.jhat.value & SDL_HAT_LEFT)
                            desktop.Move(-1);
                        else if (event.jhat.value & SDL_HAT_RIGHT)
                            desktop.Move(1);
                    }
                    break;
                case SDL_JOYAXISMOTION:
                    if (!controller && event.jaxis.axis == 0) {
                        const bool new_left = event.jaxis.value < -16000;
                        const bool new_right = event.jaxis.value > 16000;
                        if (new_left && !axis_left)
                            desktop.Move(-1);
                        if (new_right && !axis_right)
                            desktop.Move(1);
                        axis_left = new_left;
                        axis_right = new_right;
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
                std::fprintf(stderr, "[frame] %.1f FPS renderer_ok=%s\n", fps, render_interface.IsHealthy() ? "yes" : "no");
                frame_count = 0;
                fps_frame_start = now;
            }

            if (static_cast<double>(now - status_update_start) / frequency >= 1.0) {
                UpdateDeviceStatus(document);
                status_update_start = now;
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
    IMG_Quit();
    SDL_Quit();
    return exit_code;
}
