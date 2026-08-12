#include "game_catalog.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>

namespace {

constexpr std::array<GameSystemInfo, 5> kSystems = {{
    {GameSystem::GameBoy, "GB", "GAME BOY", "GB", "GAMBATTE", "icons/gb-dmg-simple.png"},
    {GameSystem::GameBoyColor, "GBC", "GAME BOY COLOR", "GBC", "GAMBATTE", "icons/gbc-atomic-purple-simple.png"},
    {GameSystem::GameBoyAdvance, "GBA", "GAME BOY ADVANCE", "GBA", "GPSP", "icons/gba-indigo-simple.png"},
    {GameSystem::SuperNintendo, "SFC", "SUPER NINTENDO", "SFC", "SNES9X 2005+", "icons/snes-classic-simple.png"},
    {GameSystem::SegaGenesis, "MD", "SEGA GENESIS", "MD", "PICO DRIVE", "icons/genesis-model1-simple.png"},
}};

std::string JoinPath(const std::string& parent, const std::string& child)
{
    if (parent.empty() || parent.back() == '/')
        return parent + child;
    return parent + "/" + child;
}

bool IsDirectory(const std::string& path)
{
    struct stat status = {};
    return stat(path.c_str(), &status) == 0 && S_ISDIR(status.st_mode);
}

bool IsRegularFile(const std::string& path)
{
    struct stat status = {};
    return stat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode);
}

std::string LowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character;
    });
    return value;
}

std::string Extension(const std::string& name)
{
    const std::string::size_type dot = name.find_last_of('.');
    return dot == std::string::npos ? std::string() : LowerAscii(name.substr(dot));
}

bool HasSupportedExtension(GameSystem system, const std::string& name)
{
    const std::string extension = Extension(name);
    switch (system) {
    case GameSystem::GameBoy: return extension == ".gb" || extension == ".zip";
    case GameSystem::GameBoyColor: return extension == ".gbc" || extension == ".zip";
    case GameSystem::GameBoyAdvance: return extension == ".gba" || extension == ".zip";
    case GameSystem::SuperNintendo: return extension == ".sfc" || extension == ".smc" || extension == ".zip";
    case GameSystem::SegaGenesis:
        return extension == ".md" || extension == ".gen" || extension == ".bin" ||
            extension == ".smd" || extension == ".zip";
    }
    return false;
}

std::string DisplayTitle(const std::string& filename)
{
    const std::string::size_type dot = filename.find_last_of('.');
    std::string title = dot == std::string::npos ? filename : filename.substr(0, dot);
    std::replace(title.begin(), title.end(), '_', ' ');
    return title;
}

std::string FilenameStem(const std::string& filename)
{
    const std::string::size_type dot = filename.find_last_of('.');
    return dot == std::string::npos ? filename : filename.substr(0, dot);
}

std::string CoverForRom(const std::string& rom_directory, const std::string& filename)
{
    // Convention: Roms/<system>/.media/<ROM filename without extension>.png
    // Keeping artwork beside the ROM collection makes the catalog data-driven:
    // adding or renaming a game never requires recompiling the frontend.
    const std::string cover = JoinPath(
        JoinPath(rom_directory, ".media"),
        FilenameStem(filename) + ".png");
    return IsRegularFile(cover) ? cover : std::string();
}

std::string FindSystemDirectory(const std::string& root, const GameSystemInfo& system)
{
    const std::array<std::string, 3> direct_candidates = {{
        JoinPath(root, system.id),
        JoinPath(root, system.title),
        JoinPath(root, std::string("Roms/") + system.id),
    }};
    for (const std::string& candidate : direct_candidates) {
        if (IsDirectory(candidate))
            return candidate;
    }

    DIR* directory = opendir(root.c_str());
    if (!directory)
        return {};

    const std::string tag = std::string("(") + system.rom_tag + ")";
    std::string match;
    while (dirent* entry = readdir(directory)) {
        if (entry->d_name[0] == '.')
            continue;
        const std::string name = entry->d_name;
        const std::string path = JoinPath(root, name);
        if (name.find(tag) != std::string::npos && IsDirectory(path)) {
            match = path;
            break;
        }
    }
    closedir(directory);
    return match;
}

bool HasLineBreak(const std::string& value)
{
    return value.find('\n') != std::string::npos || value.find('\r') != std::string::npos;
}

} // namespace

const GameSystemInfo& GetGameSystemInfo(GameSystem system)
{
    for (const GameSystemInfo& info : kSystems) {
        if (info.system == system)
            return info;
    }
    return kSystems.front();
}

std::vector<GameInfo> ScanGames(const std::string& rom_root, GameSystem system)
{
    std::vector<GameInfo> games;
    const std::string directory_path = FindSystemDirectory(rom_root, GetGameSystemInfo(system));
    if (directory_path.empty())
        return games;

    DIR* directory = opendir(directory_path.c_str());
    if (!directory)
        return games;

    while (dirent* entry = readdir(directory)) {
        if (entry->d_name[0] == '.' || !HasSupportedExtension(system, entry->d_name))
            continue;
        const std::string path = JoinPath(directory_path, entry->d_name);
        if (!IsRegularFile(path))
            continue;
        GameInfo game;
        game.path = path;
        game.title = DisplayTitle(entry->d_name);
        game.cover = CoverForRom(directory_path, entry->d_name);
        games.push_back(std::move(game));
    }
    closedir(directory);

    std::sort(games.begin(), games.end(), [](const GameInfo& left, const GameInfo& right) {
        return LowerAscii(left.title) < LowerAscii(right.title);
    });
    return games;
}

bool WriteLaunchRequest(
    const std::string& request_path,
    GameSystem system,
    const std::string& rom_path,
    std::string& error)
{
    if (request_path.empty() || rom_path.empty() || rom_path.front() != '/' ||
        HasLineBreak(request_path) || HasLineBreak(rom_path)) {
        error = "启动请求路径无效";
        return false;
    }

    const std::string temporary_path = request_path + ".tmp";
    {
        std::ofstream stream(temporary_path, std::ios::binary | std::ios::trunc);
        if (!stream) {
            error = std::string("无法写入启动请求: ") + std::strerror(errno);
            return false;
        }
        stream << GetGameSystemInfo(system).id << '\n' << rom_path << '\n';
        stream.flush();
        if (!stream) {
            error = "启动请求写入不完整";
            std::remove(temporary_path.c_str());
            return false;
        }
    }

    if (std::rename(temporary_path.c_str(), request_path.c_str()) != 0) {
        error = std::string("无法提交启动请求: ") + std::strerror(errno);
        std::remove(temporary_path.c_str());
        return false;
    }
    return true;
}
