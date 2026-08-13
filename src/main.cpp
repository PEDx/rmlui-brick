#include "game_catalog.h"
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
constexpr Uint64 kBatteryPollMilliseconds = 10000;
constexpr Uint64 kFrameLogMilliseconds = 60000;
constexpr Uint64 kScreenshotDelayMilliseconds = 1250;
constexpr Uint64 kConsoleCarouselTransitionMilliseconds = 180;
constexpr Uint64 kGameRailTransitionMilliseconds = 240;
constexpr int kGameVisibleCount = 4;
constexpr int kGameTrackSlotCount = 6;
constexpr int kGameCardStep = 236;
constexpr int kGameTrackRestingLeft = -231;

struct ModelInfo {
    const char* id;
    GameSystem system;
};

constexpr std::array<ModelInfo, 6> kModels = {{
    {"model-gb", GameSystem::GameBoy},
    {"model-gbc", GameSystem::GameBoyColor},
    {"model-gba", GameSystem::GameBoyAdvance},
    {"model-sfc", GameSystem::SuperNintendo},
    {"model-md", GameSystem::SegaGenesis},
    {"model-gg", GameSystem::SegaGameGear},
}};

constexpr int WrapIndex(int index, int count)
{
    return (index % count + count) % count;
}

static_assert(WrapIndex(-1, 6) == 5, "carousel must wrap left");
static_assert(WrapIndex(6, 6) == 0, "carousel must wrap right");

struct Options {
    std::string assets = "assets";
    std::string font;
    std::string renderer;
    std::string screenshot;
    std::string rom_root = "/mnt/SDCARD/Roms";
    std::string request = "/tmp/rmlui-next";
    std::string state = "/tmp/rmlui-state";
    float dp_ratio = 1.0f;
    int exit_after_seconds = 0;
    bool fullscreen = false;
};

void PrintUsage(const char* program)
{
    std::fprintf(
        stderr,
        "Usage: %s --font FILE [--assets DIR] [--rom-root DIR] [--request FILE] [--state FILE] [--renderer NAME] [--dp-ratio N] [--fullscreen] [--seconds N] [--screenshot BMP]\n",
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
        } else if (std::strcmp(argument, "--rom-root") == 0 && i + 1 < argc) {
            options.rom_root = argv[++i];
        } else if (std::strcmp(argument, "--request") == 0 && i + 1 < argc) {
            options.request = argv[++i];
        } else if (std::strcmp(argument, "--state") == 0 && i + 1 < argc) {
            options.state = argv[++i];
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

Rml::String EscapeRml(const Rml::String& value)
{
    Rml::String escaped;
    escaped.reserve(value.size());
    for (char character : value) {
        switch (character) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        default: escaped += character; break;
        }
    }
    return escaped;
}

void SetText(Rml::ElementDocument* document, const char* id, const Rml::String& value)
{
    if (Rml::Element* element = document->GetElementById(id))
        element->SetInnerRML(EscapeRml(value));
}

int ReadBatteryPercent()
{
    std::ifstream stream("/sys/class/power_supply/axp2202-battery/capacity");
    int percent = -1;
    if (stream >> percent && percent >= 0 && percent <= 100)
        return percent;
    return -1;
}

std::string ReadBatteryStatus()
{
    std::ifstream stream("/sys/class/power_supply/axp2202-battery/status");
    std::string status;
    if (std::getline(stream, status))
        return status;
    return {};
}

struct DeviceStatusState {
    std::string clock;
    int battery = -2;
    std::string battery_status;
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
        const std::string battery_status = ReadBatteryStatus();
        if (state.battery != battery || state.battery_status != battery_status) {
            state.battery = battery;
            state.battery_status = battery_status;
            if (battery >= 0) {
                SetText(document, "battery-text", Rml::CreateString("%d%%", battery));
                if (Rml::Element* fill = document->GetElementById("battery-fill")) {
                    const int fill_width = battery == 0
                        ? 0
                        : std::max(3, (30 * battery + 50) / 100);
                    fill->SetProperty("width", Rml::CreateString("%dpx", fill_width));
                }
            } else {
                SetText(document, "battery-text", "--%");
                if (Rml::Element* fill = document->GetElementById("battery-fill"))
                    fill->SetProperty("width", "0px");
            }
            if (Rml::Element* status = document->GetElementById("battery-status")) {
                const bool charging = battery_status == "Charging" || battery_status == "Full";
                status->SetClass("charging", charging);
                status->SetClass("charged", battery_status == "Full");
                status->SetClass("critical", !charging && battery >= 0 && battery <= 10);
                status->SetClass("low", !charging && battery > 10 && battery <= 20);
            }
            changed = true;
        }
    }

    return changed;
}

class DesktopController {
public:
    DesktopController(Rml::ElementDocument* document, const Options& options)
        : document(document), options(options)
    {
        if (Rml::Element* roll = document->GetElementById("console-roll"))
            roll->SetClass("initializing", true);
        RestoreState();
    }

    bool Move(int delta)
    {
        if (view == View::Home) {
            const int direction = delta < 0 ? -1 : 1;
            if (carousel_animation != CarouselAnimation::Idle) {
                carousel_queued_steps += direction;
                return true;
            }
            StartCarouselMove(direction);
            return true;
        }

        if (games.empty())
            return false;
        const int count = static_cast<int>(games.size());
        selected_game = static_cast<std::size_t>(
            (static_cast<int>(selected_game) + delta + count) % count);
        UpdateLibrary();
        SaveState(games[selected_game].path);
        return true;
    }

    bool Activate()
    {
        if (view == View::Home) {
            OpenLibrary();
            return true;
        }

        if (games.empty())
            return false;

        std::string error;
        if (!WriteLaunchRequest(
                options.request,
                kModels[selected_system].system,
                games[selected_game].path,
                error)) {
            SetText(document, "library-message", error);
            return true;
        }

        SaveState(games[selected_game].path);
        std::fprintf(
            stderr,
            "[launch] system=%s rom=%s request=%s\n",
            GetGameSystemInfo(kModels[selected_system].system).id,
            games[selected_game].path.c_str(),
            options.request.c_str());
        exit_requested = true;
        return true;
    }

    bool Back()
    {
        if (view != View::Library)
            return false;

        SetDisplay("library-view", "none");
        SetDisplay("library-footer", "none");
        SetDisplay("library-mark", "none");
        SetDisplay("home-view", "flex");
        SetDisplay("home-footer", "flex");
        SetDisplay("home-mark", "flex");
        view = View::Home;
        SaveState({});
        UpdateCarousel();
        return true;
    }

    bool ExitRequested() const { return exit_requested; }

    bool UpdateAnimations()
    {
        bool changed = false;
        if (carousel_initialization_frames > 0) {
            --carousel_initialization_frames;
            if (carousel_initialization_frames == 0) {
                if (Rml::Element* roll = document->GetElementById("console-roll"))
                    roll->SetClass("initializing", false);
                changed = true;
            }
        }

        const Uint64 now = SDL_GetTicks64();
        if (carousel_animation == CarouselAnimation::Positioning) {
            // An input received while the app is idle is processed in the
            // event-only branch of the main loop. Keep the off-screen setup
            // unchanged for one render before assigning target slots, or the
            // hidden card's teleport can be interpolated across the viewport.
            carousel_animation = CarouselAnimation::Prepared;
            changed = true;
        } else if (carousel_animation == CarouselAnimation::Prepared) {
            // The incoming card was rendered once just outside the viewport.
            // Assign all target slots on the following frame so every move,
            // including GG -> GB and GB -> GG, travels the short direction.
            UpdateCarousel(carousel_direction);
            if (Rml::Element* incoming = document->GetElementById(kModels[carousel_incoming].id))
                incoming->SetClass("carousel-snap", false);
            carousel_animation_deadline = now + kConsoleCarouselTransitionMilliseconds;
            carousel_animation = CarouselAnimation::Sliding;
            changed = true;
        } else if (carousel_animation == CarouselAnimation::Sliding) {
            if (now < carousel_animation_deadline) {
                changed = true;
            } else {
                if (Rml::Element* outgoing = document->GetElementById(kModels[carousel_outgoing].id)) {
                    // The outgoing card has completed its normal trip beyond
                    // the viewport. Stop drawing it until it is explicitly
                    // prepared as an incoming card on a later move.
                    outgoing->SetClass("carousel-snap", true);
                    outgoing->SetClass("carousel-parked", true);
                }
                carousel_animation = CarouselAnimation::Idle;
                if (carousel_queued_steps != 0) {
                    const int direction = carousel_queued_steps < 0 ? -1 : 1;
                    carousel_queued_steps -= direction;
                    StartCarouselMove(direction);
                }
                changed = true;
            }
        }

        if (rail_animation == RailAnimation::Idle)
            return changed;

        if (rail_animation == RailAnimation::Revealing) {
            // The rail was populated while hidden. Reveal it once with all
            // transitions disabled so no stale selection state from the
            // previous library visit can be painted in an intermediate slot.
            SetDisplay("game-track", "flex");
            rail_animation = RailAnimation::Snapping;
            return true;
        }

        if (rail_animation == RailAnimation::Sliding) {
            if (now < rail_animation_deadline) {
                // Drive the rail position explicitly instead of relying on an
                // RCSS transition. This guarantees visible, frame-by-frame
                // movement on the Brick renderer and gives the scroll a soft
                // cubic ease-out as it settles on the next card.
                const double elapsed = static_cast<double>(now - rail_animation_started_at);
                const double duration = static_cast<double>(kGameRailTransitionMilliseconds);
                const double progress = std::min(elapsed / duration, 1.0);
                const double remaining = 1.0 - progress;
                const double eased = 1.0 - remaining * remaining * remaining;
                const double left = static_cast<double>(rail_animation_from_left) +
                    static_cast<double>(rail_animation_to_left - rail_animation_from_left) * eased;
                if (Rml::Element* track = document->GetElementById("game-track"))
                    track->SetProperty("left", Rml::CreateString("%.2fpx", left));
                return true;
            }

            visible_start = rail_target_start;
            if (Rml::Element* track = document->GetElementById("game-track")) {
                // Keep the active card visually neutral during the invisible
                // ring-buffer reset. It will reveal only after reaching its
                // final slot.
                track->SetClass("snapping", true);
                track->SetProperty("left", Rml::CreateString("%dpx", kGameTrackRestingLeft));
            }
            PopulateGameTrack();
            rail_animation = RailAnimation::Snapping;
            return true;
        }

        // The reset above was rendered once without a transition. Re-enable
        // transitions on the next frame, then continue toward any selection
        // queued by rapid directional input.
        Rml::Element* track = document->GetElementById("game-track");
        if (track)
            track->SetClass("snapping", false);
        rail_animation = RailAnimation::Idle;

        const std::size_t desired_start = CalculateVisibleStart();
        if (desired_start != visible_start) {
            // Keep the active treatment hidden while catching up with rapid
            // input; reveal it only after the final queued step arrives.
            StartRailMove(desired_start);
        } else if (track) {
            track->SetClass("scrolling", false);
        }
        return true;
    }

    bool IsAnimating() const
    {
        return carousel_initialization_frames > 0 ||
            carousel_animation != CarouselAnimation::Idle ||
            rail_animation != RailAnimation::Idle;
    }

private:
    enum class View { Home, Library };
    enum class CarouselAnimation { Idle, Positioning, Prepared, Sliding };
    enum class RailAnimation { Idle, Revealing, Sliding, Snapping };

    void SetDisplay(const char* id, const char* value)
    {
        if (Rml::Element* element = document->GetElementById(id))
            element->SetProperty("display", value);
    }

    void SetImage(const char* id, const std::string& source)
    {
        if (Rml::Element* image = document->GetElementById(id))
            image->SetAttribute("src", source);
    }

    void OpenLibrary(const std::string& preferred_rom_path = {})
    {
        games = ScanGames(options.rom_root, kModels[selected_system].system);
        selected_game = 0;
        if (!preferred_rom_path.empty()) {
            const auto match = std::find_if(games.begin(), games.end(), [&](const GameInfo& game) {
                return game.path == preferred_rom_path;
            });
            if (match != games.end())
                selected_game = static_cast<std::size_t>(match - games.begin());
        }
        visible_start = 0;
        rail_target_start = 0;
        rail_animation = RailAnimation::Idle;
        library_track_initialized = false;
        std::fprintf(
            stderr,
            "[catalog] system=%s root=%s games=%zu\n",
            GetGameSystemInfo(kModels[selected_system].system).id,
            options.rom_root.c_str(),
            games.size());
        SetDisplay("home-view", "none");
        SetDisplay("home-footer", "none");
        SetDisplay("home-mark", "none");
        SetDisplay("library-view", "flex");
        SetDisplay("library-footer", "flex");
        SetDisplay("library-mark", "flex");
        view = View::Library;
        UpdateLibrary();
        SaveState(games.empty() ? std::string() : games[selected_game].path);
    }

    std::size_t CalculateVisibleStart() const
    {
        if (games.size() <= kGameVisibleCount)
            return 0;

        // Treat the second visible card as the navigation anchor. After the
        // selection reaches it, subsequent input scrolls the rail beneath the
        // fixed active position. At the beginning and on the final screen the
        // window is clamped, allowing the active card to travel to the first,
        // third, and fourth positions so every game remains reachable.
        const std::size_t maximum_start = games.size() - kGameVisibleCount;
        const std::size_t anchored_start = selected_game > 0 ? selected_game - 1 : 0;
        return std::min(anchored_start, maximum_start);
    }

    void PopulateGameTrack()
    {
        const GameSystemInfo& system = GetGameSystemInfo(kModels[selected_system].system);
        for (int slot = 0; slot < kGameTrackSlotCount; ++slot) {
            const Rml::String card_id = Rml::CreateString("game-card-%d", slot);
            Rml::Element* card = document->GetElementById(card_id);
            if (!card)
                continue;

            // Slot zero and slot five are the off-screen buffers. Keeping empty
            // slots in the flex layout is important; display:none would collapse
            // them and change the track's geometry near either end of the list.
            const long long game_index = static_cast<long long>(visible_start) + slot - 1;
            const bool valid = game_index >= 0 && game_index < static_cast<long long>(games.size());
            card->SetProperty("display", "block");
            card->SetClass("buffer-empty", !valid);
            card->SetClass("active", valid && static_cast<std::size_t>(game_index) == selected_game);
            if (!valid) {
                card->SetClass("placeholder", false);
                continue;
            }

            const GameInfo& game = games[static_cast<std::size_t>(game_index)];
            card->SetClass("placeholder", game.cover.empty());
            SetImage(
                Rml::CreateString("game-cover-%d", slot).c_str(),
                game.cover.empty() ? system.console_image : game.cover);
            SetText(
                document,
                Rml::CreateString("game-label-%d", slot).c_str(),
                game.title);
        }
    }

    void StartRailMove(std::size_t desired_start)
    {
        if (rail_animation != RailAnimation::Idle || desired_start == visible_start)
            return;

        const long long difference = static_cast<long long>(desired_start) -
            static_cast<long long>(visible_start);
        if (difference != -1 && difference != 1) {
            // Restoring a far-away game or wrapping from one end to the other is
            // not spatially adjacent, so snap directly instead of sweeping past
            // unrelated covers.
            visible_start = desired_start;
            PopulateGameTrack();
            return;
        }

        PopulateGameTrack();
        rail_target_start = desired_start;
        const int direction = difference > 0 ? 1 : -1;
        rail_animation_from_left = kGameTrackRestingLeft;
        rail_animation_to_left = kGameTrackRestingLeft - direction * kGameCardStep;
        if (Rml::Element* track = document->GetElementById("game-track")) {
            track->SetClass("snapping", false);
            track->SetClass("scrolling", true);
            track->SetProperty("left", Rml::CreateString("%dpx", rail_animation_from_left));
        }
        rail_animation_started_at = SDL_GetTicks64();
        rail_animation_deadline = rail_animation_started_at + kGameRailTransitionMilliseconds;
        rail_animation = RailAnimation::Sliding;
    }

    void UpdateLibrary()
    {
        const GameSystemInfo& system = GetGameSystemInfo(kModels[selected_system].system);
        SetText(document, "library-system", system.title);
        SetText(document, "library-count", Rml::CreateString("%zu GAMES", games.size()));
        SetText(document, "library-core", Rml::CreateString("%s · %s", system.core_name, system.title));
        SetText(document, "library-message", "");

        if (games.empty()) {
            SetDisplay("library-empty", "flex");
            SetDisplay("game-track", "none");
            SetText(document, "library-title", "没有找到可运行的 ROM");
            SetText(document, "library-position", "0 / 0");
            return;
        }

        SetDisplay("library-empty", "none");
        const std::size_t desired_start = CalculateVisibleStart();
        if (!library_track_initialized) {
            // Populate off-screen first. The next animation update reveals the
            // already-finalized first frame, preventing a one-frame flash of a
            // stale active card in the second slot.
            SetDisplay("game-track", "none");
            visible_start = desired_start;
            if (Rml::Element* track = document->GetElementById("game-track")) {
                track->SetClass("scrolling", false);
                track->SetClass("snapping", true);
                track->SetProperty("left", Rml::CreateString("%dpx", kGameTrackRestingLeft));
            }
            PopulateGameTrack();
            rail_animation = RailAnimation::Revealing;
            library_track_initialized = true;
        } else if (rail_animation == RailAnimation::Idle) {
            SetDisplay("game-track", "flex");
            if (desired_start == visible_start)
                PopulateGameTrack();
            else
                StartRailMove(desired_start);
        } else {
            if (rail_animation != RailAnimation::Revealing)
                SetDisplay("game-track", "flex");
            // Refresh the active marker immediately when input arrives during a
            // slide. The requested window is picked up after the current step.
            PopulateGameTrack();
        }

        SetText(document, "library-title", games[selected_game].title);
        SetText(
            document,
            "library-position",
            Rml::CreateString("%zu / %zu", selected_game + 1, games.size()));
    }

    void SaveState(const std::string& rom_path)
    {
        if (options.state.empty() || rom_path.find('\n') != std::string::npos)
            return;
        const std::string temporary = options.state + ".tmp";
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
            return;
        stream << GetGameSystemInfo(kModels[selected_system].system).id << '\n' << rom_path << '\n';
        stream.close();
        if (stream)
            std::rename(temporary.c_str(), options.state.c_str());
    }

    void RestoreState()
    {
        std::ifstream stream(options.state, std::ios::binary);
        std::string system_id;
        std::string rom_path;
        if (!std::getline(stream, system_id) || !std::getline(stream, rom_path)) {
            UpdateCarousel();
            return;
        }

        for (std::size_t index = 0; index < kModels.size(); ++index) {
            if (system_id == GetGameSystemInfo(kModels[index].system).id) {
                selected_system = static_cast<int>(index);
                break;
            }
        }
        UpdateCarousel();
        if (rom_path.empty())
            return;

        // Resolve the saved game before the rail is populated. Restoring it
        // after OpenLibrary() prepared the first frame briefly displayed the
        // beginning of the catalog and could leave no visible card active.
        OpenLibrary(rom_path);
    }

    void StartCarouselMove(int direction)
    {
        const int count = static_cast<int>(kModels.size());
        carousel_direction = direction;
        selected_system = WrapIndex(selected_system + direction, count);
        carousel_incoming = WrapIndex(selected_system + direction, count);
        carousel_outgoing = WrapIndex(selected_system - direction * 2, count);

        const int target_left = WrapIndex(selected_system - 1, count);
        const int target_right = WrapIndex(selected_system + 1, count);
        for (int index = 0; index < count; ++index) {
            const bool target_visible =
                index == target_left || index == selected_system || index == target_right;
            if (target_visible || index == carousel_outgoing) {
                if (Rml::Element* participant = document->GetElementById(kModels[index].id))
                    participant->SetClass("carousel-parked", false);
                continue;
            }

            if (Rml::Element* parked = document->GetElementById(kModels[index].id)) {
                // Only four cards take part in a move: outgoing, left, center
                // and right. Reposition every other card outside the viewport
                // during the preparation frame with transitions disabled, so
                // a wrapped index can never fly across the visible carousel.
                parked->SetClass("carousel-snap", true);
                parked->SetClass("slot-left", false);
                parked->SetClass("slot-center", false);
                parked->SetClass("slot-right", false);
                parked->SetClass("slot-off-left", direction > 0);
                parked->SetClass("slot-off-right", direction < 0);
                parked->SetClass("carousel-parked", true);
            }
        }

        if (Rml::Element* incoming = document->GetElementById(kModels[carousel_incoming].id)) {
            // Teleport the hidden card to the entering side with transitions
            // disabled. It becomes visible only when UpdateAnimations assigns
            // the adjacent slot on the next rendered frame.
            incoming->SetClass("slot-left", false);
            incoming->SetClass("slot-center", false);
            incoming->SetClass("slot-right", false);
            incoming->SetClass("slot-off-left", direction < 0);
            incoming->SetClass("slot-off-right", direction > 0);
            incoming->SetClass("carousel-snap", true);
        }
        carousel_animation = CarouselAnimation::Positioning;
    }

    void UpdateCarousel(int direction = 0)
    {
        const int count = static_cast<int>(kModels.size());
        const int left = WrapIndex(selected_system - 1, count);
        const int right = WrapIndex(selected_system + 1, count);

        for (int index = 0; index < count; ++index) {
            if (Rml::Element* card = document->GetElementById(kModels[index].id)) {
                const bool hidden = index != left && index != selected_system && index != right;
                const bool outgoing = direction != 0 && index == carousel_outgoing;
                if (direction == 0) {
                    // Initial/restored layouts have no movement to animate.
                    // Keep hidden cards parked with transitions disabled until
                    // one is explicitly selected as the next incoming card.
                    card->SetClass("carousel-snap", hidden);
                } else if (hidden && !outgoing) {
                    card->SetClass("carousel-snap", true);
                }
                card->SetClass("carousel-parked", hidden && !outgoing);
                card->SetClass("slot-left", index == left);
                card->SetClass("slot-center", index == selected_system);
                card->SetClass("slot-right", index == right);
                card->SetClass("slot-off-left", hidden && direction >= 0);
                card->SetClass("slot-off-right", hidden && direction < 0);
                card->SetClass("selected", index == selected_system);
                if (index == selected_system)
                    card->Focus(true);
            }
            if (Rml::Element* dot = document->GetElementById(Rml::CreateString("position-%d", index)))
                dot->SetClass("active", index == selected_system);
        }
    }

    Rml::ElementDocument* document = nullptr;
    const Options& options;
    std::vector<GameInfo> games;
    int selected_system = 1;
    int carousel_direction = 0;
    int carousel_incoming = 0;
    int carousel_outgoing = 0;
    int carousel_queued_steps = 0;
    Uint64 carousel_animation_deadline = 0;
    CarouselAnimation carousel_animation = CarouselAnimation::Idle;
    std::size_t selected_game = 0;
    std::size_t visible_start = 0;
    std::size_t rail_target_start = 0;
    Uint64 rail_animation_started_at = 0;
    Uint64 rail_animation_deadline = 0;
    int rail_animation_from_left = kGameTrackRestingLeft;
    int rail_animation_to_left = kGameTrackRestingLeft;
    RailAnimation rail_animation = RailAnimation::Idle;
    int carousel_initialization_frames = 2;
    bool library_track_initialized = false;
    View view = View::Home;
    bool exit_requested = false;
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
    // The TG5040 stock input daemon exposes the physical A/B buttons with the
    // first-release indices swapped: physical A is SDL B, physical B is SDL A.
    case SDL_CONTROLLER_BUTTON_B: return desktop.Activate();
    case SDL_CONTROLLER_BUTTON_A:
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
        DesktopController desktop(document, options);
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
                    // Match the TG5040 physical labels in raw joystick fallback
                    // mode as well: button 1 is A, button 0 is B.
                    if (event.jbutton.button == 1) {
                        changed = desktop.Activate();
                    } else if (event.jbutton.button == 0) {
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

            if (desktop.ExitRequested())
                running = false;

            if (changed) {
                dirty = true;
                animate_until = SDL_GetPerformanceCounter() + transition_ticks;
            }
        };

        while (running) {
            Uint64 now = SDL_GetPerformanceCounter();
            dirty = desktop.UpdateAnimations() || dirty;

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
                    (now < animate_until || desktop.IsAnimating()) ? "animated" : "idle",
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

            const bool animating = now < animate_until || desktop.IsAnimating();
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
