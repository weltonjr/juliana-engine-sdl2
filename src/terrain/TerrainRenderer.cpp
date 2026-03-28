#include "terrain/TerrainRenderer.h"
#include "render/Camera.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"
#include "package/BackgroundDef.h"
#include <algorithm>
#include <cstdio>

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

uint32_t TerrainRenderer::ColorToPixel(Color c) const {
    return (static_cast<uint32_t>(c.r) << 24) |
           (static_cast<uint32_t>(c.g) << 16) |
           (static_cast<uint32_t>(c.b) << 8)  |
           0xFF;
}

Color TerrainRenderer::GetMaterialColor(MaterialID id) const {
    if (registry_) {
        auto* mat = registry_->GetMaterialByRuntimeID(id);
        if (mat) return mat->color;
    }
    if (id < NUM_FALLBACK_COLORS) return fallback_colors[id];
    return {255, 0, 255};
}

Color TerrainRenderer::GetBackgroundColor(BackgroundID id) const {
    if (registry_) {
        auto* bg = registry_->GetBackgroundByRuntimeID(id);
        if (bg) return bg->color;
    }
    return {0, 0, 0};
}

Color TerrainRenderer::GetCellColor(const Cell& cell) const {
    if (registry_) {
        auto* mat = registry_->GetMaterialByRuntimeID(cell.material_id);
        if (mat && mat->state == MaterialState::None) {
            auto* bg = registry_->GetBackgroundByRuntimeID(cell.background_id);
            if (bg && !bg->transparent) {
                return bg->color;
            }
            return mat->color;
        }
    }
    return GetMaterialColor(cell.material_id);
}

TerrainRenderer::TerrainRenderer(SDL_Renderer* renderer, const Terrain& terrain, const DefinitionRegistry* registry)
    : terrain_(terrain), registry_(registry), renderer_(renderer)
    , width_(terrain.GetWidth()), height_(terrain.GetHeight())
{
    chunks_x_ = (width_ + CHUNK_SIZE - 1) / CHUNK_SIZE;
    chunks_y_ = (height_ + CHUNK_SIZE - 1) / CHUNK_SIZE;
    chunks_.resize(chunks_x_ * chunks_y_);

    // Create textures for each chunk
    for (auto& chunk : chunks_) {
        chunk.texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STREAMING,
            CHUNK_SIZE, CHUNK_SIZE
        );
        chunk.dirty = true;
    }

    // Build color LUTs for fast rendering (avoids registry hash lookups in hot loop)
    fg_color_lut_.resize(256, ColorToPixel({255, 0, 255}));
    bg_color_lut_.resize(256, ColorToPixel({0, 0, 0}));
    is_air_lut_.resize(256, false);
    bg_transparent_lut_.resize(256, true);

    if (registry_) {
        for (int i = 0; i < 256; i++) {
            auto* mat = registry_->GetMaterialByRuntimeID(static_cast<MaterialID>(i));
            if (mat) {
                fg_color_lut_[i] = ColorToPixel(mat->color);
                is_air_lut_[i] = (mat->state == MaterialState::None);
            }
            auto* bg = registry_->GetBackgroundByRuntimeID(static_cast<BackgroundID>(i));
            if (bg) {
                bg_color_lut_[i] = ColorToPixel(bg->color);
                bg_transparent_lut_[i] = bg->transparent;
            }
        }
    } else {
        for (int i = 0; i < NUM_FALLBACK_COLORS; i++) {
            fg_color_lut_[i] = ColorToPixel(fallback_colors[i]);
        }
    }

    std::printf("Terrain renderer: %dx%d chunks (%d total)\n", chunks_x_, chunks_y_, chunks_x_ * chunks_y_);
}

TerrainRenderer::~TerrainRenderer() {
    for (auto& chunk : chunks_) {
        if (chunk.texture) SDL_DestroyTexture(chunk.texture);
    }
}

void TerrainRenderer::RebuildChunk(SDL_Renderer* /*renderer*/, int cx, int cy) {
    int idx = ChunkIndex(cx, cy);
    Chunk& chunk = chunks_[idx];
    if (!chunk.dirty || !chunk.texture) return;

    void* pixels_raw;
    int pitch;
    if (SDL_LockTexture(chunk.texture, nullptr, &pixels_raw, &pitch) != 0) {
        return;
    }

    auto* pixels = static_cast<uint32_t*>(pixels_raw);
    int pitch_pixels = pitch / 4;

    int world_x0 = cx * CHUNK_SIZE;
    int world_y0 = cy * CHUNK_SIZE;

    for (int ly = 0; ly < CHUNK_SIZE; ly++) {
        int wy = world_y0 + ly;
        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            int wx = world_x0 + lx;
            uint32_t pixel;
            if (wx < width_ && wy < height_) {
                Cell cell = terrain_.GetCell(wx, wy);
                if (is_air_lut_[cell.material_id] && !bg_transparent_lut_[cell.background_id]) {
                    pixel = bg_color_lut_[cell.background_id];
                } else {
                    pixel = fg_color_lut_[cell.material_id];
                }
            } else {
                pixel = 0x000000FF;  // black for out-of-bounds
            }
            pixels[ly * pitch_pixels + lx] = pixel;
        }
    }

    SDL_UnlockTexture(chunk.texture);
    chunk.dirty = false;
}

void TerrainRenderer::FullRebuild() {
    // Mark all chunks dirty — they'll be rebuilt lazily on next Render
    for (auto& chunk : chunks_) {
        chunk.dirty = true;
    }
}

void TerrainRenderer::UpdateRegion(int rx, int ry, int rw, int rh) {
    // Find which chunks overlap this region and mark them dirty
    int cx0 = std::max(0, rx / CHUNK_SIZE);
    int cy0 = std::max(0, ry / CHUNK_SIZE);
    int cx1 = std::min(chunks_x_ - 1, (rx + rw - 1) / CHUNK_SIZE);
    int cy1 = std::min(chunks_y_ - 1, (ry + rh - 1) / CHUNK_SIZE);

    for (int cy = cy0; cy <= cy1; cy++) {
        for (int cx = cx0; cx <= cx1; cx++) {
            chunks_[ChunkIndex(cx, cy)].dirty = true;
        }
    }
}

void TerrainRenderer::Render(SDL_Renderer* renderer, const Camera& camera) {
    // Determine visible chunk range from camera
    float cam_x = camera.GetX();
    float cam_y = camera.GetY();
    int view_w = camera.GetViewWorldWidth();
    int view_h = camera.GetViewWorldHeight();

    int cx0 = std::max(0, static_cast<int>(cam_x) / CHUNK_SIZE);
    int cy0 = std::max(0, static_cast<int>(cam_y) / CHUNK_SIZE);
    int cx1 = std::min(chunks_x_ - 1, static_cast<int>(cam_x + view_w) / CHUNK_SIZE);
    int cy1 = std::min(chunks_y_ - 1, static_cast<int>(cam_y + view_h) / CHUNK_SIZE);

    float scale = camera.GetScale();

    for (int cy = cy0; cy <= cy1; cy++) {
        for (int cx = cx0; cx <= cx1; cx++) {
            int idx = ChunkIndex(cx, cy);

            // Lazy rebuild dirty chunks only when they become visible
            if (chunks_[idx].dirty) {
                RebuildChunk(renderer, cx, cy);
            }

            // Calculate screen position for this chunk
            float world_x = static_cast<float>(cx * CHUNK_SIZE);
            float world_y = static_cast<float>(cy * CHUNK_SIZE);

            int sx, sy;
            camera.WorldToScreen(world_x, world_y, sx, sy);

            int chunk_screen_size = static_cast<int>(CHUNK_SIZE * scale);

            // Clamp source rect for edge chunks
            int src_w = std::min(CHUNK_SIZE, width_ - cx * CHUNK_SIZE);
            int src_h = std::min(CHUNK_SIZE, height_ - cy * CHUNK_SIZE);

            SDL_Rect src = {0, 0, src_w, src_h};
            SDL_Rect dst = {sx, sy, static_cast<int>(src_w * scale), static_cast<int>(src_h * scale)};
            SDL_RenderCopy(renderer, chunks_[idx].texture, &src, &dst);
        }
    }
}
