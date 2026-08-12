#pragma once

#include <string>
#include <vector>

enum class GameSystem {
    GameBoy,
    GameBoyColor,
    GameBoyAdvance,
    SuperNintendo,
    SegaGenesis,
    SegaGameGear,
};

struct GameSystemInfo {
    GameSystem system;
    const char* id;
    const char* title;
    const char* rom_tag;
    const char* core_name;
    const char* console_image;
};

struct GameInfo {
    std::string path;
    std::string title;
    std::string cover;
};

const GameSystemInfo& GetGameSystemInfo(GameSystem system);
std::vector<GameInfo> ScanGames(const std::string& rom_root, GameSystem system);
bool WriteLaunchRequest(
    const std::string& request_path,
    GameSystem system,
    const std::string& rom_path,
    std::string& error);
