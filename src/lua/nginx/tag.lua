local tags = {}
local tconcat = table.concat
local join = function(...)
  return tconcat({...})
end
local function wholetag(part1, part2, ...)
  if not part2 then return part1 end
  return join(part1, part2, ...)
end
function _G.findTag(ref, ...)
  local mytags = rawget(tags, ref)
  return mytags and rawget(mytags, wholetag(...))
end

function _G.setTag(ref, ...)
  local mytags = rawget(tags, ref)
  if not mytags then
    rawset(tags, ref, {[wholetag(...)]=true})
    return true
  else
    rawset(mytags, wholetag(...), true)
    return nil
  end
end
function _G.clearTags(ref)
  rawset(tags, ref, nil)
end
