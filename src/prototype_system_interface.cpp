#include "prototype_system_interface.h"

#include <SDL.h>

#include <cstdio>

double PrototypeSystemInterface::GetElapsedTime()
{
    return static_cast<double>(SDL_GetPerformanceCounter()) /
        static_cast<double>(SDL_GetPerformanceFrequency());
}

bool PrototypeSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message)
{
    const char* level = "INFO";
    switch (type) {
    case Rml::Log::LT_ALWAYS: level = "ALWAYS"; break;
    case Rml::Log::LT_ERROR: level = "ERROR"; break;
    case Rml::Log::LT_ASSERT: level = "ASSERT"; break;
    case Rml::Log::LT_WARNING: level = "WARN"; break;
    case Rml::Log::LT_INFO: level = "INFO"; break;
    case Rml::Log::LT_DEBUG: level = "DEBUG"; break;
    case Rml::Log::LT_MAX: level = "UNKNOWN"; break;
    }
    std::fprintf(stderr, "[RmlUi/%s] %s\n", level, message.c_str());
    std::fflush(stderr);
    return true;
}

void PrototypeSystemInterface::JoinPath(
    Rml::String& translated_path,
    const Rml::String& document_path,
    const Rml::String& path)
{
    // RmlUi's default implementation strips the leading slash because many
    // integrations use a virtual resource root. This frontend uses native
    // filesystem paths for ROM-side artwork, so preserve absolute paths.
    if (!path.empty() && path.front() == '/') {
        translated_path = path;
        return;
    }
    Rml::SystemInterface::JoinPath(translated_path, document_path, path);
}
