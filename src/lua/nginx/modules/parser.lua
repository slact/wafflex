local mm = require "mm"
local Rule = require "rule"
local json = require "dkjson"
local Binding = require "binding"

local function parseRulesetThing(parser, data_in, opt)
  local data = data_in[opt.key]
  parser:pushContext(data, opt.key)
  local ruleset = parser.ruleset
  
  if data then
    parser:assert_type(data, opt.type, "wrong type for ruleset %s, expected %s, got %s", opt.key, opt.type, parser:jsontype(data))
    local ret, err
    for k,v in pairs(data) do
      parser:assert_type(k, "string", "wrong key type for %s, expected string, got %s %s", opt.thing, parser:jsontype(k), tostring(k))
      ret, err = opt.parser_method(parser, v, k)
      parser:assert(ret, err)
      parser:assert(ruleset[opt.key][ret.name] == nil, "%s %s already exists", opt.thing, ret.name)
      ruleset[opt.key][ret.name]=ret
    end
  end
  parser:popContext()
  return true
end

local function inheritmetatable(dst, src)
  if type(dst) == type(src) then
    setmetatable(dst, getmetatable(src))
  end
end

local getloc; do --location caching
  local lc = setmetatable({}, {__mode="k"}) -- weak keys
  getloc = function(str, where)
    local line, pos, linepos = 1, 1, 0
    local prev = lc[str]
    if prev and prev.pos < where then
      line = prev.line
      pos = prev.pos
    end
    while true do
      pos = str:find("\n", pos, true)
      if pos and pos < where then
        line = line + 1
        linepos = pos
        pos = pos + 1
      else
        break
      end
    end
    return line, (where - linepos) -- line, column
  end
end

local function jsonmeta(what)
  return function(str, where)
    local line, column = getloc(str, where)
    return {__pos=where,__line=line, __column=column,  __jsontype = what, __jsonmeta = true}
  end
end

local class = {}

function class:jsontype(var)
  if type(var) == "table" then
    local m = getmetatable(var)
    return m and m.__jsontype or nil
  else
    return type(var)
  end
end
function class:assert(cond, err, ...)
  if not cond then self:error(err, ...) end
  return cond
end
function class:assert_type(var, expected_type, err, ...)
  if err then
    return self:assert(type(var) == expected_type, err, ...)
  else
    return self:assert(type(var) == expected_type, "expected type '%s', got '%s'", expected_type, type(var))
  end
end
function class:assert_jsontype(var, expected_type, err, ...)
  if err then
    return self:assert(self:jsontype(var) == expected_type, err, ...)
  else
    return self:assert(self:jsontype(var) == expected_type,"expected JSON type '%s', got '%s'", expected_type, self:jsontype(var))
  end
end
function class:assert_table_size(var, expected_size, err, ...)
  self:assert_type(var, "table")
  local n = 0
  for _, _ in pairs(var) do
    n = n + 1
  end
  if n ~= expected_size then
    if err then
      self:error(err, ...)
    else
      self:error("wrong table size, expected %i, got %i", expected_size, n)
    end
  end
  return var
end

function class:error(err, ...)
  
  if not err then err = "unknown error" end
  if select("#", ...) > 0 then
    err = err:format(...)
  end
  local function getlc(tbl)
    local mt = getmetatable(tbl)
    if mt.__line and mt.__column then
      return mt.__line, mt.__column
    end
  end
  
  local nested_names = {}
  
  for i=#self.ctx_stack,1,-1 do
    local cur = self.ctx_stack[i]
    if cur.name then table.insert(nested_names, cur.name) end
    local line, column = getlc(cur.ctx)
    if line and column then
      if self.name then table.insert(nested_names, self.name) end
      error(("%s at line %i column %i: %s"):format(table.concat(nested_names, " in "), line, column, err))
    end
  end
  if self.name then table.insert(nested_names, self.name) end
  if #nested_names > 0 then
    error(("%s: %s"):format(table.concat(nested_names, " in "), err))
  else
    error(err)
  end
end

function class:setInterpolationChecker(func)
  self.interpolation_checker = func
end
function class:checkInterpolatedString(str)
  if self.interpolation_checker then
    self.interpolation_checker(str, self)
  end
  return true
end

function class:pushContext(ctx, name)
  table.insert(self.ctx_stack, {ctx=ctx, name=name})
  self.context = self.ctx_stack[#self.ctx_stack]
  return self
end
function class:popContext()
  table.remove(self.ctx_stack, #self.ctx_stack)
  self.context = self.ctx_stack[#self.ctx_stack]
end
function class:getContext(name)
  if not name then return self.context and self.context.ctx end
  for i=#self.ctx_stack, 1, -1 do
    local cur = self.ctx_stack[i]
    if cur.name==name then
      return cur.ctx
    end
  end
  return nil
end

function class:parseFile(path)
  local file = assert(io.open(path, "rb")) -- r read mode and b binary mode
  local content = file:read("*a") -- *a or *all reads the whole file
  file:close()
  self.name = path
  return self:parseJSON(content, "file " .. path)
end

function class:parseJSON(json_str, json_name)
  self:assert_type(json_str, "string", "expected a JSON string")
  local data, _, err = json.decode(json_str, 1, json.null, jsonmeta("object"), jsonmeta("array"))
  self.name = json_name or self.context_name
  self.source = json_str
  if not data then
    self:error(err)
  end
  return self:parseRuleSet(data)
end

function class:parseInterpolatedString(str)
  --validate the string
  for sub in str:gmatch("%${?%w*}?") do
    if sub:sub(2,2) == "{" then
      if sub:sub(-1) ~="}" then --unterminated bracket
        self:error("missing '}' in interpolated string")
      end
      sub=sub:sub(3, -2)
      if sub == "" then
        self:error("invalid variable ${} in interpolated string")
      elseif sub:match("^%d%d+") then
        self:error("invalid regex capture \"%s\" in interpolated string. 1-9 only (nginx quirk)", sub)
      elseif sub:match("^%d.+") then
        self:error("invalid variable \"%s\" in interpolated string. can't sart with a number (nginx quirk)", sub)
      end
    else
      sub=sub:sub(2, -1)
    end
    if sub == "" then
      self:error("invalid empty variable in interpolated string")
    end
    
  end
  
  return {string = str}
end

function class:parseRuleSet(data, name)
  self.ruleset = {
    limiters= {},
    rules= {},
    lists= {},
    phases={},
    name = name
  }
  self:pushContext(data, "ruleset")
  
  self:assert_type(data, "table", "wrong type for ruleset")
  parseRulesetThing(self, data, {
    thing="limiter", key="limiters", type="table",
    parser_method= self.parseLimiter
  })
  self:checkLimiters(data.limiters)
  
  parseRulesetThing(self, data, {
    thing="rule", key="rules",  type="table",
    parser_method=function(self, data, name)
      self:pushContext(data, "rule")
      self:assert(type(data) ~= "string", ("named rule \"%s\" cannot be a string referring to another named rule \"%s\""):format(name, tostring(data)))
      self:popContext()
      return self:parseRule(data, name)
    end
  })
  
  parseRulesetThing(self, data, {
    thing="list", key="lists",  type="table",
    parser_method= self.parseRuleList
  })
  
  self.ruleset.phases = self:parsePhaseTable(data.phases)
  --convert debug metatable data to __dbg table whenever possible
  local function move_dbg_data(tbl)
    local meta = getmetatable(tbl)
    if meta and meta.__jsonmeta then
      if not meta.__line then
        mm(tbl)
      end
      setmetatable(tbl, {line=meta.__line, col=meta.__column})
    end
    for _, v in pairs(tbl) do
      if type(v) == "table" then
        move_dbg_data(v)
      end
    end
  end
  move_dbg_data(self.ruleset)
  
  return self.ruleset
end

function class:parsePhaseTable(data)
  self:assert(data ~= nil, "missing phase table (\"phases\" attribute)")
  self:assert_jsontype(data, "object", "phase table must be an object")
  self:pushContext(data, "phase table")
  
  for phase_name, phase_data in pairs(data) do
    self:assert_type(phase_name, "string", "phase table entries must be strings")
    if self:jsontype(phase_data) == "array" then
      for i, list in ipairs(phase_data) do
        if type(list)=="string" or self:jsontype(list) == "array" or self:jsontype(list) == "object" then
          phase_data[i]=self:parseRuleList(list)
        else
          self:error("invalid rule list type: %s", self:jsontype(list))
        end
      end
    elseif type(phase_data) == "string" then
      --singe named list
      data[phase_name]={ self:parseRuleList(phase_data) }
    elseif self:jsontype(phase_data)=="object" then
      --single long-form list
      data[phase_name]=self:parseRuleList(phase_data)
    end
  end
  
  self:popContext()
  
  return data
end

function class:parseRuleList(data, name)
  if type(data)=="string" then
    self:assert(self.ruleset.lists[data], ([[named list "%s" not found]]):format(data))
    return self.ruleset.lists[data]
  end
  self:pushContext(data, "list")
  local list
  if self:jsontype(data) == "object" then
    if name then
      self:assert(name == data.name, "rule list 'name' attribute must match outside list name")
    else
      name = tostring(data.name)
    end
    data = data.rules
  end
  self:assert_jsontype(data, "array", "rule list must be an array")
  local rules = {}
  for _,v in ipairs(data) do
    table.insert(rules, self:parseRule(v))
  end
  self:popContext()
  list = {name=name, rules=rules}
  inheritmetatable(list, data)
  return list
end

function class:parseRule(data, name)
  self:pushContext(data, "rule")
  local rule
  if type(data) == "string" then
    rule = self.ruleset.rules[data]
    self:assert(rule, ([[named rule "%s" not found]]):format(data))
    self:popContext()
    return rule
  end
  self:assert_type(data, "table", "invalid rule data type: " .. type(data))
  self:assert_jsontype(data, "object", ("invalid rule data type: %s"):format(self:jsontype(data)))
  
  if ((data["if"] or data["if-any"] or data["if-all"]) or data["then"]) then
    self:assert(not data["always"], [["always" clause can't be present in if/then rule]])
    self:assert(not data["switch"], [["switch" clause can't be present in if/then rule]])
  end
  
  if data["then"] then
    if (data["if"] and (data["if-any"] or data["if-all"])) or (data["if-any"] and data["if-all"]) then
      self:error("only one of \"if\", \"if-any\" or \"if-all\" allowed in if/then rule")
    end
    
    local condition
    if data["if"] then
      condition = self:parseCondition(data["if"])
    elseif data["if-any"] or data["if-all"] then
      local conditions = {}
      for _, v in ipairs(data["if-any"] or data["if-all"]) do
        condition = self:assert(self:parseCondition(v))
        table.insert(conditions, condition)
      end
      condition = {[(data["if-any"] and "any" or "all")]=conditions}
      inheritmetatable(condition, data["if"] or data["if-any"] or data["if-all"])
    end
    rule = {["if"]=condition, ["then"]=data["then"], ["else"]=data["else"], name=data["name"] or name, info=data["info"], key=data["key"]}
  elseif data["always"] then
    rule = {["if"]={["true"]={}}, ["then"]=data["always"], name=data["name"] or name, info=data["info"], key=data["key"]}
  elseif next(data) == nil then
    self:error("empty rule not allowed")
  else
    self:error("rule must have at least an \"if\", \"then\", or \"always\" attribute")
  end
  if rule["if"] then
    rule["then"] = self:parseActions(rule["then"])
    rule["else"] = self:parseActions(rule["else"])
  end
  if rule.key then
    rule.key = self:parseInterpolatedString(rule.key)
  end
  
  self:popContext()
  --reuse metatable for debugging purposes
  inheritmetatable(rule, data)
  return rule
end

function class:parseCondition(data)
  self:pushContext(data, "condition")
  local condition
  if type(data) == "string" then
    condition = {[data]={}}
  elseif type(data) == "table" then
    self:assert_jsontype(data, "object", "condition cannot be an array, must be an object")
    self:assert_table_size(data, 1, "condition object must have exactly one attribute (the condition name)")
    condition = data
  else
    self:error("wrong type (%s) for condition", type(data))
  end
  self:popContext()
  -- be more specific with condition name
  self:pushContext(data, "condition " .. (next(condition)))
  condition = Rule.condition.parse(condition, self)
  self:popContext()
  inheritmetatable(condition, data)
  return condition
end
  
function class:parseAction(data)
  self:pushContext(data, "action")
  local action
  if type(data) == "string" then
    action = {[data]={}}
  elseif self:jsontype(data) == "object" then
    self:assert(next(data, next(data)) == nil, "action object must have only 1 attribute -- the action name")
    action = data
  else
    self:error("action must be string on 1-attribute object, but instead was a %s", self:jsontype(data))
  end
  self:popContext()
  --we can be more specific about the action name now
  self:pushContext(data, "action " .. (next(action)))
  inheritmetatable(action, data)
  action = Rule.action.parse(action, self)
  self:popContext()
  return action
end

function class:parseActions(data)
  if data == nil then
    return {}
  end
  self:pushContext(data) --no context name plz
  local actions = {}
  inheritmetatable(actions, data)
  if self:jsontype(data) == "object" or type(data)=="string" or (#data == 0 and next(data) ~= nil) then
    table.insert(actions, self:parseAction(data))
  elseif type(data) == "table" then
    for _, v in ipairs(data) do
      table.insert(actions, self:parseAction(v))
    end
  end
  self:popContext()
  return actions
end

function class:parseTimeInterval(data, err)
  if err then err = " for " .. err end
  local typ = self:jsontype(data)
  if typ == "number" then
    return data
  elseif typ == "string" then
    local num, unit = data:match("^([%d.]+)(%w*)")
    local scale
    num = tonumber(num)
    self:assert(num and unit, ("invalid time string \"%s\"%s"):format(data, err))
    if unit == "ms" or unit:match("^millisec(ond(s?))?") then
      scale = .01
    elseif unit == "" or unit == "s" or unit:match("^sec(ond(s?))?") then
      scale = 1
    elseif unit == "m" or unit:match("^min(ute(s)?)?") then
      scale = 60
    elseif unit == "h" or unit:match("^hour(s?)") then
      scale = 3600
    elseif unit == "d" or unit:match("^day(s)?") then
      scale = 86400
    elseif unit == "w" or unit == "wk" or unit:match("^week(s)?") then
      scale = 604800
    elseif unit == "M" or unit:match("^month(s)?") then
      scale = 2628001
    else
      self:error("unknown time unit \"%s\"%s", unit, err)
    end
    return num * scale
  else
    self:error("invalid time inteval type \"%s\"%s", self:jsontype(data), err)
  end
end

function class:parseLimiter(data, name)
  self:pushContext(data, "limiter")
  
  if not data.name then data.name = name end
  data.interval = self:parseTimeInterval(data.interval, "interval value")
  self:assert(data.limit, "missing \"limit\" value")
  data.limit = self:assert(tonumber(data.limit), "invalid \"limit\" value, must be a number")
  if data.sync_steps then
    data.sync_steps = self:assert(tonumber(data.sync_steps), "invalid \"sync-steps\" value")
  end
  if data.burst then 
    self:assert_type(data.burst, "string", "invalid \"burst\" value type")
  end
  if data["burst-expire"] then
    data.burst_expire = self:parseTimeInterval(data["burst-expire"], "burst_expire value")
    data["burst-expire"] = nil
  end
  
  self:assert_type(data.name, "string", "invalid limiter name")
  self:popContext()
  return data
end
function class:checkLimiters(data)
  self:pushContext(data, "limiters")
  for k, v in pairs(data) do
    self:pushContext(v, ("limiter \"%s\""):format(v.name))
    if v.burst then
      --make sure the burst value refers to a known limiter
      self:assert(data[v.burst], ("limiter references unknown burst limiter \"%s\""):format(v.burst))
    end
    self:popContext()
  end
  self:popContext()
end

local function newparser()
  local parser = {
    name = "<?>",
    ctx_stack = {}
  }
  
  setmetatable(parser, {__index = class})
  return parser
end

local Parser = {new = newparser}

return Parser
