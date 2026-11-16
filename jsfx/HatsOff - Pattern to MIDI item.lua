-- HatsOff - Pattern to MIDI item.lua
-- Prints the current HatsOff pattern as a MIDI item on the selected track.
--  * With a time selection: the item fills the time selection (pattern repeats).
--  * Without one: one pattern cycle is placed at the edit cursor.
-- The pattern comes from the HatsOff instance you last clicked in.
-- If the target track holds HatsOff, its SEQ is switched off so the new
-- MIDI item and the internal sequencer don't play the hats twice.

local r = reaper
local title = "HatsOff"

if not r.gmem_attach then
  r.MB("This script needs REAPER 6.0 or newer.", title, 0)
  return
end
r.gmem_attach("HatsOff")

if r.gmem_read(0) ~= 1 then
  r.MB("No HatsOff pattern found.\n\nOpen the HatsOff window and click anywhere in it, then run this script again.", title, 0)
  return
end

local track = r.GetSelectedTrack(0, 0)
if not track then
  r.MB("Select the track that should receive the MIDI item.", title, 0)
  return
end

local nsteps  = math.floor(r.gmem_read(1) + 0.5)
local steplen = r.gmem_read(2)          -- in quarter notes
local swing   = r.gmem_read(3)          -- 0..100
local pitch   = math.floor(r.gmem_read(4) + 0.5)
local swd     = swing / 100 * 0.5 * steplen
local patlen  = nsteps * steplen        -- in quarter notes

local steps, any = {}, false
for s = 0, nsteps - 1 do
  local on  = r.gmem_read(10 + s * 3) > 0.5
  local vel = math.floor(r.gmem_read(11 + s * 3) + 0.5)
  local rat = math.max(1, math.floor(r.gmem_read(12 + s * 3) + 0.5))
  steps[s] = { on = on, vel = math.max(1, math.min(127, vel)), rat = rat }
  if on then any = true end
end
if not any then
  r.MB("The HatsOff pattern is empty.", title, 0)
  return
end

-- placement
local ts_start, ts_end = r.GetSet_LoopTimeRange(false, false, 0, 0, false)
local startQN, endQN
if ts_end > ts_start then
  startQN = r.TimeMap2_timeToQN(0, ts_start)
  endQN   = r.TimeMap2_timeToQN(0, ts_end)
else
  startQN = r.TimeMap2_timeToQN(0, r.GetCursorPosition())
  endQN   = startQN + patlen
end

r.Undo_BeginBlock()
r.PreventUIRefresh(1)

local item = r.CreateNewMIDIItemInProj(track,
  r.TimeMap2_QNToTime(0, startQN), r.TimeMap2_QNToTime(0, endQN), false)
local take = item and r.GetActiveTake(item)
if not take then
  r.PreventUIRefresh(-1)
  r.Undo_EndBlock("HatsOff: pattern to MIDI item (failed)", -1)
  r.MB("Could not create the MIDI item.", title, 0)
  return
end
r.GetSetMediaItemTakeInfo_String(take, "P_NAME", "HatsOff", true)

local cycle = 0
while startQN + cycle * patlen < endQN - 1e-9 do
  for s = 0, nsteps - 1 do
    local st = steps[s]
    if st.on then
      local sub  = steplen / st.rat
      local base = startQN + cycle * patlen + s * steplen + ((s % 2 == 1) and swd or 0)
      for j = 0, st.rat - 1 do
        local q0 = base + j * sub
        if q0 < endQN - 1e-9 then
          local q1 = math.min(q0 + sub * 0.5, endQN)
          local p0 = r.MIDI_GetPPQPosFromProjQN(take, q0)
          local p1 = r.MIDI_GetPPQPosFromProjQN(take, q1)
          r.MIDI_InsertNote(take, false, false, p0, math.max(p1, p0 + 1), 0, pitch, st.vel, true)
        end
      end
    end
  end
  cycle = cycle + 1
end
r.MIDI_Sort(take)

-- switch off the internal sequencer of HatsOff on this track
for fx = 0, r.TrackFX_GetCount(track) - 1 do
  local _, name = r.TrackFX_GetFXName(track, fx, "")
  if name:find("HatsOff") then
    r.TrackFX_SetParam(track, fx, 1, 0)   -- parameter 2 = SEQ
  end
end

r.PreventUIRefresh(-1)
r.UpdateArrange()
r.Undo_EndBlock("HatsOff: pattern to MIDI item", -1)
