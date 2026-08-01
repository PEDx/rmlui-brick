#include "prototype_render_interface.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Log.h>
#include <SDL_image.h>

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
    auto* geometry = new GeometryView;
    geometry->vertices.resize(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const Rml::Vertex& source = vertices[i];
        geometry->vertices[i] = {
            {source.position.x, source.position.y},
            {
                source.colour.red,
                source.colour.green,
                source.colour.blue,
                source.colour.alpha,
            },
            {source.tex_coord.x, source.tex_coord.y},
        };
    }
    if (!indices.empty())
        geometry->indices.assign(indices.data(), indices.data() + indices.size());
    return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry);
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
    const SDL_Vertex* vertices = geometry->vertices.data();

    if (translation.x != 0.0f || translation.y != 0.0f) {
        translated_vertices.resize(vertex_count);
        for (size_t i = 0; i < vertex_count; ++i) {
            translated_vertices[i] = geometry->vertices[i];
            translated_vertices[i].position.x += translation.x;
            translated_vertices[i].position.y += translation.y;
        }
        vertices = translated_vertices.data();
    }

    const int result = SDL_RenderGeometry(
        renderer,
        reinterpret_cast<SDL_Texture*>(texture),
        vertices,
        static_cast<int>(vertex_count),
        geometry->indices.empty() ? nullptr : geometry->indices.data(),
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
    SDL_Surface* loaded = IMG_Load(source.c_str());
    if (!loaded) {
        healthy = false;
        Rml::Log::Message(Rml::Log::LT_ERROR, "IMG_Load failed for %s: %s", source.c_str(), IMG_GetError());
        return {};
    }

    SDL_Surface* surface = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(loaded);
    if (!surface) {
        healthy = false;
        Rml::Log::Message(Rml::Log::LT_ERROR, "Image conversion failed for %s: %s", source.c_str(), SDL_GetError());
        return {};
    }

    if (SDL_MUSTLOCK(surface))
        SDL_LockSurface(surface);
    for (int y = 0; y < surface->h; ++y) {
        auto* row = static_cast<Uint8*>(surface->pixels) + y * surface->pitch;
        for (int x = 0; x < surface->w; ++x) {
            Uint8* pixel = row + x * 4;
            const Uint8 alpha = pixel[3];
            pixel[0] = static_cast<Uint8>((static_cast<unsigned>(pixel[0]) * alpha + 127) / 255);
            pixel[1] = static_cast<Uint8>((static_cast<unsigned>(pixel[1]) * alpha + 127) / 255);
            pixel[2] = static_cast<Uint8>((static_cast<unsigned>(pixel[2]) * alpha + 127) / 255);
        }
    }
    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        surface->w,
        surface->h);
    if (!texture || SDL_UpdateTexture(texture, nullptr, surface->pixels, surface->pitch) != 0) {
        healthy = false;
        Rml::Log::Message(Rml::Log::LT_ERROR, "Image texture upload failed for %s: %s", source.c_str(), SDL_GetError());
        if (texture)
            SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
        return {};
    }

    texture_dimensions = {surface->w, surface->h};
    SDL_FreeSurface(surface);
    SDL_SetTextureBlendMode(texture, premultiplied_alpha);
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
#endif
    return reinterpret_cast<Rml::TextureHandle>(texture);
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
