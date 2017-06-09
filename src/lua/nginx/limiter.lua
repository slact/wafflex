
--luacheck: globals findLimiterValue setLimiterValue unsetLimiterValue
--local mm = require "mm"
local setmetatable, rawset, rawget = setmetatable, rawset, rawget

local function tabling_meta(depth, ephemeron)
  return {__index = function(t,k)
    local v = {}
    if depth > 0 then
      setmetatable(v, tabling_meta(depth-1, type(ephemeron) == "number" and ephemeron-1 or ephemeron))
    end
    t[k]=v
    return v
  end, __mode = (ephemeron == true or (ephemeron and ephemeron > 0)) and "k" or nil }
end

local limiters = setmetatable({}, tabling_meta(2))

function findLimiterValue(limiter_ptr, key)
  local limit = rawget(limiters, limiter_ptr)
  if not limit then
    return nil
  end
  local data = rawget(limit, key)
  if not data then
    return nil
  end
  return data
end

function setLimiterValue(limiter_ptr, key, limit_data_ptr)
  limiters[limiter_ptr][key]=limit_data_ptr
  
  --check uniqueness
  --[[
  local uniq = {}
  mm(limiters[limiter_ptr])
  for k, v in pairs(limiters[limiter_ptr]) do
    assert(not uniq[v], "found two different keys mapping to same limiter value")
    uniq[v] = true
  end
  ]]
  
  return limit_data_ptr
end
  
function unsetLimiterValue(limiter_ptr, key, limit_data_ptr)
  --mm(limiters)
  local limit = rawget(limiters, limiter_ptr)
  local data = rawget(limit, key)
  if data then
    assert(type(data) == "userdata")
    if limit_data_ptr then
      assert(data == limit_data_ptr)
    end
    rawset(limit, key, nil)
    if next(limit) == nil then
      rawset(limiters, limiter_ptr, nil)
    end
    return true
  else
    return nil
  end
end

