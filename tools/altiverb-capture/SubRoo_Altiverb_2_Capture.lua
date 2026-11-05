-- SubRoo Rooms - Altiverb capture, step 2: capture
-- Renders one impulse through Altiverb for every line of capture_list.csv and
-- saves the result as a 4-channel (quad) WAV:
--   Documents/SubRoo Rooms/Altiverb Captures/<Category>__<Room>__<Position>.wav
--
-- capture_list.csv (in <REAPER resource path>/SubRoo/):
--   param;norm;Category;Room;Position
--   12;0.03412500;Post;Bathroom small;Position 1
--
-- neutral.csv (optional, same folder): parameters set before every capture
--   param;norm
--
-- Altiverb settings must be neutral: wet only (no dry), direct/early/tail at
-- 0 dB, no EQ, no damping, size and reverb time 100 %, Bright off.

local RES = reaper.GetResourcePath() .. "/SubRoo"
local HOME = os.getenv("USERPROFILE") or os.getenv("HOME")
local OUTDIR = HOME .. "/Documents/SubRoo Rooms/Altiverb Captures"
local FS = 48000
local LENGTH_SEC = 5.0
local WAIT_SEC = 3.0          -- time for Altiverb to load each IR
local APPLY_MULTICHANNEL = 41993  -- Item: Apply track/take FX to items (multichannel output)

reaper.RecursiveCreateDirectory(OUTDIR, 0)
local function log(s) reaper.ShowConsoleMsg(tostring(s) .. "\n") end

local function readCsv(path)
  local rows = {}
  local f = io.open(path, "r")
  if not f then return rows end
  local header = true
  for line in f:lines() do
    if header then header = false
    elseif line:match("%S") then
      local t = {}
      for cell in (line .. ";"):gmatch("([^;]*);") do t[#t + 1] = cell end
      rows[#rows + 1] = t
    end
  end
  f:close()
  return rows
end

local function writeImpulse(path)
  local n = math.floor(FS * LENGTH_SEC)
  local f = io.open(path, "wb")
  local dataBytes = n * 4
  f:write("RIFF", string.pack("<I4", 36 + dataBytes), "WAVE")
  f:write("fmt ", string.pack("<I4I2I2I4I4I2I2", 16, 3, 1, FS, FS * 4, 4, 32))
  f:write("data", string.pack("<I4", dataBytes))
  f:write(string.pack("<f", 1.0))
  local zero = string.pack("<f", 0.0)
  local chunk = zero:rep(4096)
  local left = n - 1
  while left >= 4096 do f:write(chunk); left = left - 4096 end
  f:write(zero:rep(left))
  f:close()
end

local function copyFile(src, dst)
  local i = io.open(src, "rb"); if not i then return false end
  local data = i:read("a"); i:close()
  local o = io.open(dst, "wb"); if not o then return false end
  o:write(data); o:close()
  return true
end

local function safe(s) return (s:gsub("[\\/:%*%?\"<>|]", "-")) end

-- ------------------------------------------------------------------ setup
local track
for i = 0, reaper.CountTracks(0) - 1 do
  local tr = reaper.GetTrack(0, i)
  local _, name = reaper.GetSetMediaTrackInfo_String(tr, "P_NAME", "", false)
  if name == "ALTIVERB CAPTURE" then track = tr end
end
if not track then log("Run SubRoo_Altiverb_1_Discover.lua first."); return end

local fx = -1
for i = 0, reaper.TrackFX_GetCount(track) - 1 do
  local _, n = reaper.TrackFX_GetFXName(track, i, "")
  if n:lower():find("altiverb") then fx = i end
end
if fx < 0 then log("Altiverb is not on the track."); return end

reaper.GetSetProjectInfo(0, "PROJECT_SRATE", FS, true)
reaper.GetSetProjectInfo(0, "PROJECT_SRATE_USE", 1, true)
reaper.SetMediaTrackInfo_Value(track, "I_NCHAN", 4)

local impulse = RES .. "/subroo_impulse_48k.wav"
writeImpulse(impulse)

local list = readCsv(RES .. "/capture_list.csv")
local neutral = readCsv(RES .. "/neutral.csv")
if #list == 0 then log("capture_list.csv is empty."); return end
reaper.ClearConsole()
log(string.format("Capturing %d impulse responses -> %s", #list, OUTDIR))

local idx, phase, t0 = 1, "set", 0
local done, failed = 0, 0

local function step()
  if idx > #list then
    log(string.format("\nFINISHED: %d captured, %d failed.", done, failed))
    return
  end
  local row = list[idx]
  local param, norm = tonumber(row[1]), tonumber(row[2])
  local cat, room, pos = row[3] or "Altiverb", row[4] or ("IR " .. idx), row[5] or "1"

  if phase == "set" then
    for _, nrow in ipairs(neutral) do
      reaper.TrackFX_SetParamNormalized(track, fx, tonumber(nrow[1]), tonumber(nrow[2]))
    end
    reaper.TrackFX_SetParamNormalized(track, fx, param, norm)
    t0 = reaper.time_precise()
    phase = "wait"
  elseif phase == "wait" then
    if reaper.time_precise() - t0 >= WAIT_SEC then phase = "render" end
  elseif phase == "render" then
    -- fresh impulse item at 0
    for i = reaper.CountTrackMediaItems(track) - 1, 0, -1 do
      reaper.DeleteTrackMediaItem(track, reaper.GetTrackMediaItem(track, i))
    end
    reaper.SetOnlyTrackSelected(track)
    reaper.SetEditCurPos(0, false, false)
    reaper.InsertMedia(impulse, 0)
    local item = reaper.GetTrackMediaItem(track, 0)
    reaper.SelectAllMediaItems(0, false)
    reaper.SetMediaItemSelected(item, true)
    reaper.Main_OnCommand(APPLY_MULTICHANNEL, 0)
    local take = reaper.GetActiveTake(item)
    local src = reaper.GetMediaItemTake_Source(take)
    local file = reaper.GetMediaSourceFileName(src, "")
    local nch = reaper.GetMediaSourceNumChannels(src)
    local name = safe(cat) .. "__" .. safe(room) .. "__" .. safe(pos) .. ".wav"
    if file ~= impulse and copyFile(file, OUTDIR .. "/" .. name) then
      done = done + 1
      log(string.format("[%d/%d] %s  (%d ch)", idx, #list, name, nch))
    else
      failed = failed + 1
      log(string.format("[%d/%d] FAILED %s", idx, #list, name))
    end
    reaper.DeleteTrackMediaItem(track, item)
    reaper.UpdateArrange()
    idx = idx + 1
    phase = "set"
  end
  reaper.defer(step)
end

step()
