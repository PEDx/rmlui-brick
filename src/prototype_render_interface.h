#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <SDL.h>

class PrototypeRenderInterface final : public Rml::RenderInterface {
public:
    explicit PrototypeRenderInterface(SDL_Renderer* renderer);

    void BeginFrame();
    void EndFrame();

    Rml::CompiledGeometryHandle CompileGeometry(
        Rml::Span<const Rml::Vertex> vertices,
        Rml::Span<const int> indices) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
    void RenderGeometry(
        Rml::CompiledGeometryHandle geometry,
        Rml::Vector2f translation,
        Rml::TextureHandle texture) override;

    Rml::TextureHandle LoadTexture(
        Rml::Vector2i& texture_dimensions,
        const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(
        Rml::Span<const Rml::byte> source,
        Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;

    const Rml::String& GetDiagnostics() const { return diagnostics; }
    bool IsHealthy() const { return healthy; }

private:
    struct GeometryView {
        Rml::Span<const Rml::Vertex> vertices;
        Rml::Span<const int> indices;
    };

    SDL_Renderer* renderer = nullptr;
    SDL_BlendMode premultiplied_alpha = SDL_BLENDMODE_NONE;
    SDL_Rect scissor = {};
    bool scissor_enabled = false;
    bool healthy = true;
    bool render_error_reported = false;
    Rml::String diagnostics;
};
