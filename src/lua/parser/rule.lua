local ignore_leading_hash_key = function(self, key)
  if key:sub(1,1)=="#" then
    return self[key:sub(2)]
  end
end

local function create_thing_storage(thing_name)
  local self = {table = setmetatable({}, {__index = ignore_leading_hash_key})}
  
  local function unpack_thing(data)
    local name, val = next(data)
    local cnd = self.table[name]
    assert(cnd, ("Unknown %s \"%s\""):format(thing_name, name))
    return name, val
  end
  
  function self.add(name, parse_func, init_func)
    assert(self.table[name] == nil, ("%s %s already exists"):format(thing_name, name))
    self.table[name]={
      parse=parse_func,
      initialize=init_func,
      meta={__index = {
        type=thing_name,
        [thing_name]=name
      }}
    }
    return true
  end
  
  function self.parse(data, parser)
    unpack_thing(data)    
    return cnd.parse(data, parser)
  end
  
  function self.new(data, ruleset)
    local name, val = unpack_thing(data)
    local thing = setmetatable({[name]=val}, cnd.meta)
    return cnd.initialize(thing, ruleset)
  end
  
  return self
end

local rule = {
  condition = create_thing_storage("condition"),
  action = create_thing_storage("action")
}

return rule
