#include "terrain/TerrainRenderer.h"
#include "terrain/TerrainSimulator.h"
#include "render/Camera.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"
#include "package/BackgroundDef.h"
#include <algorithm>
#include <cmath>
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
        chunk.debug_texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STREAMING,
            CHUNK_SIZE, CHUNK_SIZE
        );
        SDL_SetTextureBlendMode(chunk.debug_texture, SDL_BLENDMODE_BLEND);
        chunk.dirty = true;
        chunk.debug_dirty = true;
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
        if (chunk.debug_texture) SDL_DestroyTexture(chunk.debug_texture);
    }
}

// Compute an RGBA8888 pixel for the active overlay mode at world coordinate (wx,wy).
// Returns 0x00000000 when no overlay should be drawn for this cell.
uint32_t TerrainRenderer::OverlayPixel(int wx, int wy) const {
    if (!simulator_ || overlay_ == Overlay::None || overlay_ == Overlay::Diagnostics)
        return 0;

    int idx = wy * width_ + wx;

    auto pack = [](uint8_t r, uint8_t g, uint8_t b, uint8_t a) -> uint32_t {
        return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | a;
    };

    switch (overlay_) {
    case Overlay::Heatmap: {
        const float* temp = simulator_->GetTempOverlay();
        if (!temp) return 0;
        float t = temp[idx];
        // Clamp to visible range: [-50, 500]
        float norm = (t + 50.0f) / 550.0f;
        norm = std::max(0.0f, std::min(1.0f, norm));
        // cool blue(0) → green(0.5) → red(1)
        uint8_t r, g, b;
        if (norm < 0.5f) {
            float f = norm * 2.0f;
            r = 0;
            g = static_cast<uint8_t>(f * 255);
            b = static_cast<uint8_t>((1.0f - f) * 255);
        } else {
            float f = (norm - 0.5f) * 2.0f;
            r = static_cast<uint8_t>(f * 255);
            g = static_cast<uint8_t>((1.0f - f) * 255);
            b = 0;
        }
        return pack(r, g, b, 160);
    }
    case Overlay::Health: {
        const int16_t* hp = simulator_->GetHealthOverlay();
        if (!hp) return 0;
        Cell cell = terrain_.GetCell(wx, wy);
        if (registry_) {
            auto* mat = registry_->GetMaterialByRuntimeID(cell.material_id);
            if (!mat || mat->max_health <= 0) return 0;
            float norm = static_cast<float>(hp[idx]) / static_cast<float>(mat->max_health);
            norm = std::max(0.0f, std::min(1.0f, norm));
            // white(1) → red(0.5) → black(0)
            uint8_t r, g, b;
            if (norm > 0.5f) {
                float f = (norm - 0.5f) * 2.0f;
                r = 255; g = static_cast<uint8_t>(f * 255); b = g;
            } else {
                float f = norm * 2.0f;
                r = static_cast<uint8_t>(f * 255); g = 0; b = 0;
            }
            return pack(r, g, b, 160);
        }
        return 0;
    }
    case Overlay::Crack: {
        uint8_t cr = simulator_->GetCrack(wx, wy, width_);
        if (cr == 0) return 0;
        uint8_t grey = static_cast<uint8_t>(40);
        uint8_t a = static_cast<uint8_t>(std::min(255, static_cast<int>(cr)));
        return pack(grey, grey, grey, a);
    }
    case Overlay::Stain: {
        uint8_t sa = simulator_->GetStainA(idx);
        if (sa == 0) return 0;
        uint8_t sr = simulator_->GetStainR(idx);
        uint8_t sg = simulator_->GetStainG(idx);
        uint8_t sb = simulator_->GetStainB(idx);
        return pack(sr, sg, sb, sa);
    }
    default:
        return 0;
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

                // Apply stain overlay from simulator (always active — part of the base render)
                if (simulator_ && overlay_ != Overlay::Stain) {
                    int sidx = wy * width_ + wx;
                    uint8_t sa = simulator_->GetStainA(sidx);
                    if (sa > 0) {
                        float a = sa / 255.0f;
                        uint8_t sr = simulator_->GetStainR(sidx);
                        uint8_t sg = simulator_->GetStainG(sidx);
                        uint8_t sb = simulator_->GetStainB(sidx);
                        uint8_t br = (pixel >> 24) & 0xFF;
                        uint8_t bgg = (pixel >> 16) & 0xFF;
                        uint8_t bb = (pixel >>  8) & 0xFF;
                        br = static_cast<uint8_t>(br * (1.f - a) + sr * a);
                        bgg = static_cast<uint8_t>(bgg * (1.f - a) + sg * a);
                        bb = static_cast<uint8_t>(bb * (1.f - a) + sb * a);
                        pixel = ((uint32_t)br << 24) | ((uint32_t)bgg << 16) |
                                ((uint32_t)bb <<  8) | 0xFF;
                    }
                }

                // Apply visualization overlay (heatmap, health, crack, stain isolation)
                uint32_t ovl = OverlayPixel(wx, wy);
                if (ovl != 0) {
                    float oa = static_cast<float>(ovl & 0xFF) / 255.0f;
                    uint8_t or_ = (ovl >> 24) & 0xFF;
                    uint8_t og  = (ovl >> 16) & 0xFF;
                    uint8_t ob  = (ovl >>  8) & 0xFF;
                    uint8_t br = (pixel >> 24) & 0xFF;
                    uint8_t bgg = (pixel >> 16) & 0xFF;
                    uint8_t bb = (pixel >>  8) & 0xFF;
                    br = static_cast<uint8_t>(br * (1.f - oa) + or_ * oa);
                    bgg = static_cast<uint8_t>(bgg * (1.f - oa) + og * oa);
                    bb = static_cast<uint8_t>(bb * (1.f - oa) + ob * oa);
                    pixel = ((uint32_t)br << 24) | ((uint32_t)bgg << 16) |
                            ((uint32_t)bb <<  8) | 0xFF;
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
        chunk.debug_dirty = true;
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
            auto& chunk = chunks_[ChunkIndex(cx, cy)];
            chunk.dirty = true;
            chunk.debug_dirty = true;
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

            // Clamp source rect for edge chunks
            int src_w = std::min(CHUNK_SIZE, width_ - cx * CHUNK_SIZE);
            int src_h = std::min(CHUNK_SIZE, height_ - cy * CHUNK_SIZE);

            // Compute right/bottom edge from world coords to avoid 1px gaps between chunks
            int sx2 = static_cast<int>((world_x + src_w - cam_x) * scale);
            int sy2 = static_cast<int>((world_y + src_h - cam_y) * scale);

            SDL_Rect src = {0, 0, src_w, src_h};
            SDL_Rect dst = {sx, sy, sx2 - sx, sy2 - sy};
            SDL_RenderCopy(renderer, chunks_[idx].texture, &src, &dst);
        }
    }
}

void TerrainRenderer::RebuildDebugChunk(SDL_Renderer* /*renderer*/, int cx, int cy) {
    int idx = ChunkIndex(cx, cy);
    Chunk& chunk = chunks_[idx];
    if (!chunk.debug_dirty || !chunk.debug_texture) return;

    void* pixels_raw;
    int pitch;
    if (SDL_LockTexture(chunk.debug_texture, nullptr, &pixels_raw, &pitch) != 0) {
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
                // Solid/powder cells get semi-transparent red overlay
                pixel = is_air_lut_[cell.material_id] ? 0x00000000 : 0xFF000060;
            } else {
                pixel = 0x00000000;
            }
            pixels[ly * pitch_pixels + lx] = pixel;
        }
    }

    SDL_UnlockTexture(chunk.debug_texture);
    chunk.debug_dirty = false;
}

void TerrainRenderer::RenderDebugOverlay(SDL_Renderer* renderer, const Camera& camera) {
    float cam_x = camera.GetX();
    float cam_y = camera.GetY();
    int view_w = camera.GetViewWorldWidth();
    int view_h = camera.GetViewWorldHeight();
    float scale = camera.GetScale();

    int cx0 = std::max(0, static_cast<int>(cam_x) / CHUNK_SIZE);
    int cy0 = std::max(0, static_cast<int>(cam_y) / CHUNK_SIZE);
    int cx1 = std::min(chunks_x_ - 1, static_cast<int>(cam_x + view_w) / CHUNK_SIZE);
    int cy1 = std::min(chunks_y_ - 1, static_cast<int>(cam_y + view_h) / CHUNK_SIZE);

    // Render collision overlay textures
    for (int cy = cy0; cy <= cy1; cy++) {
        for (int cx = cx0; cx <= cx1; cx++) {
            int idx = ChunkIndex(cx, cy);

            if (chunks_[idx].debug_dirty) {
                RebuildDebugChunk(renderer, cx, cy);
            }

            float world_x = static_cast<float>(cx * CHUNK_SIZE);
            float world_y = static_cast<float>(cy * CHUNK_SIZE);

            int sx, sy;
            camera.WorldToScreen(world_x, world_y, sx, sy);

            int src_w = std::min(CHUNK_SIZE, width_ - cx * CHUNK_SIZE);
            int src_h = std::min(CHUNK_SIZE, height_ - cy * CHUNK_SIZE);
            int sx2 = static_cast<int>((world_x + src_w - cam_x) * scale);
            int sy2 = static_cast<int>((world_y + src_h - cam_y) * scale);

            SDL_Rect src = {0, 0, src_w, src_h};
            SDL_Rect dst = {sx, sy, sx2 - sx, sy2 - sy};
            SDL_RenderCopy(renderer, chunks_[idx].debug_texture, &src, &dst);
        }
    }

    // Draw chunk border lines (green)
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 120);

    // Vertical lines
    for (int cx = cx0; cx <= cx1 + 1; cx++) {
        int sx = static_cast<int>((cx * CHUNK_SIZE - cam_x) * scale);
        int sy_top = static_cast<int>((cy0 * CHUNK_SIZE - cam_y) * scale);
        int sy_bot = static_cast<int>(((cy1 + 1) * CHUNK_SIZE - cam_y) * scale);
        SDL_RenderDrawLine(renderer, sx, sy_top, sx, sy_bot);
    }

    // Horizontal lines
    for (int cy = cy0; cy <= cy1 + 1; cy++) {
        int sy = static_cast<int>((cy * CHUNK_SIZE - cam_y) * scale);
        int sx_left = static_cast<int>((cx0 * CHUNK_SIZE - cam_x) * scale);
        int sx_right = static_cast<int>(((cx1 + 1) * CHUNK_SIZE - cam_x) * scale);
        SDL_RenderDrawLine(renderer, sx_left, sy, sx_right, sy);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
