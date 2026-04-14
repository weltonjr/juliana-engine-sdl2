# Terrain Pressure & Gas System

## Overview

The terrain simulator uses a **Hybrid Gravity + Mass Overlay** approach for liquid pressure and gas behavior. Vertical movement (falling, rising) uses direct cell swaps — the same cellular automata approach as powder. Horizontal equalization and pressure-driven flow use a **per-cell mass overlay** stored in a separate array, keeping the core `Cell` struct at 2 bytes.

## Mass Overlay

Each cell has an associated `uint8_t` mass value (0–255) stored in a flat array parallel to the terrain grid. This is **not** part of the `Cell` struct — it lives in `TerrainSimulator` as `std::vector<uint8_t> mass_`.

| Cell type | Default mass | Meaning |
|-----------|-------------|---------|
| Air / Solid / Powder | 0 | Not pressure-simulated |
| Liquid | 255 | Full cell |
| Gas | Lifetime-based | Decrements each tick; cell becomes air at 0 |

### Effective Pressure (Liquids)

A liquid cell's effective pressure combines its own mass with the weight of liquid above it:

```
effective_pressure = mass[x, y] + (cells_of_liquid_above * COLUMN_WEIGHT)
```

Where `COLUMN_WEIGHT = 16` and the scan looks up to 16 cells above. This gives a pressure range of 0–511 for a full column, sufficient to drive equalization.

### Pressure Equalization Algorithm

Runs once per simulation tick, after gravity and diagonal passes, on active chunks only:

```
For each liquid cell (x, y) in active chunks:
    p_self = effective_pressure(x, y)

    For each horizontal neighbor (nx, y):
        if neighbor is air AND p_self > SPAWN_THRESHOLD:
            Spawn liquid at (nx, y) with mass = transfer_amount
            Reduce own mass by transfer_amount
            If own mass <= 0: become air

        else if neighbor is same liquid:
            p_neighbor = effective_pressure(nx, y)
            delta = (p_self - p_neighbor) / 2
            if delta > MIN_TRANSFER:
                mass[x, y] -= delta
                mass[nx, y] += delta
```

Constants:
- `SPAWN_THRESHOLD = 32` — minimum pressure to push liquid into an empty cell
- `MIN_TRANSFER = 2` — minimum delta to avoid oscillation
- `COLUMN_WEIGHT = 16` — pressure contribution per cell of liquid above
- `MAX_COLUMN_SCAN = 16` — how far up to scan for column weight

### U-Tube Behavior

When liquid fills a U-shaped tunnel, the tall side has higher effective pressure than the short side. The equalization pass transfers mass horizontally (through the connecting liquid at the bottom of the U) until both columns reach the same effective pressure. This naturally produces communicating-vessel behavior without flood-fill or BFS.

### Mass Transfer During Movement

When a liquid cell moves (gravity fall, diagonal fall, horizontal spread), its mass value moves with it:
```
mass[dest] = mass[src]
mass[src] = 0  // source becomes air or inherits swapped cell's mass
```

When two liquid cells swap (density settling), their mass values swap too.

### Mass Initialization

- **Map generation**: After terrain generation completes, scan all liquid cells and set `mass = DEFAULT_LIQUID_MASS (255)`.
- **NotifyModified**: When terrain is dug/blasted, initialize mass for any newly exposed liquid cells.
- **Simulation spawns**: When pressure equalization spawns a new liquid cell, it receives the transferred mass amount.

## Gas Simulation

Gas is a new material state that behaves as the inverse of powder/liquid: it rises instead of falling.

### Material Properties

| Property | Type | Description |
|----------|------|-------------|
| `rise_rate` | int | Upward movement passes per tick (like `flow_rate` for liquids) |
| `dispersion` | int | Horizontal spread passes per tick |
| `lifetime` | int | Simulation ticks before dissipation (0 = permanent) |

### Gas Movement (per tick)

Scanned **top-to-bottom** (opposite of powder/liquid) within active chunks:

1. **Rise**: If cell above is air → swap up. If cell above is lighter gas (lower density) → swap up.
2. **Diagonal-up**: If blocked above, try diagonal-up-left or diagonal-up-right (alternating with `(x+y)&1`). Both the diagonal cell and the side cell must be air or lighter gas.
3. **Horizontal dispersion**: If blocked above and diagonally, spread sideways into air cells.

The `rise_rate` controls how many upward movement passes run per tick (same multi-pass approach as liquid `flow_rate`). The `dispersion` controls horizontal spread passes.

### Gas Dissipation

Gas cells use the mass overlay as a lifetime counter:
- On creation: `mass = lifetime` (from material definition, clamped to 255)
- Each simulation tick: `mass -= 1`
- When `mass == 0`: cell becomes air

Permanent gases (`lifetime = 0` in definition) skip dissipation — their mass stays at `DEFAULT_GAS_MASS`.

### Gas Density Interactions

- Gas does not block powder or liquid movement — they pass through gas as if it were air.
- Gas rises through liquids: if the cell above a gas is liquid, the gas swaps upward (gas always lighter than liquid by convention).
- Multiple gas types settle by density: heavier gas sinks below lighter gas (same density-comparison swap as liquids).

## Processed-Flag Overlay

A separate `std::vector<uint8_t> processed_` array prevents cells from being simulated multiple times per tick. Cleared with `memset` at the start of each simulation step. Before moving any cell, the simulator checks `processed_[dest]`; after moving, it sets `processed_[dest] = 1`.

This is standard practice in falling-sand simulations and prevents:
- Diagonal/horizontal moves causing a cell to be re-encountered in the same scan
- Liquid spreading cascading within a single tick
- Gas rising multiple times per pass

Cost: 1 byte per cell (~1MB for 2048x512). The `memset` is a single cache-line sweep.

## Performance

| Aspect | Approach |
|--------|----------|
| Mass overlay | Only read/written for active chunks; zero cost for inactive terrain |
| Pressure scan | Column scan capped at 16 cells; O(1) per cell amortized |
| Gas dissipation | Single decrement per gas cell per tick |
| Processed flag | `memset` once per tick; byte-access is cache-friendlier than bit-packing |
| Chunk activation | Gas rising activates chunk above (symmetric with powder activating chunk below) |
| Multi-pass flow | Water gets 3 passes, lava gets 1 — controlled by `flow_rate` property |

## Simulation Order (Updated)

```
1. Clear processed flags (memset)
2. Initialize mass for newly created liquid/gas cells
3. Simulate powder (bottom-to-top) — gravity, diagonal pile
4. Simulate liquid (bottom-to-top, multi-pass for flow_rate):
   - Gravity fall into air
   - Density settling (heavier sinks through lighter)
   - Diagonal-down fall
   - Horizontal spread (one cell per pass)
   - Mass transfer with each movement
5. Simulate gas (top-to-bottom, multi-pass for rise_rate):
   - Rise into air or lighter gas
   - Diagonal-up spread
   - Horizontal dispersion
   - Dissipation (mass decrement)
6. Equalize pressure (horizontal mass transfer between liquid cells)
7. Aggregate dirty rects for renderer
```
