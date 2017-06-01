local tags = {}
local tconcat = table.concat
local rawset, rawget = rawset, rawget

function findTag(ref, key)
  local mytags = rawget(tags, ref)
  return mytags and rawget(mytags, key) or nil
end

function setTag(ref, key)
  local mytags = rawget(tags, ref)
  if not mytags then
    rawset(tags, ref, {[key]=true})
    return true
  else
    rawset(mytags, key, true)
    return nil
  end
end
function clearTags(ref)
  rawset(tags, ref, nil)
end
