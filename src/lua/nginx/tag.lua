local tags = {}
local rawget = table.rawget
local rawset = table.rawset
function _G.getTag(ref, tag)
  local mytags = rawget(tags, ref)
  return mytags and rawget(mytags, tag)
end

function _G.setTag(ref, tag)
  local mytags = rawget(tags, ref)
  if not mytags then
    rawset(tags, {[tag]=true})
    return true
  else
    rawset(mytags, tag, true)
    return nil
  end
end
function _G.clearTags(ref)
  rawset(tags, ref, nil)
end
