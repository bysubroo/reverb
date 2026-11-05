-- shared helpers (prepended to jobs)
local SCRIPT_DIR = SUBROO_DIR or debug.getinfo(1, "S").source:match("^@?(.*[\\/])")

local function findTrack()
  for i = 0, reaper.CountTracks(0) - 1 do
    local t = reaper.GetTrack(0, i)
    local _, n = reaper.GetSetMediaTrackInfo_String(t, "P_NAME", "", false)
    if n == "ALTIVERB CAPTURE" then return t end
  end
  return reaper.GetTrack(0, 0)
end

local b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
local b64map = {}
for i = 1, #b64chars do b64map[b64chars:byte(i)] = i - 1 end

local function b64decode(s)
  local out, n = {}, 0
  local bits, nbits = 0, 0
  for i = 1, #s do
    local v = b64map[s:byte(i)]
    if v then
      bits = (bits << 6) | v
      nbits = nbits + 6
      if nbits >= 8 then
        nbits = nbits - 8
        n = n + 1
        out[n] = string.char((bits >> nbits) & 255)
      end
    end
  end
  return table.concat(out)
end

local function getState(tr, fx)
  local ok, chunk = reaper.TrackFX_GetNamedConfigParm(tr, fx, "vst_chunk")
  if not ok or not chunk then return nil end
  local xml = b64decode(chunk)
  local path = xml:match("<key>IR FolderID and Path</key>%s*<string>(.-)</string>")
  local hf = xml:match("<key>IR HashFloat</key>%s*<real>([%d%.%-]+)</real>")
  local h = xml:match("<key>IR Hash</key>%s*<integer>(%-?%d+)</integer>")
  return path, tonumber(hf), h, xml
end

local function log(name, line)
  local f = io.open(SCRIPT_DIR .. name, "a")
  if f then f:write(line, "\n"); f:close() end
end

local function b64encode(data)
  local out, n = {}, 0
  local len = #data
  for i = 1, len, 3 do
    local a, b, c = data:byte(i, i + 2)
    local v = (a << 16) | ((b or 0) << 8) | (c or 0)
    local c1 = (v >> 18) & 63
    local c2 = (v >> 12) & 63
    local c3 = (v >> 6) & 63
    local c4 = v & 63
    n = n + 1
    out[n] = b64chars:sub(c1 + 1, c1 + 1) .. b64chars:sub(c2 + 1, c2 + 1)
      .. (b and b64chars:sub(c3 + 1, c3 + 1) or "=")
      .. (c and b64chars:sub(c4 + 1, c4 + 1) or "=")
  end
  return table.concat(out)
end

local function readFile(p)
  local f = io.open(p, "rb"); if not f then return nil end
  local s = f:read("a"); f:close(); return s
end

-- build a state from the template with another IR path; hashes cleared so the path is used
local function stateForPath(templateXml, irPath, keepHash)
  local xml = templateXml
  local esc = irPath:gsub("%%", "%%%%")
  xml = xml:gsub("(<key>IR FolderID and Path</key>%s*<string>).-(</string>)", "%1" .. esc .. "%2", 1)
  if not keepHash then
    xml = xml:gsub("(<key>IR Hash</key>%s*<integer>)%-?%d+(</integer>)", "%10%2", 1)
    xml = xml:gsub("(<key>IR HashFloat</key>%s*<real>)[%d%.%-]+(</real>)", "%10.0000000000%2", 1)
    xml = xml:gsub("(<key>last_hash</key>%s*<integer>)%-?%d+(</integer>)", "%10%2", 1)
  end
  return xml
end
-- SubRoo Rooms – batch capture of Altiverb IRs through Altiverb itself
-- Reads capture_list.csv next to this script:
--     hashfloat;Category;Room;Position;Mode
-- Mode "ms" (mono-in IR)  -> one render, impulse on both inputs          -> ..__C.wav
-- Mode "ss" (stereo-in IR)-> two renders, impulse on L only / R only      -> ..__L.wav, ..__R.wav
-- For each row: load the IR through Altiverb's own IR automation parameter (47),
-- wait until Altiverb reports that exact IR as loaded, render the impulse through
-- the track FX (REAPER action 41993) and copy the rendered take to Captures\.
-- Already captured files are skipped, so the job can be stopped and resumed.
-- Stop file: STOP_CAPTURE next to the script.

local tr = findTrack()
local FX, P_IR = 0, 47
local SCALE = 2147483648.0
local FS = 48000
local LENGTH_SHORT, LENGTH_LONG = 6.0, 12.0
local LONG_CATS = { ["outdoor & underground"] = true, ["public spaces"] = true }
local CAP_DIR = SCRIPT_DIR .. "AltiverbPOST\\"
reaper.RecursiveCreateDirectory(CAP_DIR, 0)

-- neutral settings: full IR (direct + ER + tail), 100 % wet, no EQ / decay / mod / reverse
local NEUTRAL = {
  [2] = 1.0,  -- ER On
  [3] = 0.5,  -- Direct gain 0 dB
  [5] = 0.5,  -- ER gain 0 dB
  [6] = 0.0,  -- ER delay 0
  [7] = 0.5,  -- Tail gain 0 dB
  [8] = 0.5,  -- Tail delay 0
  [9] = 0.642857, -- reverb time 100 %
  [10] = 0.5, [11] = 0.5, [12] = 0.5, -- damping 100 %
  [21] = 0.5, [22] = 0.5, [23] = 0.5, [24] = 0.5, -- levels 0 dB
  [25] = 1.0, -- dry/wet 100 % wet
  [33] = 0.0, -- EQ off
  [34] = 0.5, -- size 100 %
  [35] = 0.0, -- stage positioning off
  [38] = 0.0, -- decay off
  [39] = 0.0, -- reverse off
  [40] = 1.0, -- direct on
  [41] = 1.0, -- tail on
  [42] = 0.0, -- bright 0
  [43] = 0.0, -- attack 0
  [46] = 0.0, -- modulation off
}

local function applyNeutral()
  for p, v in pairs(NEUTRAL) do reaper.TrackFX_SetParamNormalized(tr, FX, p, v) end
end

local function sanitize(s)
  s = s:gsub("&apos;", ""):gsub("&amp;", "and"):gsub("&quot;", "")
  s = s:gsub("[^%w%-%. ]", "_"):gsub("%s+", " ")
  return (s:gsub("^%s+", ""):gsub("%s+$", ""))
end

-- impulse files (float32, 48 kHz, 2 ch): both / L only / R only
local function writeImpulse(path, gl, gr, seconds)
  local n = math.floor(FS * seconds)
  local data = {}
  data[1] = string.pack("<ff", gl, gr)
  local zero = string.pack("<ff", 0.0, 0.0)
  for i = 2, n do data[i] = zero end
  local body = table.concat(data)
  local fmt = string.pack("<I2I2I4I4I2I2", 3, 2, FS, FS * 8, 8, 32)
  local f = io.open(path, "wb")
  f:write("RIFF", string.pack("<I4", 4 + (8 + #fmt) + (8 + #body)), "WAVE")
  f:write("fmt ", string.pack("<I4", #fmt), fmt)
  f:write("data", string.pack("<I4", #body), body)
  f:close()
end

-- Altiverb sums L+R for mono-in IRs (measured), so the centre impulse is 0.5 + 0.5 = unity
local IMP = {}
for _, len in ipairs({ {"S", LENGTH_SHORT}, {"Lg", LENGTH_LONG} }) do
  for _, sp in ipairs({ {"C", 0.5, 0.5}, {"L", 1.0, 0.0}, {"R", 0.0, 1.0} }) do
    local key = sp[1] .. len[1]
    IMP[key] = SCRIPT_DIR .. "impulse_" .. key .. ".wav"
    writeImpulse(IMP[key], sp[2], sp[3], len[2])
  end
end

-- job list
local jobs = {}
do
  local f = io.open(SCRIPT_DIR .. "capture_list.csv", "r")
  if not f then
    log("capture_log.txt", "no capture_list.csv")
    return
  end
  for line in f:lines() do
    local hf, cat, room, pos, mode = line:match("^([%d%.]+);([^;]*);([^;]*);([^;]*);(%a+)")
    if hf then
      local base = sanitize(cat) .. "__" .. sanitize(room) .. "__" .. sanitize(pos)
      local srcs = {"C"}
      if mode == "ss" then srcs = {"L", "R"} elseif mode == "cal" then srcs = {"C", "L", "R"} end
      for _, s in ipairs(srcs) do
        local lenKey = LONG_CATS[cat] and "Lg" or "S"
        jobs[#jobs + 1] = { hf = tonumber(hf), name = base .. "__" .. s .. ".wav", src = s .. lenKey }
      end
    end
  end
  f:close()
end

local function fileExists(p)
  local f = io.open(p, "rb"); if f then f:close(); return true end; return false
end

local function copyFile(src, dst)
  local fi = io.open(src, "rb"); if not fi then return false end
  local d = fi:read("a"); fi:close()
  local fo = io.open(dst, "wb"); if not fo then return false end
  fo:write(d); fo:close(); return true
end

local function clearItems()
  for i = reaper.CountTrackMediaItems(tr) - 1, 0, -1 do
    local it = reaper.GetTrackMediaItem(tr, i)
    reaper.DeleteTrackMediaItem(tr, it)
  end
end

local function renderImpulse(src, outPath)
  clearItems()
  reaper.SetOnlyTrackSelected(tr)
  reaper.SetEditCurPos(0, false, false)
  reaper.SelectAllMediaItems(0, false)
  reaper.InsertMedia(IMP[src], 0)
  local it = reaper.GetTrackMediaItem(tr, 0)
  if not it then return false, "no item" end
  reaper.SetMediaItemInfo_Value(it, "D_POSITION", 0)
  reaper.SetMediaItemSelected(it, true)
  reaper.Main_OnCommand(41993, 0) -- apply track FX to items as new take (multichannel output)
  local take = reaper.GetActiveTake(it)
  if not take then return false, "no take" end
  local srcObj = reaper.GetMediaItemTake_Source(take)
  local fn = reaper.GetMediaSourceFileName(srcObj, "")
  local ok = copyFile(fn, outPath)
  clearItems()
  -- the rendered take was written by this job into REAPER's media folder; drop it after copying
  if ok and fn and fn ~= "" and fn ~= IMP[src] then os.remove(fn) end
  return ok, fn
end

-- 48 kHz project rate (impulse must not be resampled), stereo track (Altiverb VST2 delivers stereo here)
reaper.GetSetProjectInfo(0, "PROJECT_SRATE", FS, true)
reaper.GetSetProjectInfo(0, "PROJECT_SRATE_USE", 1, true)
reaper.SetMediaTrackInfo_Value(tr, "I_NCHAN", 2)
reaper.PreventUIRefresh(1)
applyNeutral()
reaper.PreventUIRefresh(-1)
log("capture_log.txt", os.date("%H:%M:%S") .. " capture started, renders: " .. #jobs)

local idx, phase, t0 = 0, "next", 0
local loadedHf = nil

local function nextJob()
  idx = idx + 1
  while idx <= #jobs and fileExists(CAP_DIR .. jobs[idx].name) do idx = idx + 1 end
end

local function tick()
  if reaper.file_exists(SCRIPT_DIR .. "STOP_CAPTURE") then
    log("capture_log.txt", os.date("%H:%M:%S") .. " stopped by STOP_CAPTURE")
    return
  end
  local now = reaper.time_precise()
  if phase == "next" then
    nextJob()
    if idx > #jobs then
      log("capture_log.txt", os.date("%H:%M:%S") .. " capture finished")
      return
    end
    local j = jobs[idx]
    if loadedHf ~= j.hf then
      reaper.TrackFX_SetParamNormalized(tr, FX, P_IR, j.hf / SCALE)
      phase, t0 = "wait", now
    else
      phase, t0 = "settle", now
    end
  elseif phase == "wait" then
    local j = jobs[idx]
    if now - t0 > 0.5 then
      local _, hf = getState(tr, FX)
      if hf and math.abs(hf - j.hf) < 1 then
        loadedHf = j.hf
        phase, t0 = "settle", now
      elseif now - t0 > 15 then
        log("capture_log.txt", string.format("%s FAILED to load %.1f (%s)", os.date("%H:%M:%S"), j.hf, j.name))
        phase = "next"
      end
    end
  elseif phase == "settle" then
    if now - t0 > 1.5 then
      local j = jobs[idx]
      applyNeutral()
      local ok, info = renderImpulse(j.src, CAP_DIR .. j.name)
      log("capture_log.txt", string.format("%s %s %s %s", os.date("%H:%M:%S"), ok and "OK" or "ERR", j.name, tostring(info)))
      phase = "next"
    end
  end
  reaper.defer(tick)
end
tick()
