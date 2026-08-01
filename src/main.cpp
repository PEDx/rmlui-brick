#include "prototype_render_interface.h"
#include "prototype_system_interface.h"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>
#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
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
constexpr Uint64 kTransitionMilliseconds = 220;
constexpr Uint64 kIdlePollMilliseconds = 16;
constexpr Uint64 kBatteryPollMilliseconds = 60000;
constexpr Uint64 kFrameLogMilliseconds = 60000;
constexpr Uint64 kScreenshotDelayMilliseconds = 1250;

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

struct DeviceStatusState {
    std::string clock;
    int battery = -2;
};

bool UpdateDeviceStatus(
    Rml::ElementDocument* document,
    DeviceStatusState& state,
    bool update_battery)
{
    bool changed = false;
    const std::time_t now = std::time(nullptr);
    std::tm local_time = {};
#if defined(_WIN32)
    localtime_s(&local_time, &now);
#else
    localtime_r(&now, &local_time);
#endif
    char clock[16] = {};
    std::strftime(clock, sizeof(clock), "%H:%M", &local_time);
    if (state.clock != clock) {
        state.clock = clock;
        SetText(document, "clock", clock);
        changed = true;
    }

    if (update_battery) {
        const int battery = ReadBatteryPercent();
        if (state.battery != battery) {
            state.battery = battery;
            if (battery >= 0) {
                SetText(document, "battery-text", Rml::CreateString("%d%%", battery));
                if (Rml::Element* fill = document->GetElementById("battery-fill"))
                    fill->SetProperty("width", Rml::CreateString("%d%%", battery));
            } else {
                SetText(document, "battery-text", "--%");
                if (Rml::Element* fill = document->GetElementById("battery-fill"))
                    fill->SetProperty("width", "0%");
            }
            changed = true;
        }
    }

    return changed;
}

class DesktopController {
public:
    explicit DesktopController(Rml::ElementDocument* document) : document(document) { UpdateCarousel(); }

    bool Move(int delta)
    {
        if (detail_visible)
            return false;
        const int count = static_cast<int>(kModels.size());
        selected = (selected + delta + count) % count;
        UpdateCarousel();
        return true;
    }

    bool Activate()
    {
        if (detail_visible)
            return false;

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
        return true;
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
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return desktop.Move(-1);
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return desktop.Move(1);
    case SDL_CONTROLLER_BUTTON_A: return desktop.Activate();
    case SDL_CONTROLLER_BUTTON_B:
        if (desktop.Back())
            return true;
        running = false;
        return false;
    default: return false;
    }
}

Uint64 MillisecondsToTicks(Uint64 milliseconds, Uint64 frequency)
{
    return milliseconds * frequency / 1000;
}

Uint64 MillisecondsUntilNextMinute()
{
    const std::time_t now = std::time(nullptr);
    const int seconds_into_minute = static_cast<int>((now % 60 + 60) % 60);
    return static_cast<Uint64>(60 - seconds_into_minute) * 1000;
}

int MillisecondsUntil(Uint64 now, Uint64 deadline, Uint64 frequency)
{
    if (now >= deadline)
        return 0;
    const Uint64 ticks = deadline - now;
    return static_cast<int>((ticks * 1000 + frequency - 1) / frequency);
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
    DeviceStatusState device_status;

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
    UpdateDeviceStatus(document, device_status, true);
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
        bool dirty = true;
        Uint64 frame_count = 0;
        bool screenshot_pending = !options.screenshot.empty();
        const Uint64 app_start = SDL_GetPerformanceCounter();
        const Uint64 frequency = SDL_GetPerformanceFrequency();
        const Uint64 transition_ticks = MillisecondsToTicks(kTransitionMilliseconds, frequency);
        const Uint64 battery_poll_ticks = MillisecondsToTicks(kBatteryPollMilliseconds, frequency);
        const Uint64 frame_log_ticks = MillisecondsToTicks(kFrameLogMilliseconds, frequency);
        Uint64 animate_until = 0;
        Uint64 next_clock_poll = app_start + MillisecondsToTicks(MillisecondsUntilNextMinute(), frequency);
        Uint64 next_battery_poll = app_start + battery_poll_ticks;
        Uint64 next_frame_log = app_start + frame_log_ticks;
        Uint64 frame_log_start = app_start;
        const Uint64 screenshot_at = app_start + MillisecondsToTicks(kScreenshotDelayMilliseconds, frequency);
        const Uint64 exit_at = options.exit_after_seconds > 0
            ? app_start + static_cast<Uint64>(options.exit_after_seconds) * frequency
            : 0;

        auto process_event = [&](const SDL_Event& event) {
            bool changed = false;
            switch (event.type) {
            case SDL_QUIT: running = false; break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_EXPOSED)
                    changed = true;
                break;
            case SDL_KEYDOWN:
                if (event.key.repeat)
                    break;
                if (event.key.keysym.sym == SDLK_LEFT)
                    changed = desktop.Move(-1);
                else if (event.key.keysym.sym == SDLK_RIGHT)
                    changed = desktop.Move(1);
                else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE)
                    changed = desktop.Activate();
                else if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_BACKSPACE) {
                    changed = desktop.Back();
                    if (!changed)
                        running = false;
                } else if (event.key.keysym.sym == SDLK_q) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_F8) {
                    Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
                    changed = true;
                }
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                std::fprintf(stderr, "[input] controller button=%u\n", event.cbutton.button);
                changed = HandleControllerButton(event.cbutton.button, running, desktop);
                break;
            case SDL_JOYBUTTONDOWN:
                std::fprintf(stderr, "[input] joystick button=%u\n", event.jbutton.button);
                if (!controller) {
                    if (event.jbutton.button == 0) {
                        changed = desktop.Activate();
                    } else if (event.jbutton.button == 1) {
                        changed = desktop.Back();
                        if (!changed)
                            running = false;
                    }
                }
                break;
            case SDL_JOYHATMOTION:
                std::fprintf(stderr, "[input] hat=%u value=%u\n", event.jhat.hat, event.jhat.value);
                if (!controller) {
                    if (event.jhat.value & SDL_HAT_LEFT)
                        changed = desktop.Move(-1);
                    else if (event.jhat.value & SDL_HAT_RIGHT)
                        changed = desktop.Move(1);
                }
                break;
            case SDL_JOYAXISMOTION:
                if (!controller && event.jaxis.axis == 0) {
                    const bool new_left = event.jaxis.value < -16000;
                    const bool new_right = event.jaxis.value > 16000;
                    if (new_left && !axis_left)
                        changed = desktop.Move(-1);
                    if (new_right && !axis_right)
                        changed = desktop.Move(1) || changed;
                    axis_left = new_left;
                    axis_right = new_right;
                }
                break;
            default: break;
            }

            if (changed) {
                dirty = true;
                animate_until = SDL_GetPerformanceCounter() + transition_ticks;
            }
        };

        while (running) {
            Uint64 now = SDL_GetPerformanceCounter();

            if (now >= next_clock_poll) {
                dirty = UpdateDeviceStatus(document, device_status, false) || dirty;
                next_clock_poll = now + MillisecondsToTicks(MillisecondsUntilNextMinute(), frequency);
            }
            if (now >= next_battery_poll) {
                dirty = UpdateDeviceStatus(document, device_status, true) || dirty;
                next_battery_poll = now + battery_poll_ticks;
            }
            if (now >= next_frame_log) {
                const double sample_seconds = static_cast<double>(now - frame_log_start) / frequency;
                const double rendered_fps = static_cast<double>(frame_count) / sample_seconds;
                std::fprintf(
                    stderr,
                    "[frame] rendered=%.1f FPS mode=%s renderer_ok=%s\n",
                    rendered_fps,
                    now < animate_until ? "animated" : "idle",
                    render_interface.IsHealthy() ? "yes" : "no");
                frame_count = 0;
                frame_log_start = now;
                next_frame_log = now + frame_log_ticks;
            }
            if (screenshot_pending && now >= screenshot_at)
                dirty = true;
            if (exit_at != 0 && now >= exit_at) {
                running = false;
                break;
            }

            const bool animating = now < animate_until;
            if (!dirty && !animating) {
                int wait_milliseconds = MillisecondsUntil(now, next_clock_poll, frequency);
                wait_milliseconds = std::min(
                    wait_milliseconds,
                    MillisecondsUntil(now, next_battery_poll, frequency));
                wait_milliseconds = std::min(
                    wait_milliseconds,
                    MillisecondsUntil(now, next_frame_log, frequency));
                if (screenshot_pending) {
                    wait_milliseconds = std::min(
                        wait_milliseconds,
                        MillisecondsUntil(now, screenshot_at, frequency));
                }
                if (exit_at != 0) {
                    wait_milliseconds = std::min(
                        wait_milliseconds,
                        MillisecondsUntil(now, exit_at, frequency));
                }

                wait_milliseconds = std::min(
                    wait_milliseconds,
                    static_cast<int>(kIdlePollMilliseconds));
                // Brick's SDL_WaitEventTimeout fallback pumps the controller at
                // roughly 1 kHz. An explicit delay keeps input responsive while
                // avoiding that idle CPU cost.
                if (wait_milliseconds > 0)
                    SDL_Delay(static_cast<Uint32>(wait_milliseconds));

                SDL_Event event;
                while (SDL_PollEvent(&event))
                    process_event(event);
                continue;
            }

            SDL_Event event;
            while (SDL_PollEvent(&event))
                process_event(event);
            if (!running)
                break;

            now = SDL_GetPerformanceCounter();
            context->Update();
            render_interface.BeginFrame();
            context->Render();
            if (screenshot_pending && now >= screenshot_at) {
                SaveScreenshot(renderer, options.screenshot);
                screenshot_pending = false;
            }
            render_interface.EndFrame();
            dirty = false;
            ++frame_count;
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
