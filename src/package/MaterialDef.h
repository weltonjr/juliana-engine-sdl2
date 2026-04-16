#pragma once

#include "core/Types.h"
#include <string>
#include <vector>

enum class MaterialState {
    None,
    Solid,
    Powder,
    Liquid,
    Gas
};

// A single temperature-triggered phase-change rule.
// Fires when: above==true && temp > threshold, or above==false && temp < threshold.
struct PhaseChangeRule {
    float       threshold = 0.0f;
    bool        above     = true;   // true = "temp > threshold", false = "temp < threshold"
    std::string into;               // qualified material id to convert to
};

// Fragmentation style for solid materials
enum class FragStyle { None, Grid, Random };

struct MaterialDef {
    std::string id;
    std::string name;
    std::string qualified_id;  // "package:id"

    // Physics
    MaterialState state    = MaterialState::None;
    int   density          = 100;
    float friction         = 0.8f;
    int   hardness         = 30;

    // Visual
    Color color;
    int   color_variation  = 0;
    float transparency     = 0.0f;
    bool  glow             = false;

    // Behavior
    bool gravity             = false;
    bool flammable           = false;
    int  blast_resistance    = 20;
    int  flow_rate           = 0;
    float liquid_drag        = 0.0f;
    float inertial_resistance = 0.0f;  // [0,1] probability powder resists diagonal slide
    int  rise_rate           = 0;
    int  dispersion          = 0;
    int  lifetime            = 0;
    std::string dig_product;
    std::string small_fragment;
    int  min_fragment_pixels = 8;
    int  solidify_ticks      = 0;      // ticks stationary before converting; 0 = never
    std::string solidify_into;         // qualified material id to solidify into

    // Temperature
    float heat_conductivity  = 0.05f;  // fraction of temp-diff conducted to each neighbor/tick
    float ambient_temp       = 0.0f;   // equilibrium temperature (materials cool/heat toward this)
    float combustion_heat    = 0.0f;   // heat added to all 4 neighbors/tick when burning
    float ignition_temp      = -1.0f;  // temperature at which cell ignites; -1 = cannot ignite
    bool  conducts_heat      = true;   // false disables outgoing conduction (e.g. Air)

    // Phase changes (array supports bidirectional: Ice↔Water↔Steam)
    std::vector<PhaseChangeRule> phase_changes;

    // Health & damage
    int         max_health    = 0;     // 0 = indestructible
    std::string death_product;         // material to become on death (empty = Air)
    int         corrode_damage = 0;    // damage dealt to each adjacent cell per tick (acid)
    bool        corrode_self   = false;// whether corrosion also reduces own health

    // Stain (color mark left on neighboring cells or on self)
    Color stain_color;                 // color this material stains others with
    float stain_strength   = 0.0f;    // [0,1] stain alpha applied per contact tick
    float stain_fade_rate  = 0.005f;  // how fast stain alpha decays per tick (0 = permanent)

    // Fragmentation (for solid materials)
    FragStyle   frag_style     = FragStyle::None;
    int         frag_min_pixels = 4;
    int         frag_max_pixels = 64;

    // Scripting hooks (set by PackageLoader when script.lua defines these functions)
    bool        has_on_tick    = false;
    bool        has_on_contact = false;
    bool        has_on_heat    = false;
    std::string script_path;

    // Runtime assigned
    MaterialID runtime_id = 0;
};
