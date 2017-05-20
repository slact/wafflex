local mm = require "mm"
local Rule = require "rule"
local json = require "dkjson"

local function parseRulesetThing(parser, data_in, opt)
  parser:pushContext(data_in, opt.key)
  local data = data_in[opt.key]
  local ruleset = parser.ruleset
  
  if data then
    parser:assert_type(data, opt.type, "wrong type for ruleset " .. opt.key)
    local ret, err
    for k,v in pairs(data) do
      print("yep", k,v)
      ret, err = opt.parser_method(parser, v, k)
      parser:assert(ret, err)
      --assert(ret.id, ("failed to generate id for %s"):format(opt.thing))
      print("name is", ret.name)
      parser:assert(ruleset[opt.key][ret.name] == nil, ("%s %s already exists"):format(opt.thing, ret.name))
      print(opt.key, ret.name)
      ruleset[opt.key][ret.name]=ret
    end
  end
  parser:popContext()
  return true
end

local function jsonmeta(what)
  return function(str, where)
    return {__pos=where, __jsontype = what}
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
function class:assert(cond, err)
  if not cond then self:error(err) end
  return cond
end
function class:assert_type(var, expected_type, err)
  return self:assert(type(var) == expected_type, err or ("expected type '%s', got '%s'"):format(expected_type, type(var)))
end
function class:assert_jsontype(var, expected_type, err)
  return self:assert(self:jsontype(var) == expected_type,
    err or ("expected JSON type '%s', got '%s'"):format(expected_type, self:jsontype(var))
  )
end
function class:assert_table_size(var, expected_size, err)
  self:assert_type(var, "table")
  local n = 0
  for _, _ in pairs(var) do
    n = n + 1
  end
  if n ~= expected_size then
    self:error(err or ("wrong table size, expected %i, got %i"):format(expected_size, n))
  end
  return var
end
function class:error(err)
  local getloc = function(str, where)
    local line, pos, linepos = 1, 1, 0
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
    return "line " .. line .. ", column " .. (where - linepos)
  end
  
  local getpos = function(ctx)
    local meta = getmetatable(ctx)
    if meta then
      return meta.__pos
    end
  end
  
  if not err then err = "unknown error" end
  
  local nested_names = {}
  
  for i=#self.ctx_stack,1,-1 do
    local cur = self.ctx_stack[i]
    if cur.name then table.insert(nested_names, cur.name) end
    local pos = getpos(cur.ctx)
    if pos then
      if self.name then table.insert(nested_names, self.name) end
      error(("%s at %s: %s"):format(table.concat(nested_names, " in "), getloc(self.source, pos), err))
    end
  end
  if self.name then table.insert(nested_names, self.name) end
  if #nested_names > 0 then
    error(("%s: %s"):format(table.concat(nested_names, " in "), err))
  else
    error(err)
  end
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

function class:parseRuleSet(data, name)
  self.ruleset = {
    limiters= {},
    rules= {},
    lists= {},
    table= {},
    name = name
  }
  self:pushContext(data, "ruleset")
  
  self:assert_type(data, "table", "wrong type for ruleset")
  parseRulesetThing(self, data, {
    thing="limiter", key="limiters", type="table",
    parser_method= self.parseLimiter
  })
  
  parseRulesetThing(self, data, {
    thing="rule", key="rules",  type="table",
    parser_method= self.parseRule
  })
  
  parseRulesetThing(self, data, {
    thing="list", key="lists",  type="table",
    parser_method= self.parseRuleList
  })
  
  self.ruleset.table = self:parseRuleTable(data.table)
  return self.ruleset
end

function class:parseRuleTable(data)
  self:pushContext(data, "ruletable")
  self:assert_jsontype(data, "object", "rule table must be an object")
  local rule_table = {}
  
  for k,v in pairs(data) do
    self:assert_type(k, "string", "rule table entries must be strings")
    if self:jsontype(v) == "array" then
      --array of lists
      local lists = {}
      for _, vv in ipairs(v) do
        if type(vv)=="string" or self:jsontype(vv) == "array" or self:jsontype(vv) == "object" then
          table.insert(lists, self:parseRuleList(vv))
        else
          self:error(("invalid rule list type: %s"):format(self:jsontype(vv)))
        end
      end
      rule_table[k]=lists
    elseif type(v) == "string" then
      --singe named list
      rule_table[k]={ self:parseRuleList(v) }
    elseif self:jsontype(v)=="object" then
      --single long-form list
      rule_table[k]=self:parseRuleList(v)
    end
  end
  
  self:popContext()
  return rule_table
end

function class:parseRuleList(data, name)
  self:pushContext(data, "list")
  if type(data)=="string" then
    self:assert(self.ruleset.lists[data], ([[named list "%s" not found]]):format(data))
    self:popContext()
    return self.ruleset.lists[data]
  end
  
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
  return {name=name, rules=rules}
end

local function clear_json_meta(data)
  if type(data) == "table" then
    local meta = getmetatable(data)
    if meta and meta.__jsontype then
      setmetatable(data, nil)
    end
    for _, v in pairs(data) do
      clear_json_meta(v)
    end
  end
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
        mm(condition)
        table.insert(conditions, condition)
      end
      condition = {[(data["if-any"] and "any" or "all")]=conditions}
      mm(condition)
    end
    rule = {["if"]=condition, ["then"]=data["then"], name=data["name"] or name, key=data["key"]}
    
  elseif data["always"] then
    rule = {["if"]={["true"]={}}, ["then"]=data["always"], name=data["name"] or name, key=data["key"]}
  else
    self:error("something went wrong parsing rule")
  end
  if self:jsontype(rule["then"]) ~= "array" or (#rule["then"] == 0 and next(rule["then"]) ~= nil) then
    rule["then"] = { rule["then"] }
  end
  
  self:popContext()
  clear_json_meta(rule)
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
    self:error(("wrong type (%s) for condition"):format(type(data)))
  end
  condition = Rule.condition.parse(condition, self)
  self:popContext()
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
    self:error(("action must be string on 1-attribute object, but instead was a %s"):format(self:jsontype(data)))
  end
  action = Rule.action.parse(action, self)
  self:popContext()
  return action
end

function class:parseActions(data)
  self:pushContext(data) --no context name plz
  local actions = {}
  if self:jsontype(data) == "array" then
    for _, v in ipairs(data) do
      table.insert(actions, self:parseAction(v))
    end
  else
    actions = { self:parseAction(data) }
  end
  self:popContext()
  return actions
end

function class:parseLimiter(data, name)
  self:pushContext(data)
  print("parseLimiter", data, name)
  self:popContext()
  return nil, "nonono"
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
