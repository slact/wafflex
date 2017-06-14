local Binding = require "binding"
local json = require "dkjson"
--local mm = require "mm"

local Component -- forward declaration

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
    if type(name) == "number" then
      parser:error("invalid data value, expected {\"key\":value}, got {\"key\"}")
    elseif type(name) ~= "string" then
      parser:error("unexpected data type %s", type(name))
    end
    local thing = self.table[name]
    if parser then
      parser:assert(thing, ("Unknown %s \"%s\""):format(thing_name, name))
    else
      assert(thing, ("Unknown %s \"%s\""):format(thing_name, name))
    end
    return name, val
  end
  
  function self.add(name, funcs)
    if type(name) == "table" then
      for _,v in pairs(name) do
        self.add(v, funcs)
      end
      return true
    end
    assert(funcs.parse, ("%s missing parse callback"):format(thing_name))
    assert(self.table[name] == nil, ("%s %s already exists"):format(thing_name, name))
    local added = {
      parse=funcs.parse,
      init=funcs.init,
      delete=funcs.delete,
      meta = {
        __jsonval = funcs.jsonval,
        __jsonorder = funcs.jsonorder or {"action", "condition"},
        __index = {
          toJSON = function(tbl)
            return json.encode(tbl, {indent = true})
          end
        }
      }
    }
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
    name = ignore_leading_hash(name)
    local thing_preset = self.table[name]
    local thing = setmetatable({[thing_name]=name, data=val}, thing_preset.meta)
    if thing_preset.init then
      local replacement_data = thing_preset.init(val, thing, ruleset)
      if replacement_data then
        thing.data = replacement_data
      end
    end
    Binding.call(("%s:%s"):format(thing_name, name), "create", thing)
    return thing
  end
  
  function self.delete(thing, ruleset)
    local name = thing[thing_name] or thing.name
    local thing_preset = self.table[name]
    print(thing_name .. " DELETE THING " .. name .. " " .. tostring(thing_preset.delete))
    if thing_preset.delete then
      thing_preset.delete(thing.data, ruleset)
    end
    Binding.call(("%s:%s"):format(thing_name, name), "delete", thing)
  end
  
  return self
end

Component = {
  condition = create_thing_storage("condition"),
  action = create_thing_storage("action")
}

--now let's add some basic conditions and actions
Component.condition.add("any", {
  parse = function(data, parser)
    parser:assert_jsontype(data, "array", "\"any\" condition value must be an array of conditions")
    for i, v in ipairs(data) do
      local condition = parser:parseCondition(v)
      data[i]=condition
    end
  end,
  init = function(data, thing, ruleset)
    for i, v in ipairs(data) do
      data[i] = Component.condition.new(v, ruleset)
    end
  end,
  delete = function(data, ruleset)
    for _, cond in ipairs(data) do
      Component.condition.delete(cond, ruleset)
    end
  end,
  jsonval = function(self)
    return {any=self.data}
  end
})

Component.condition.add("all", {
  parse = function(data, parser)
    parser:assert_jsontype(data, "array", "\"all\" condition value must be an array of conditions")
    for i, v in ipairs(data) do
      local condition = parser:parseCondition(v)
      data[i]=condition
    end
  end,
  init = function(data, ruleset)
    for i, v in ipairs(data) do
      data[i] = Component.condition.new(v, ruleset)
    end
  end,
  delete = function(data, ruleset)
    for _, cond in ipairs(data) do
      Component.condition.delete(cond, ruleset)
    end
  end,
  jsonval = function(self)
    return {all=self.data}
  end
})

Component.condition.add({"true", "false"}, {
  parse = function(data, parser)
  --parser:assert(next(data) == nil, "\"true\" condition must have empty parameters")
  end,
  jsonval = function(self)
    return self.condition
  end
})

Component.condition.add("tag-check", {
  parse = function(data, parser)
    parser:assert_type(data, "string", "\"tag-check\" value must be a string")
    return parser:parseInterpolatedString(data)
  end,
  init = function(data)
    Binding.call("string", "create", data)
  end,
  delete = function(data)
    Binding.call("string", "delete", data)
  end,
  jsonval=function(self)
    return {["tag-check"]=self.data.string}
  end
})

Component.condition.add("match", {
  parse = function(data, parser)
    parser:assert_jsontype(data, "array", "\"match\" value must be an array of strings")
    for i, v in ipairs(data) do
      parser:assert_jsontype(v, "string", "\"match\" value must be an array of strings")
      data[i]=parser:parseInterpolatedString(v)
    end
  end,
  init = function(data)
    local complexity = function(str)
      local n = 0
      for _ in str.string:gmatch("%$") do
        n=n+1
      end
      return n
    end
    local simplefirst = function(str1, str2)
      return complexity(str1) < complexity(str2)
    end
    table.sort(data, simplefirst)
    for _, str in ipairs(data) do
      Binding.call("string", "create", str)
    end
  end,
  delete = function(data)
    for _, str in ipairs(data) do
      Binding.call("string", "delete", str)
    end
  end,
  jsonval = function(self)
    local strings = {}
    for _, str in ipairs(self.data) do
      table.insert(strings, str.string)
    end
    return {match=strings}
  end
})

local limit_thing_meta = {__jsonorder={"name", "key", "increment"}}

--limiter conditions
Component.condition.add({"limit-break", "limit-check"}, {
  parse = function(data, parser)
    if type(data) == "string" then
      data = {name=data}
    elseif type(data) ~= "table" then
      parser:error("invalid value type %s", type(data))
    end
    local condition_name = next(parser:getContext())
    local rule = parser:getContext("rule")
    
    if not data.key then
      data.key = rule.key
    end
    parser:assert(data.key, "limiter \"key\" missing, and no default \"key\" in rule")
    parser:assert_type(data.key, "string", "invalid limiter \"key\" type")
    
    data.key = parser:parseInterpolatedString(data.key)
    
    if not data.increment then
      if condition_name == "limit-break" then
        data.increment = 1
      elseif condition_name == "limit-check" then
        data.increment = 0
      end
    end
    data.increment = parser:assert(tonumber(data.increment), "invalid or empty \"increment\" value")
    parser:assert(data.increment >= 0, "\"increment\" must be >= 0")
    
    parser:assert(data.name, "name missing")
    parser:assert_type(data.name, "string", "invalid \"name\" type")
    
    if Component.generate_refs then
      if not rule.refs then rule.refs = {} end
      if not rule.refs.limiter then rule.refs.limiter = {} end
      table.insert(rule.refs.limiter, data.name)
    end
    
    return data
  end,
  init = function(data, thing, ruleset)
    local limiter = ruleset:findLimiter(data.name)
    if not limiter then error("unknown limiter " .. data.name) end
    data.name = nil
    data.limiter = limiter
    if data.key then
      Binding.call("string", "create", data.key)
    end
  end,
  delete = function(data)
    if data.key then
      Binding.call("string", "delete", data.key)
    end
  end,
  jsonval = function(self)
    local cpy = {}
    for k,v in pairs(self.data) do
      cpy[k]=v
    end
    if cpy.derived_key then
      cpy.derived_key = nil
      cpy.key = nil
    elseif cpy.key then
      cpy.key = cpy.key.string
    end
    cpy.name = cpy.limiter.name
    cpy.limiter = nil
    setmetatable(cpy, limit_thing_meta)
    local ret = {[self.condition]=cpy}
    return ret
  end
})

Component.condition.add(".delay", {
  parse = function(data, parser)
    parser:assert_jsontype(data, "number", "delay by <number> please")
  end
})

--some actions, too
Component.action.add("tag", {
  parse = function(data, parser)
    parser:assert_jsontype(data, "string", "\"tag\" value must be a string")
    return parser:parseInterpolatedString(data)
  end,
  init = function(data)
    Binding.call("string", "create", data)
  end,
  delete = function(data)
    Binding.call("string", "delete", data)
  end,
  jsonval = function(self)
    return {tag=self.data.string}
  end
})

Component.action.add("accept", {parse = function(data, parser)
  parser:assert_type(data, "table", "\"accept\" value must be an object")
  parser:assert_table_size(data, 0, "\"accept\" value must be empty")
end})
Component.action.add("reject", {parse = function(data, parser)
  parser:assert_type(data, "table", "\"reject\" value must be an object")
  --parser:assert_table_size(data, 0, "\"reject\" value must be empty")
end})

Component.action.add("wait", {
  parse = function(data, parser)
    parser:assert_jsontype(data, "number", "\"wait\" value must be a number")
  end
})

return Component
