#include "terrain/TerrainRenderer.h"
#include "render/Camera.h"
#include "package/DefinitionRegistry.h"
#include <stdexcept>

// Fallback colors when no registry is available
static const Color fallback_colors[] = {
    {135, 206, 235},   // 0: Air (sky blue)
    {139, 119, 101},   // 1: Dirt
    {128, 128, 128},   // 2: Rock
    {210, 190, 140},   // 3: Sand
    {200, 170,  50},   // 4: GoldOre
    { 60, 100, 200},   // 5: Water
};
static constexpr int NUM_FALLBACK_COLORS = sizeof(fallback_colors) / sizeof(fallback_colors[0]);

Color TerrainRenderer::GetMaterialColor(MaterialID id) const {
    if (registry_) {
        auto* mat = registry_->GetMaterialByRuntimeID(id);
        if (mat) return mat->color;
    }
    if (id < NUM_FALLBACK_COLORS) return fallback_colors[id];
    return {255, 0, 255};
}

TerrainRenderer::TerrainRenderer(SDL_Renderer* renderer, const Terrain& terrain, const DefinitionRegistry* registry)
    : terrain_(terrain), registry_(registry), width_(terrain.GetWidth()), height_(terrain.GetHeight())
{
    texture_ = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        width_, height_
    );
    if (!texture_) {
        throw std::runtime_error(std::string("Failed to create terrain texture: ") + SDL_GetError());
    }
}

TerrainRenderer::~TerrainRenderer() {
    if (texture_) SDL_DestroyTexture(texture_);
}

void TerrainRenderer::FullRebuild() {
    void* pixels_raw;
    int pitch;
    if (SDL_LockTexture(texture_, nullptr, &pixels_raw, &pitch) != 0) {
        return;
    }

    auto* pixels = static_cast<uint32_t*>(pixels_raw);
    int pitch_pixels = pitch / 4;

    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            Cell cell = terrain_.GetCell(x, y);
            Color c = GetMaterialColor(cell.material_id);
            pixels[y * pitch_pixels + x] =
                (static_cast<uint32_t>(c.r) << 24) |
                (static_cast<uint32_t>(c.g) << 16) |
                (static_cast<uint32_t>(c.b) << 8)  |
                0xFF;
        }
    }

    SDL_UnlockTexture(texture_);
}

void TerrainRenderer::Render(SDL_Renderer* renderer, const Camera& camera) {
    SDL_Rect src = camera.GetSourceRect(width_, height_);
    SDL_Rect dst = {0, 0, camera.GetViewportWidth(), camera.GetViewportHeight()};
    SDL_RenderCopy(renderer, texture_, &src, &dst);
}
