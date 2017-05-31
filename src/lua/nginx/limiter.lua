local limiters = {}
return function(create_limit_data_cfunc, destroy_limit_data_cfunc)

  function _G.findLimit(limiter_ref, key)
    local limit = rawget(limiters, limiter_ref)
    if not limit then 
      return nil
    end
    local data = rawget(limit, key)
    if not data then
      return nil
    end
    return data
  end

  function _G.getLimit(limiter_ref, key)
    local limit = rawget(limiters, limiter_ref)
    local data
    if not limit then 
      limit = {}
      rawset(limiters, limiter_name, limit)
    end
    local data = rawget(limit, key)
    if not data then
      data = create_limit_data_cfunc()
      rawset(limit, key, data)
    end
    return data
  end
  
  function _G.clearLimit(limiter_ref, key)
    if key and limiter_ref then
      local limit = limiters[limiter_ref]
      if limit and limit[key] then 
        destroy_limit_data_cfunc(limit[key])
      end
    elseif not key then
      if limiter_ref then  --clear all for a limiter
        local limit = limiters[limiter_ref] or {}
        for _, v in pairs(limit) do
          destroy_limit_data_cfunc(v)
        end
      else --clear everything
        for ref, limit in pairs(limiters) do
          for key, v in pairs(limit) do
            destroy_limit_data_cfunc(v)
          end
        end
      end
    end
  end
end
