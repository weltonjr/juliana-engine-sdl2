#include "Game.h"
#include "core/Types.h"
#include "entity/CharacterFSM.h"
#include <cstdio>
#include <cstring>

// Try common macOS font paths; fall back to nullptr (no text drawn)
static TTF_Font* open_system_font(int pt_size) {
    const char* candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/SFNSText.ttf",
        nullptr
    };
    for (int i = 0; candidates[i]; i++) {
        TTF_Font* f = TTF_OpenFont(candidates[i], pt_size);
        if (f) return f;
    }
    return nullptr;
}

// ── constructor / destructor ────────────────────────────────────────────────

Game::Game(int screen_w, int screen_h, SDL_Renderer* renderer)
    : m_screen_w(screen_w),
      m_screen_h(screen_h),
      m_renderer(renderer),
      m_terrain(MAP_CELLS_W, MAP_CELLS_H),
      m_camera(screen_w, screen_h, MAP_PX_W, MAP_PX_H)
{
    m_font = open_system_font(14);
    terrain_generate(m_terrain);
}

Game::~Game() {
    for (int cy = 0; cy < m_terrain.chunks_y(); cy++)
        for (int cx = 0; cx < m_terrain.chunks_x(); cx++) {
            auto& chunk = m_terrain.get_chunk(cx, cy);
            if (chunk.tex_valid) {
                SDL_DestroyTexture(chunk.tex);
                chunk.tex       = nullptr;
                chunk.tex_valid = false;
            }
        }
    if (m_font) { TTF_CloseFont(m_font); m_font = nullptr; }
}

// ── poll_events ─────────────────────────────────────────────────────────────
// Called once per frame from main before update().
// InputManager::poll() drains the event queue; we also honour its quit flag.

void Game::poll_events() {
    m_input.poll();
    if (m_input.quit_requested()) m_quit = true;
}

// ── update ──────────────────────────────────────────────────────────────────

void Game::update(float dt) {
    // F1: toggle debug overlay (rising edge only)
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    bool f1_now = keys[SDL_SCANCODE_F1] != 0;
    bool f2_now = keys[SDL_SCANCODE_F2] != 0;

    if (f1_now && !m_f1_prev) m_debug.toggle();

    if (f2_now && !m_f2_prev) {
        m_free_cam = !m_free_cam;
        if (m_free_cam) {
            Vector2 off   = m_camera.offset();
            m_free_cam_x  = off.x + m_screen_w * 0.5f;
            m_free_cam_y  = off.y + m_screen_h * 0.5f;
        }
    }

    m_f1_prev = f1_now;
    m_f2_prev = f2_now;

    if (!m_free_cam) {
        m_character.update(dt, m_input, m_terrain);
        m_camera.follow(m_character.center(), dt);
    } else {
        update_free_camera(dt);
    }

    // Rebuild GPU textures for any dirty chunks
    m_terrain_renderer.bake_dirty_chunks(m_terrain, m_renderer);
}

void Game::update_free_camera(float dt) {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    if (keys[SDL_SCANCODE_RIGHT]) m_free_cam_x += FREE_CAM_SPEED * dt;
    if (keys[SDL_SCANCODE_LEFT])  m_free_cam_x -= FREE_CAM_SPEED * dt;
    if (keys[SDL_SCANCODE_DOWN])  m_free_cam_y += FREE_CAM_SPEED * dt;
    if (keys[SDL_SCANCODE_UP])    m_free_cam_y -= FREE_CAM_SPEED * dt;
    m_camera.follow({m_free_cam_x, m_free_cam_y}, dt);
}

// ── draw ────────────────────────────────────────────────────────────────────

void Game::draw_text(SDL_Renderer* r, const char* text, int x, int y,
                     uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca) const {
    if (!m_font || !text || !*text) return;
    SDL_Color sc = {cr, cg, cb, ca};
    SDL_Surface* surf = TTF_RenderText_Blended(m_font, text, sc);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;
    int tw, th;
    SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
    SDL_FRect dst = {(float)x, (float)y, (float)tw, (float)th};
    SDL_RenderCopyF(r, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

void Game::draw(SDL_Renderer* renderer) const {
    // Sky: flat sky-blue (gradient requires manual vertical blit; flat is fine for now)
    SDL_SetRenderDrawColor(renderer, 148, 210, 240, 255);
    SDL_RenderClear(renderer);

    Vector2 offset = m_camera.offset();
    m_terrain_renderer.draw(m_terrain, offset, m_screen_w, m_screen_h, renderer);
    m_character.draw(offset, renderer);

    // Debug overlay (drawn on top of everything)
    m_debug.draw(m_terrain, m_character, m_camera,
                 m_screen_w, m_screen_h, renderer, m_font);

    // Minimal HUD (always visible when debug is off)
    if (!m_debug.enabled) {
        const char* mode = m_free_cam ? " [FREE CAM]" : "";
        char hud[80];
        snprintf(hud, sizeof(hud), "A/D: move  SPACE: jump  C: dig%s", mode);
        draw_text(renderer, hud, 10, 10, 255, 255, 255, 200);
        draw_text(renderer, char_state_name(m_character.state()), 10, 28,
                  255, 235, 0, 255);
    }
}
