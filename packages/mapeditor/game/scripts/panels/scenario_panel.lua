-- Scenario properties panel for the map editor.
-- Drives the generic property_panel with MapConfig property groups.
--
-- Adding a new shape type = add it to the "shape" options list.
-- Adding new shape params = add entries to the "Shape Params" group.
-- Zero C++ changes required.

local PropertyPanel = require("panels/property_panel")

local M = {}

-- Default material rules for common shapes
local DEFAULT_MATERIALS = {
    { id = "base:Air",   rule = "above_surface" },
    { id = "base:Dirt",  rule = "surface_layer", depth = 30 },
    { id = "base:Rock",  rule = "deep",          min_depth = 60 },
    { id = "base:Rock",  rule = "fill" },
}

-- Default feature set
local DEFAULT_FEATURES = {
    { type = "caves",     density = 0.05, min_size = 10, max_size = 40 },
    { type = "ore_veins", material = "base:GoldOre", zone = "rock",
      density = 0.03, vein_radius = 8 },
}

-- Build the property groups definition.
-- Extend this table to expose new generator options to the editor.
local function make_groups()
    return {
        {
            label = "Map",
            props = {
                { key = "width",  label = "Width",  type = "int",
                  value = 2048, min = 256, max = 8192, step = 256 },
                { key = "height", label = "Height", type = "int",
                  value = 512,  min = 128, max = 2048, step = 128 },
                { key = "seed",   label = "Seed",   type = "int",
                  value = 0,    min = 0,   max = 999999999 },
                { key = "shape",  label = "Shape",  type = "enum",
                  value = "flat",
                  options = { "flat", "island", "mountain", "bowl" } },
            },
        },
        {
            label = "Shape Params",
            props = {
                { key = "roughness",     label = "Roughness",  type = "float",
                  value = 0.5,  min = 0.0, max = 1.0, step = 0.05 },
                { key = "surface_level", label = "Surface",    type = "float",
                  value = 0.35, min = 0.1, max = 0.9, step = 0.05 },
                { key = "sea_level",     label = "Sea Level",  type = "float",
                  value = 0.6,  min = 0.1, max = 0.9, step = 0.05 },
                { key = "terrain_height",label = "Terrain H",  type = "float",
                  value = 0.4,  min = 0.1, max = 0.9, step = 0.05 },
            },
        },
        {
            label = "Scenario",
            props = {
                { key = "name",        label = "Name",        type = "string", value = "New Map" },
                { key = "description", label = "Description", type = "string", value = "" },
            },
        },
    }
end

-- Build the panel UI inside parent_frame.
-- on_regenerate(config_table) is called when the "Generate" button is clicked.
-- Returns a handle with get_config(), set_from_scenario(tbl), handle.panel.
function M.build(parent_frame, on_regenerate)
    local W = 290
    local groups = make_groups()

    -- Build property panel leaving room for the Generate button at bottom
    local panel_handle = PropertyPanel.create(parent_frame, groups, function(key, val)
        -- Live update — no auto-regen (wait for Generate button)
    end)

    local btn_y = panel_handle.content_height + 8
    local gen_btn = parent_frame:add_button(
        "Generate", 6, btn_y, W - 12, 28)

    gen_btn:on_click(function()
        local v = panel_handle.get_values()
        on_regenerate(M.build_map_config(v))
    end)

    local handle = {}

    -- Assemble a map config table from current panel values
    function handle.get_config()
        local v = panel_handle.get_values()
        return M.build_map_config(v)
    end

    -- Returns the full scenario table (for save)
    function handle.get_scenario_table()
        local v = panel_handle.get_values()
        return {
            scenario = {
                id          = v.name and v.name:lower():gsub("%s+", "_") or "untitled",
                name        = v.name or "New Map",
                description = v.description or "",
            },
            map = M.build_map_config(v),
        }
    end

    -- Update the displayed seed value (used to fix seed=0 before locking)
    function handle.set_seed(v)
        panel_handle.set_values({ seed = v })
    end

    -- Lock/unlock the seed field to prevent accidental changes after manual edits
    function handle.set_seed_locked(locked)
        panel_handle.disable_field("seed", locked)
    end

    -- Populate panel from a loaded scenario table
    function handle.set_from_scenario(tbl)
        local flat = {}
        if tbl.scenario then
            flat.name        = tbl.scenario.name or "New Map"
            flat.description = tbl.scenario.description or ""
        end
        if tbl.map then
            flat.width        = tbl.map.width
            flat.height       = tbl.map.height
            flat.seed         = tbl.map.seed
            flat.shape        = tbl.map.shape
            if tbl.map.shape_params then
                flat.roughness      = tbl.map.shape_params.roughness
                flat.surface_level  = tbl.map.shape_params.surface_level
                flat.sea_level      = tbl.map.shape_params.sea_level
                flat.terrain_height = tbl.map.shape_params.terrain_height
            end
        end
        panel_handle.set_values(flat)
    end

    return handle
end

-- Build a map config table from flat property values
function M.build_map_config(v)
    return {
        width  = v.width  or 2048,
        height = v.height or 512,
        seed   = v.seed   or 0,
        shape  = v.shape  or "flat",
        shape_params = {
            roughness      = v.roughness      or 0.5,
            surface_level  = v.surface_level  or 0.35,
            sea_level      = v.sea_level      or 0.6,
            terrain_height = v.terrain_height or 0.4,
        },
        materials = DEFAULT_MATERIALS,
        features  = DEFAULT_FEATURES,
    }
end

return M
