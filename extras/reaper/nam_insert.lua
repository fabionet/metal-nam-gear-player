-- METAL NAM GEAR PLAYER — Reaper first-test helper
--
-- Inserts a new track at position 0, names it "NAM Custom" and loads the
-- "NAM Custom" VST3 plugin on it, then opens the FX chain window.
--
-- Usage (Reaper must have scanned the VST3 folder that contains NAM Custom):
--   1. Windows / macOS / Linux Reaper GUI:
--      Actions → Load ReaScript → select this file → Run
--   2. From a shell (Linux example):
--      reaper -nonewinst /path/to/nam_insert.lua
--   3. From a shell (Windows, adjust reaper.exe path):
--      "C:\Program Files\REAPER (x64)\reaper.exe" -nonewinst nam_insert.lua
--
-- Requires REAPER 6.x or newer. If NAM Custom does not appear on the track,
-- open Options → Preferences → Plug-ins → VST → Re-scan and try again.

reaper.PreventUIRefresh(1)
reaper.InsertTrackAtIndex(0, true)
local track = reaper.GetTrack(0, 0)
if track then
  reaper.GetSetMediaTrackInfo_String(track, "P_NAME", "NAM Custom", true)
  local names = {
    "VST3: NAM Custom (fabionet)",
    "VST3: NAM Custom",
    "NAM Custom",
  }
  local idx = -1
  for _, n in ipairs(names) do
    idx = reaper.TrackFX_AddByName(track, n, false, -1)
    if idx >= 0 then break end
  end
  if idx >= 0 then
    reaper.TrackFX_Show(track, idx, 3) -- open FX chain window + show plugin UI
  else
    reaper.ShowMessageBox(
      "NAM Custom VST3 was not found in Reaper's plug-in cache.\n\n" ..
      "Make sure NAM Custom.vst3 is installed under your VST3 folder\n" ..
      "and then run Options -> Preferences -> Plug-ins -> VST -> Re-scan.",
      "NAM Custom - first-test helper", 0)
  end
end
reaper.PreventUIRefresh(-1)
reaper.TrackList_AdjustWindows(false)
reaper.UpdateArrange()
