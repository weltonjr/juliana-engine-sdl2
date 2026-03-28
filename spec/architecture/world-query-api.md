# World Query API

Scenario aspects and entity scripts can query the world for positions and spatial information. This supports wildlife spawning, AI navigation, and dynamic placement.

## Position Queries

```lua
-- Find a position matching zone + constraints
-- Returns {x, y} or nil if no match found
local pos = FindSpawnPosition(zone, constraints)

-- Find multiple positions matching zone + constraints
-- Returns array of {x, y}
local positions = FindSpawnPositions(zone, constraints, count)
```

## Terrain Queries

```lua
-- Get material at world pixel position
local material = GetMaterial(x, y)

-- Get depth from nearest surface (positive = underground)
local depth = GetDepthFromSurface(x, y)

-- Get surface Y at a column (highest solid cell)
local surface_y = GetSurfaceY(x)

-- Check if a rectangle is clear of solid terrain
local clear = IsOpenSpace(x, y, width, height)
```

## Spatial Queries

```lua
-- Find entities within radius, optionally filtered by category
local entities = FindEntities(x, y, radius, category)

-- Find nearest entity matching category
local id, dist = FindNearest(x, y, category)

-- Distance from position to nearest player
local dist = GetNearestPlayerDistance(x, y)

-- Find nearest cave (open underground space)
local cave = FindNearestCave(x, y, search_radius)
-- Returns {x, y, width, height} of the cave bounds, or nil
```
