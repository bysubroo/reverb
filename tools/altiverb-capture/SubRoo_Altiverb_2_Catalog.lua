-- SubRoo Rooms – Altiverb IR catalog (step 2)
-- Run it once, then click through the IRs in Altiverb's browser: every IR that gets loaded
-- is logged as  hashfloat;hash;path  to altiverb_ir_catalog.txt next to this script.
-- Create a file named STOP_WATCHER2 next to the script to stop it.

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
-- watcher v2: polls Altiverb's state twice a second and logs every newly loaded IR
-- (hashfloat;hash;path) to altiverb_ir_catalog.txt. Stops when STOP_WATCHER2 exists.
local tr = findTrack()
local FX = 0
local tStart = reaper.time_precise()
local seen = {}
do
  local f = io.open(SCRIPT_DIR .. "altiverb_ir_catalog.txt", "r")
  if f then
    for line in f:lines() do
      local hf = line:match("^([%d%.]+);")
      if hf then seen[hf] = true end
    end
    f:close()
  end
end
log("watcher_log.txt", os.date("%H:%M:%S") .. " watcher2 started")
local lastPoll, lastKey = 0, nil
local function poll()
  if reaper.file_exists(SCRIPT_DIR .. "STOP_WATCHER2") or reaper.time_precise() - tStart > 4 * 3600 then
    log("watcher_log.txt", os.date("%H:%M:%S") .. " watcher2 stopped")
    return
  end
  local now = reaper.time_precise()
  if now - lastPoll >= 0.5 then
    lastPoll = now
    local path, hf, h = getState(tr, FX)
    if path and path ~= "" and hf and hf < 2147483000 then
      local key = string.format("%.1f", hf)
      if key ~= lastKey then
        lastKey = key
        if not seen[key] then
          seen[key] = true
          log("altiverb_ir_catalog.txt", string.format("%s;%s;%s", key, tostring(h), path))
        end
      end
    end
  end
  reaper.defer(poll)
end
poll()
