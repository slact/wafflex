local Rule = require "rule"
local Binding = require "binding"

local function assert_unique_name(what, tbl, data)
  assert(data.name, ("a %s must have a name"):format(what))
  assert(not tbl[data.name], ("%s \"%s\" already exists"):format(what, data.name))
end

local function thing_name(thing)
  if type(thing)=="string" then
    return thing
  elseif type(thing) == "table" then
    return thing.name
  else
    return thing
  end
end

local ruleset_meta = { __index = {
  type="ruleset",
  
  findLimiter = function(self, name)
    return self.limiters[thing_name(name)]
  end,
  
  addLimiter = function(self, data)
    assert_unique_name("limiter", self.limiters, data)
    local limiter = setmetatable(data, self.__submeta.limiter)
    self.limiters[data.name]=limiter
    Binding.call("limiter", "create", limiter)
    return self
  end,
  
  findRule = function(self, name)
    return self.rules[thing_name(name)]
  end,
  addRule = function(self, data)
    if not data.name then
      data.name = self:uniqueName(self.rules, "rule")
    else
      assert_unique_name("rule", self.rules, data)
    end
    
    local rule =setmetatable(data, self.__submeta.rule)
    rule.ruleset = nil
    if data["if"] then
      data["if"] = Rule.condition.new(rule["if"], rule)
    end
    for _,clause in pairs{"then", "else"} do
      if data[clause] then
        local actions = {}
        for _,v in pairs(data[clause]) do
          table.insert(actions, Rule.action.new(v, self))
        end
        data[clause]=actions
      end
    end
    
    self.rules[data.name]=rule
    Binding.call("rule", "create", rule)
    return rule
  end,
  
  findList = function(self, name)
    return self.lists[thing_name(name)]
  end,
  addList = function(self, data)
    if not data.name then
      data.name = self:uniqueName(self.lists, "list")
    else
      assert_unique_name("list", self.lists, data)
    end
    
    for i, rule_data in ipairs(data.rules) do
      data.rules[i]= self:findRule(rule_data.name) or self:addRule(rule_data)
    end
    local list = setmetatable(data, self.__submeta.list)
    self.lists[data.name] = list
    Binding.call("list", "create", list)
    return list
  end,
  
  setPhaseTable = function(self, data)
    self.phases = {}
    for k,v in pairs(data) do
      local phase = {name=k, lists={}}
      for i, list in pairs(v) do
        phase.lists[i]=self:findList(list) or self:addList(list)
      end
      self.phases[k]=phase
      Binding.call("phase", "create", phase)
    end
    return self.phases
  end,
  
  uniqueName = function(self, names_tbl, prefix)
    if not self.__n then
      self.__n = 0
    else
      self.__n = self.__n + 1
    end
    local name = ("%s%i"):format(prefix, self.__n)
    if names_tbl[name] then --oh no it's not unique. try again
      return self:uniqueName(names_tbl, prefix)
    else
      return name
    end
  end
  
}}

local function newRuleset(data)
  local ruleset = setmetatable({
    rules={},
    lists={},
    limiters={},
    table={},
    name = data and data.name or nil
  }, ruleset_meta)
  
  ruleset.__submeta = {
    rule = {__index = {
      type="rule",
      --ruleset = ruleset,
      --in_lists = {},
    }},
    list = {__index = {
      type="list",
      --ruleset = ruleset,
      --in_tables = {}
    }},
    limiter = {__index = {
      type="limiter",
      --ruleset = ruleset
    }}
  }

  if not ruleset.name then ruleset.name = ruleset:uniqueName({}, "ruleset") end
  
  if data then
    --load data
    for _, v in pairs(data.limiters) do
      ruleset:addLimiter(v)
    end
    
    for _, v in pairs(data.rules) do
      ruleset:addRule(v)
    end
    
    for _, v in pairs(data.lists) do
      ruleset:addList(v)
    end
    
    ruleset:setPhaseTable(data.phases)
  end
  Binding.call("ruleset", "create", ruleset)
  return ruleset
end

local Ruleset = {new = newRuleset}

return Ruleset

