local tags = {}
local rawset, rawget = rawset, rawget

--luacheck: globals findTag setTag clearTags

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
