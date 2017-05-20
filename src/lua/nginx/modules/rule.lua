local mm = require "mm"
local Binding = require "binding"

local function ignore_leading_hash(str)
  return str:sub(1,1)=="#" and str:sub(2) or str
end

local thingstorage_meta = {__index = function(self, key)
  local unhashed = ignore_leading_hash(key)
  if unhashed ~= key then
    return self[ignore_leading_hash(key)]
  end
end}

local function create_thing_storage(thing_name)
  local self = {table = setmetatable({}, thingstorage_meta)}
  
  local function unpack_thing(data, parser)
    local name, val = next(data)
    local thing = self.table[name]
    if parser then
      parser:assert(thing, ("Unknown %s \"%s\""):format(thing_name, name))
    else
      assert(thing, ("Unknown %s \"%s\""):format(thing_name, name))
    end
    return name, val
  end
  
  function self.add(name, funcs, metaindex)
    assert(funcs.parse, ("%s missing parse callback"):format(thing_name))
    assert(self.table[name] == nil, ("%s %s already exists"):format(thing_name, name))
    local added = {
      parse=funcs.parse,
      init=funcs.init or function() end
    }
    if metaindex then
      added.meta={__index = metaindex}
    end
    self.table[name]=added
    return true
  end
  
  function self.parse(data, parser)
    local name, val = unpack_thing(data, parser)
    val = self.table[name].parse(val, parser) or val
    return {[name]=val}
  end
  
  function self.new(data, ruleset)
    local name, val = unpack_thing(data)
    local thing_preset = self.table[name]
    local thing = setmetatable({[thing_name]=ignore_leading_hash(name), ["data"]=val}, thing_preset.meta)
    thing_preset.init(val, thing, ruleset)
    Binding.call(thing_name, "create", thing)
    return thing
  end
  
  return self
end

local Rule = {
  condition = create_thing_storage("condition"),
  action = create_thing_storage("action")
}

--now let's add some basic conditions and actions
Rule.condition.add("any", {
  parse = function(data, parser)
    parser:assert_jsontype(data, "array", "\"any\" condition value must be an array of conditions")
    for i, v in ipairs(data) do
      local condition = parser:parseCondition(v)
      data[i]=condition
    end
  end,
  init = function(data, thing, ruleset)
    for i, v in ipairs(data) do
      data[i] = Rule.condition.new(v, ruleset)
    end
  end
})

Rule.condition.add("all", {
  parse = function(data, parser)
    parser:assert_jsontype(data, "array", "\"all\" condition value must be an array of conditions")
    for _, v in ipairs(data) do
      local condition = parser:parseCondition(v)
      data[i]=condition
    end
  end,
  init = function(data, thing, ruleset)
    for i, v in ipairs(data) do
      data[i] = Rule.condition.new(v, ruleset)
    end
  end
})

Rule.condition.add("true", {parse = function(data, parser)
  --parser:assert(next(data) == nil, "\"true\" condition must have empty parameters")
end})

Rule.condition.add("false", {parse = function(data, parser)
  --parser:assert(next(data) == nil, "\"false\" condition must have empty parameters")
end})

Rule.condition.add("tag-check", {parse = function(data, parser)
  parser:assert_type(data, "string", "\"tag-check\" value must be a string")
end})

Rule.condition.add("match", {parse = function(data, parser)
  parser:assert_jsontype(data, "array", "\"match\" value must be an array of strings")
  for _, v in ipairs(data) do
    parser:assert_jsontype(v, "string", "\"match\" value must be an array of strings")
  end
end})

--some actions, too
Rule.action.add("tag", {parse = function(data, parser)
  parser:assert_jsontype(data, "string", "\"tag\" value must be a string")
end})

Rule.action.add("accept", {parse = function(data, parser)
  parser:assert_type(data, "table", "\"accept\" value must be an object")
  parser:assert_table_size(data, 0, "\"accept\" value must be empty")
end})
Rule.action.add("reject", {parse = function(data, parser)
  parser:assert_type(data, "table", "\"reject\" value must be an object")
  --parser:assert_table_size(data, 0, "\"reject\" value must be empty")
end})


return Rule
