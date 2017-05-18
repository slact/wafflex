local ignore_leading_hash_key = function(self, key)
  if key:sub(1,1)=="#" then
    return self[key:sub(2)]
  end
end

local function create_thing_storage(thing_name)
  local self = {table = setmetatable({}, {__index = ignore_leading_hash_key})}
  
  local function unpack_thing(data)
    local name, val = next(data)
    local thing = self.table[name]
    assert(thing, ("Unknown %s \"%s\""):format(thing_name, name))
    return name, val
  end
  
  function self.add(name, funcs)
    assert(funcs.parse, ("%s missing parse callback"):format(thing_name))
    -- funcs.init may be omitted
    assert(self.table[name] == nil, ("%s %s already exists"):format(thing_name, name))
    local added = {
      parse=funcs.parse,
      init={},
      meta={__index = {
        type=thing_name,
        [thing_name]=name
      }}
    }
    if funcs.init then
      table.insert(added.init, funcs.init)
    end
    self.table[name]=added
    
    return true
  end
  
  function self.addInit(name, init_func)
    local thing_preset = self.table[name]
    assert(thing_preset, ("Unknown %s \"%s\""):format(thing_name, name))
    table.insert(thing_preset.init, init_func)
    return true
  end
  
  function self.parse(data, parser)
    local name, val = unpack_thing(data)    
    val = self.table[name].parse(val, parser) or val
    return {[name]=val}
  end
  
  function self.new(data, ruleset)
    local name, val = unpack_thing(data)
    local thing_preset = self.table[name]
    local thing = setmetatable({[name]=val, ["data"]=data}, thing_preset.meta)
    for i, init in ipairs(thing_preset.init) do
      init(data, thing, ruleset)
    end
    return thing
  end
  
  return self
end

local rule = {
  condition = create_thing_storage("condition"),
  action = create_thing_storage("action")
}

--now let's add some basic conditions and actions
rule.condition.add("any", {parse = function(data, parser)
  assert(parser.jsontype(data)=="array", "\"any\" condition value must be an array of conditions")
  for i, v in ipairs(data) do
    local condition = parser:parseCondition(v)
    data[i]=condition
  end
end})

rule.condition.add("all", {parse = function(data, parser)
  assert(parser.jsontype(data)=="array", "\"all\" condition value must be an array of conditions")
  for i, v in ipairs(data) do
    local condition = parser:parseCondition(v)
    data[i]=condition
  end
end})

rule.condition.add("true", {parse = function(data, parser)
  assert(next(data) == nil, "\"true\" condition must have empty parameters")
end})

rule.condition.add("false", {parse = function(data, parser)
  assert(next(data) == nil, "\"false\" condition must have empty parameters")
end})

rule.condition.add("tag-check", {parse = function(data, parser)
  parser.assert_type(data, "string", "\"tag-check\" value must be a string")
end})

rule.condition.add("match", {parse = function(data, parser)
  parser.assert_jsontype(data, "array", "\"match\" value must be an array of strings")
  for i, v in ipairs(data) do
    parsers.assert_jsontype(v, "string", "\"match\" value must be an array of strings")
  end
end})








return rule
