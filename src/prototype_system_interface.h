#pragma once

#include <RmlUi/Core/SystemInterface.h>

class PrototypeSystemInterface final : public Rml::SystemInterface {
public:
    double GetElapsedTime() override;
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
};
