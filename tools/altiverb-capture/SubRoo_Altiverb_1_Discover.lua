-- SubRoo Rooms - Altiverb capture, step 1: discover
-- REAPER: Actions > Show action list > New action > Load ReaScript... > run.
-- Finds (or adds) Altiverb on a track named "ALTIVERB CAPTURE", lists every
-- parameter and, if an IR-selection parameter exists, every IR it can reach.
-- Output: <REAPER resource path>/SubRoo/altiverb_params.txt and altiverb_ir_list.csv

local OUT = reaper.GetResourcePath() .. "/SubRoo"
reaper.RecursiveCreateDirectory(OUT, 0)

local function log(s) reaper.ShowConsoleMsg(tostring(s) .. "\n") end

local function findOrCreateTrack()
  for i = 0, reaper.CountTracks(0) - 1 do
    local tr = reaper.GetTrack(0, i)
    local _, name = reaper.GetSetMediaTrackInfo_String(tr, "P_NAME", "", false)
    if name == "ALTIVERB CAPTURE" then return tr end
  end
  reaper.InsertTrackAtIndex(reaper.CountTracks(0), true)
  local tr = reaper.GetTrack(0, reaper.CountTracks(0) - 1)
  reaper.GetSetMediaTrackInfo_String(tr, "P_NAME", "ALTIVERB CAPTURE", true)
  return tr
end

local function findAltiverb(tr)
  for fx = 0, reaper.TrackFX_GetCount(tr) - 1 do
    local _, name = reaper.TrackFX_GetFXName(tr, fx, "")
    if name:lower():find("altiverb") then return fx, name end
  end
  -- try the usual plug-in names (quad output first)
  for _, n in ipairs({ "Altiverb 7 XL (Audio Ease)", "Altiverb 7 XL", "Altiverb 7 Quad", "Altiverb 7", "Altiverb" }) do
    local fx = reaper.TrackFX_AddByName(tr, n, false, -1)
    if fx >= 0 then
      local _, name = reaper.TrackFX_GetFXName(tr, fx, "")
      return fx, name
    end
  end
  return -1, nil
end

reaper.ClearConsole()
local tr = findOrCreateTrack()
reaper.SetMediaTrackInfo_Value(tr, "I_NCHAN", 4)
local fx, fxname = findAltiverb(tr)
if fx < 0 then
  log("Altiverb not found. Add it to the track 'ALTIVERB CAPTURE' manually and run this again.")
  return
end
log("FX: " .. fxname)

local f = io.open(OUT .. "/altiverb_params.txt", "w")
f:write("FX: " .. fxname .. "\n")
local n = reaper.TrackFX_GetNumParams(tr, fx)
local irParam = -1
for p = 0, n - 1 do
  local _, pname = reaper.TrackFX_GetParamName(tr, fx, p, "")
  local _, val = reaper.TrackFX_GetFormattedParamValue(tr, fx, p, "")
  local norm = reaper.TrackFX_GetParamNormalized(tr, fx, p)
  local _, step = reaper.TrackFX_GetParameterStepSizes(tr, fx, p)
  local line = string.format("%3d  %-32s  %-28s  norm=%.6f  step=%s", p, pname, val, norm, tostring(step))
  f:write(line .. "\n")
  log(line)
  local l = pname:lower()
  if irParam < 0 and (l:find("impulse") or l == "ir" or l:find("ir select") or l:find("ir index") or l:find("room")) then
    irParam = p
  end
end
f:close()
log("\nparameter list -> " .. OUT .. "/altiverb_params.txt")

if irParam < 0 then
  log("No IR-selection parameter found by name. Send the parameter list to Claude.")
  return
end

-- walk the IR parameter and record every distinct value
log(string.format("\nScanning parameter %d for IR names...", irParam))
local orig = reaper.TrackFX_GetParamNormalized(tr, fx, irParam)
local csv = io.open(OUT .. "/altiverb_ir_list.csv", "w")
csv:write("norm;name\n")
local last, count = nil, 0
local steps = 8000
for i = 0, steps do
  local v = i / steps
  reaper.TrackFX_SetParamNormalized(tr, fx, irParam, v)
  local _, name = reaper.TrackFX_GetFormattedParamValue(tr, fx, irParam, "")
  if name ~= last then
    csv:write(string.format("%.8f;%s\n", v, name))
    last = name
    count = count + 1
  end
end
csv:close()
reaper.TrackFX_SetParamNormalized(tr, fx, irParam, orig)
log(string.format("%d distinct values -> %s/altiverb_ir_list.csv", count, OUT))
