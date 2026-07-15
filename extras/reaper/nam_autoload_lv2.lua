-- METAL NAM GEAR PLAYER — Reaper autoload helper (Linux / LV2)
--
-- Inserts a new track, adds the "NAM Custom" LV2 plugin, prompts the user
-- for a .nam model (and optionally an IR .wav), then attempts to preload
-- them into the plugin. If the plugin does not accept programmatic path
-- injection, the chosen paths are copied to the system clipboard as a
-- fallback and a dialog explains where to paste them.
--
-- Usage:
--   Reaper GUI: Actions → Load ReaScript → nam_autoload_lv2.lua → Run
--   CLI:        reaper -nonewinst extras/reaper/nam_autoload_lv2.lua
--
-- Requires: Reaper 6.x+, "NAM Custom.lv2" installed under ~/.lv2/ or
-- another standard LV2 path scanned by Reaper.

local EXT_NS      = "nam_autoload"
local FX_CANDS    = {
  "LV2: NAM Custom (fabionet)",
  "LV2: NAM Custom",
  "NAM Custom",
}
local NAM_DEFAULT = os.getenv("HOME") .. "/Documenti/NAM-Models"
local IR_DEFAULT  = NAM_DEFAULT

-- Try several documented / experimental named-config keys that different
-- JUCE plugin backends have exposed over the years to receive a file path.
local function try_set_path(track, fx, path)
  local keys = { "model_path", "nam_model_path", "ModelPath",
                 "state_model_path", "userStateModel" }
  for _, k in ipairs(keys) do
    if reaper.TrackFX_SetNamedConfigParm then
      local ok = reaper.TrackFX_SetNamedConfigParm(track, fx, k, path)
      if ok then return true, k end
    end
  end
  return false, nil
end

local function copy_to_clipboard(str)
  if reaper.CF_SetClipboard then           -- SWS extension
    reaper.CF_SetClipboard(str)
    return true
  end
  -- Fallback: shell out to xclip / xsel / wl-copy
  local tmp = os.tmpname()
  local f = io.open(tmp, "w"); if not f then return false end
  f:write(str); f:close()
  local ok = os.execute("(command -v wl-copy >/dev/null && wl-copy < " .. tmp ..
    ") || (command -v xclip >/dev/null && xclip -selection clipboard < " .. tmp ..
    ") || (command -v xsel >/dev/null && xsel -bi < " .. tmp .. ")")
  os.remove(tmp)
  return ok == true or ok == 0
end

local function file_exists(p)
  local f = io.open(p, "r"); if f then f:close(); return true end
  return false
end

local last_nam = reaper.GetExtState(EXT_NS, "last_nam")
local last_ir  = reaper.GetExtState(EXT_NS, "last_ir")
local nam_seed = last_nam ~= "" and last_nam or (NAM_DEFAULT .. "/")
local ir_seed  = last_ir  ~= "" and last_ir  or (IR_DEFAULT .. "/")

local ok, nam_path = reaper.GetUserFileNameForRead(nam_seed,
  "Pick a .nam model", ".nam")
if not ok then return end
if not file_exists(nam_path) then
  reaper.ShowMessageBox("File not found:\n" .. nam_path,
    "NAM autoload", 0)
  return
end

local want_ir = reaper.ShowMessageBox("Also load an IR cabinet (.wav)?",
  "NAM autoload", 4) == 6
local ir_path = ""
if want_ir then
  local ok2, chosen = reaper.GetUserFileNameForRead(ir_seed,
    "Pick an IR .wav", ".wav")
  if ok2 and file_exists(chosen) then ir_path = chosen end
end

reaper.PreventUIRefresh(1)
reaper.Undo_BeginBlock()
reaper.InsertTrackAtIndex(0, true)
local track = reaper.GetTrack(0, 0)
reaper.GetSetMediaTrackInfo_String(track, "P_NAME", "NAM Custom (LV2)", true)

local fx = -1
for _, n in ipairs(FX_CANDS) do
  fx = reaper.TrackFX_AddByName(track, n, false, -1)
  if fx >= 0 then break end
end
if fx < 0 then
  reaper.PreventUIRefresh(-1)
  reaper.Undo_EndBlock("NAM autoload (failed: LV2 not found)", -1)
  reaper.ShowMessageBox(
    "NAM Custom LV2 was not found in Reaper's plug-in cache.\n\n" ..
    "Make sure NAM Custom.lv2 is installed under ~/.lv2/ (or another LV2\n" ..
    "path Reaper scans), then Options → Preferences → Plug-ins → LV2 → Re-scan.",
    "NAM autoload", 0)
  return
end

reaper.SetExtState(EXT_NS, "last_nam", nam_path, true)
if ir_path ~= "" then reaper.SetExtState(EXT_NS, "last_ir", ir_path, true) end

local nam_ok, nam_key = try_set_path(track, fx, nam_path)
local ir_ok, ir_key = false, nil
if ir_path ~= "" then
  local keys = { "ir_path", "IRPath", "cab_ir_path" }
  for _, k in ipairs(keys) do
    if reaper.TrackFX_SetNamedConfigParm(track, fx, k, ir_path) then
      ir_ok, ir_key = true, k; break
    end
  end
end

reaper.TrackFX_Show(track, fx, 3)   -- open FX chain + show plugin UI
reaper.PreventUIRefresh(-1)
reaper.Undo_EndBlock("NAM autoload (LV2)", -1)
reaper.TrackList_AdjustWindows(false)
reaper.UpdateArrange()

if nam_ok then
  local msg = "Model loaded automatically (key = '" .. nam_key .. "')."
  if ir_path ~= "" then
    msg = msg .. "\nIR " ..
      (ir_ok and ("loaded (key = '" .. ir_key .. "').")
              or "NOT accepted programmatically — clipboard fallback used.")
  end
  reaper.ShowMessageBox(msg, "NAM autoload", 0)
else
  local clip = nam_path
  if ir_path ~= "" then clip = clip .. "\n" .. ir_path end
  copy_to_clipboard(clip)
  reaper.ShowMessageBox(
    "The plugin did not accept the model path via SetNamedConfigParm.\n\n" ..
    "Path(s) copied to clipboard — click the plugin's 'Load NAM' button,\n" ..
    "paste in the file dialog, then repeat for the IR if applicable.\n\n" ..
    "Selected model:\n" .. nam_path ..
    (ir_path ~= "" and ("\n\nSelected IR:\n" .. ir_path) or ""),
    "NAM autoload — manual step required", 0)
end
