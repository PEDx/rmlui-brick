#include "prototype_render_interface.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Log.h>

#include <memory>

PrototypeRenderInterface::PrototypeRenderInterface(SDL_Renderer* renderer) : renderer(renderer)
{
    premultiplied_alpha = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        SDL_BLENDOPERATION_ADD);

    SDL_RendererInfo info = {};
    SDL_version runtime_version = {};
    SDL_GetVersion(&runtime_version);
    if (SDL_GetRendererInfo(renderer, &info) == 0) {
        diagnostics = Rml::CreateString(
            "%s · SDL %u.%u.%u · mode=%s · max texture=%dx%d",
            info.name ? info.name : "unknown",
            runtime_version.major,
            runtime_version.minor,
            runtime_version.patch,
            (info.flags & SDL_RENDERER_ACCELERATED) ? "accelerated" : "software",
            info.max_texture_width,
            info.max_texture_height);
    } else {
        diagnostics = Rml::CreateString("renderer query failed: %s", SDL_GetError());
    }

    SDL_SetRenderDrawBlendMode(renderer, premultiplied_alpha);
}

void PrototypeRenderInterface::BeginFrame()
{
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_SetRenderDrawColor(renderer, 9, 13, 24, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, premultiplied_alpha);
}

void PrototypeRenderInterface::EndFrame()
{
    SDL_RenderPresent(renderer);
}

Rml::CompiledGeometryHandle PrototypeRenderInterface::CompileGeometry(
    Rml::Span<const Rml::Vertex> vertices,
    Rml::Span<const int> indices)
{
    return reinterpret_cast<Rml::CompiledGeometryHandle>(new GeometryView{vertices, indices});
}

void PrototypeRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry)
{
    delete reinterpret_cast<GeometryView*>(geometry);
}

void PrototypeRenderInterface::RenderGeometry(
    Rml::CompiledGeometryHandle handle,
    Rml::Vector2f translation,
    Rml::TextureHandle texture)
{
    const GeometryView* geometry = reinterpret_cast<const GeometryView*>(handle);
    const size_t vertex_count = geometry->vertices.size();
    std::unique_ptr<SDL_Vertex[]> vertices(new SDL_Vertex[vertex_count]);

    for (size_t i = 0; i < vertex_count; ++i) {
        const Rml::Vertex& source = geometry->vertices[i];
        vertices[i].position = {
            source.position.x + translation.x,
            source.position.y + translation.y,
        };
        vertices[i].color = {
            source.colour.red,
            source.colour.green,
            source.colour.blue,
            source.colour.alpha,
        };
        vertices[i].tex_coord = {source.tex_coord.x, source.tex_coord.y};
    }

    const int result = SDL_RenderGeometry(
        renderer,
        reinterpret_cast<SDL_Texture*>(texture),
        vertices.get(),
        static_cast<int>(vertex_count),
        geometry->indices.data(),
        static_cast<int>(geometry->indices.size()));

    if (result != 0 && !render_error_reported) {
        render_error_reported = true;
        healthy = false;
        Rml::Log::Message(Rml::Log::LT_ERROR, "SDL_RenderGeometry failed: %s", SDL_GetError());
    }
}

Rml::TextureHandle PrototypeRenderInterface::LoadTexture(
    Rml::Vector2i& texture_dimensions,
    const Rml::String& source)
{
    texture_dimensions = {};
    Rml::Log::Message(
        Rml::Log::LT_WARNING,
        "Image loading is intentionally disabled in the minimum prototype: %s",
        source.c_str());
    return {};
}

Rml::TextureHandle PrototypeRenderInterface::GenerateTexture(
    Rml::Span<const Rml::byte> source,
    Rml::Vector2i source_dimensions)
{
    if (!source.data() || source.size() != static_cast<size_t>(source_dimensions.x * source_dimensions.y * 4)) {
        healthy = false;
        Rml::Log::Message(Rml::Log::LT_ERROR, "Invalid RmlUi texture data");
        return {};
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        source_dimensions.x,
        source_dimensions.y);
    if (!texture) {
        healthy = false;
        Rml::Log::Message(Rml::Log::LT_ERROR, "SDL_CreateTexture failed: %s", SDL_GetError());
        return {};
    }

    if (SDL_UpdateTexture(texture, nullptr, source.data(), source_dimensions.x * 4) != 0) {
        healthy = false;
        Rml::Log::Message(Rml::Log::LT_ERROR, "Font texture upload failed: %s", SDL_GetError());
        SDL_DestroyTexture(texture);
        return {};
    }

    if (SDL_SetTextureBlendMode(texture, premultiplied_alpha) != 0) {
        healthy = false;
        Rml::Log::Message(
            Rml::Log::LT_WARNING,
            "Premultiplied alpha is unsupported (%s); using standard alpha for visibility",
            SDL_GetError());
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }

#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
#endif

    return reinterpret_cast<Rml::TextureHandle>(texture);
}

void PrototypeRenderInterface::ReleaseTexture(Rml::TextureHandle texture)
{
    SDL_DestroyTexture(reinterpret_cast<SDL_Texture*>(texture));
}

void PrototypeRenderInterface::EnableScissorRegion(bool enable)
{
    scissor_enabled = enable;
    SDL_RenderSetClipRect(renderer, enable ? &scissor : nullptr);
}

void PrototypeRenderInterface::SetScissorRegion(Rml::Rectanglei region)
{
    scissor = {region.Left(), region.Top(), region.Width(), region.Height()};
    if (scissor_enabled)
        SDL_RenderSetClipRect(renderer, &scissor);
}
