#pragma once

#include "terrain/Terrain.h"
#include "package/MaterialDef.h"
#include <vector>
#include <cstdint>

class DynamicBodyManager;
class DefinitionRegistry;

// FragmentTracker — manages crack accumulation on solid cells and detects when
// an isolated region has been fully cracked off, converting it to a DynamicBody.
//
// Cracks are stored externally in TerrainSimulator::crack_ overlay.  This class
// provides the isolation-detection logic that triggers DynamicBodyManager.
class FragmentTracker {
public:
    FragmentTracker(const DefinitionRegistry& registry);

    // Apply damage to cell (sx, sy), propagating cracks radially.
    // Crack data lives in the external crack[] array owned by TerrainSimulator.
    // After crack propagation, scans for isolated fragments and, if found,
    // calls dbm.ScanForFloatingGroups on the affected region.
    void ApplyDamage(Terrain& terrain,
                     uint8_t* crack,  // terrain-sized crack overlay
                     DynamicBodyManager& dbm,
                     int sx, int sy, int damage);

    // Returns the crack threshold above which a cell is considered "fully cracked".
    static constexpr int CRACK_THRESHOLD = 200;

private:
    // Flood-fill from (sx,sy), following the path of high-crack cells, to find
    // a closed boundary and extract an isolated fragment region.
    bool FindIsolatedFragment(const Terrain& terrain,
                              const uint8_t* crack,
                              int sx, int sy,
                              std::vector<std::pair<int,int>>& fragment_cells) const;

    const DefinitionRegistry& registry_;
    std::vector<bool>          solid_lut_;
};
