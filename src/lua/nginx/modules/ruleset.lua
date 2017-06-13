local RuleComponent = require "rulecomponent"
local Binding = require "binding" or {call=function()end}
local json = require "dkjson"
local mm = require "mm"

local Module -- forward declaration

local tcopy = function(tbl, skipmetatable)
  local cpy = {}
  for k,v in pairs(tbl) do
    cpy[k]=v
  end
  if skipmetatable then
    return cpy
  else
    return setmetatable(cpy, getmetatable(tbl))
  end
end

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

local function table_copy(tbl, exclude_keys)
  local cpy = {}
  setmetatable(cpy, getmetatable(tbl))
  for k,v in pairs(tbl) do
    if not exclude_keys[k] then
      cpy[k]=v
    end
  end
  return cpy
end

local function sorted_keys(tbl)
  local keys = {}
  for k in pairs(tbl) do
    table.insert(keys, k)
  end
  table.sort(keys)
  return keys
end

local Ruleset = {} --forward declaration

local mt = {}

mt.ruleset = {
  __index = Ruleset,
  __jsonorder = {"name", "info", "phases", "limiters", "lists", "rules"}
}
  
mt.phase = {
  new = function(name, data)
    return setmetatable({name=name, lists=data}, mt.phase)
  end,
  __index = {
    toJSON = function(self)
      return json.encode(self, {indent=true})
    end
  },
  __jsonval = function(self)
    local lists = {}
    for _, list in pairs(self.lists) do
      table.insert(lists, list.name)
    end
    return lists
  end
}
  
mt.rules={__jsonorder=sorted_keys}
mt.lists={__jsonorder=sorted_keys}
mt.limiters={__jsonorder=sorted_keys}
  
mt.list = {
  new = function(data)
    return setmetatable(data, mt.list)
  end,
  __index = {
    toJSON = function(self)
      return json.encode(self, {indent=true})
    end
  },
  __jsonorder = {"name", "info", "rules"},
  __jsonval = function(self)
    local rules = {}
    for _, rule in pairs(self.rules) do
      table.insert(rules, rule.name)
    end
    
    if self.info then
      return setmetatable({info=self.info, rules=rules}, getmetatable(self))
    else
      return setmetatable(rules, getmetatable(self))
    end
  end
}
  
mt.rule = {
  new = function(data, ruleset)
    local rule =setmetatable(data, mt.rule)
    if data["if"] then
      data["if"] = RuleComponent.condition.new(rule["if"], ruleset)
    end
    for _,clause in pairs{"then", "else"} do
      if data[clause] then
        local actions = {}
        for _,v in pairs(data[clause]) do
          table.insert(actions, RuleComponent.action.new(v, ruleset))
        end
        data[clause]=actions
      end
    end
    return data
  end,
  __index = {
    toJSON = function(self)
      return json.encode(self, {indent=true})
    end
  },
  __jsonorder = {"name", "info", "key", "if", "if-any", "if-all", "then", "else"},
  __jsonval = function(self)
    if #self["else"] <= 1 or #self["then"] <= 1 or self.key or self["if"].condition == "any" or self["if"].condition == "all" or self["refs"] then
      local ret = tcopy(self)
      if self["if"].condition == "any" then ret["if-any"] = ret["if"].data; ret["if"] = nil end
      if self["if"].condition == "all" then ret["if-all"] = ret["if"].data; ret["if"] = nil end
      if #self["then"] == 0 then ret["then"] = nil end
      if #self["then"] == 1 then ret["then"] = self["then"][1] end
      if #self["else"] == 0 then ret["else"] = nil end
      if #self["else"] == 1 then ret["else"] = self["else"][1] end
      if self.key then self.key = self.key.string end
      if ret.refs then ret.refs = nil end
      return ret
    end
    return self
  end
}
  
mt.limiter = {
  new = function(data)
    return setmetatable(data, mt.limiter)
  end,
  __index = {
    toJSON = function(self)
      return json.encode(self, {indent=true})
    end
  },
  __jsonorder = {"name", "info", "limit", "interval", "burst", "burst-expire"},
  __jsonval = function(self)
    if self.burst then
      local cpy = tcopy(self)
      cpy.burst = cpy.burst["name"]
      return cpy
    end
    return self
  end
}

function Ruleset:findLimiter(name)
  return self.limiters[thing_name(name)]
end

function Ruleset:addLimiter(data, limiters_in)
  if data.__already_loaded_as_burst_limiter then
    data.__already_loaded_as_burst_limiter = nil
    return nil
  end
  assert_unique_name("limiter", self.limiters, data)
  local limiter = mt.limiter.new(data)
  self.limiters[data.name]=limiter
  if limiter.burst then
    local burst_limiter = self:findLimiter(limiter.burst)
    if not burst_limiter then
      burst_limiter = self:addLimiter(limiters_in[limiter.burst], limiters_in)
      limiters_in[limiter.burst].__already_loaded_as_burst_limiter = true
      limiter.burst = burst_limiter
    end
  end
  Binding.call("limiter", "create", limiter)
  return limiter
end

function Ruleset:deleteLimiter(limiter)
  assert(self.limiters[limiter.name] == limiter, "tried deleting unexpected limiter of the same name")
  --TODO: check which rules use this limiter
  self.limiters[limiter.name] = nil
  
  Binding.call("limiter", "delete", limiter)
end

function Ruleset:findRule(name)
  return self.rules[thing_name(name)]
end

function Ruleset:addRule(data)
  if not data.name then
    data.name = self:uniqueName("rule")
  else
    assert_unique_name("rule", self.rules, data)
  end
  
  local rule = mt.rule.new(data, self)
  
  self.rules[data.name]=rule
  Binding.call("rule", "create", rule)
  return rule
end

function Ruleset:deleteRule(rule)
  assert(self.rules[rule.name] == rule, "tried deleting unexpected rule of the same name")
  for list_name, list in pairs(self.lists) do
    for _, list_rule in ipairs(list) do
      assert(list_rule ~= rule, ("can't delete rule \"%s\", it's used in list \"%s\""):format(rule.name, list_name))
    end
  end
  
  self.rules[rule.name] = nil
  
  if rule["if"] then
    RuleComponent.condition.delete(rule["if"], self)
  end
  
  for _,clause in pairs{"then", "else"} do
    if rule[clause] then
      local actions = rule[clause]
      for _,action in pairs(actions) do
        RuleComponent.action.delete(action, self)
      end
      rule[clause]={}
    end
  end
  Binding.call("rule", "delete", rule)
end

function Ruleset:findList(name)
  return self.lists[thing_name(name)]
end

function Ruleset:addList(data)
  if not data.name then
    data.name = self:uniqueName("list")
  else
    assert_unique_name("list", self.lists, data)
  end
  
  for i, rule_data in ipairs(data.rules) do
    data.rules[i]= self:findRule(rule_data.name) or self:addRule(rule_data)
  end
  local list = mt.list.new(data)
  self.lists[data.name] = list
  Binding.call("list", "create", list)
  return list
end

function Ruleset:deleteList(list)
  assert(self.lists[list.name] == list, "tried deleting unexpected list of the same name")
  for phase_name, phase in pairs(self.phases) do
    for _, phase_list in ipairs(phase) do
      assert(phase_list ~= list, ("can't delete list \"%s\", it's used in phase \"%s\""):format(list.name, phase_name))
    end
  end
  self.lists[list.name] = nil
  Binding.call("list", "delete", list)
end

function Ruleset:setPhaseTable(data)
  if self.phases then
    for _, phase in pairs(self.phases) do
      Binding.call("phase", "delete", phase)
    end
  end
  self.phases = {}
  for k,v in pairs(data) do
    local phase = mt.phase.new(k, data)
    for i, list in pairs(v) do
      phase.lists[i]=self:findList(list) or self:addList(list)
    end
    self.phases[k]=phase
    Binding.call("phase", "create", phase)
  end
  return self.phases
end

function Ruleset:uniqueName(thing)
  local uniqs = {
    ruleset = {},
    rule = self.rules,
    list = self.lists,
    limiter = self.limiters
  }
  local checktbl = uniqs[thing]
  if checktbl == nil then
    error("don't knoq how to generate unique name for " .. tostring(thing))
  end
  
  return assert(Module.uniqueName(thing, checktbl, self), "unique name can't be nil")
end

function Ruleset:toJSON()
  local rs = {
    name = self.name,
    info = self.info,
    rules = tcopy(self.rules),
    lists = tcopy(self.lists),
    limiters = tcopy(self.limiters),
    phases = self.phases,
  }
  setmetatable(rs, mt.ruleset)
  
  --remove inlined names from rules, lists, and limiters
  local excludes = {name=true}
  for _, thingsname in ipairs({"rules", "lists", "limiters", "phases"}) do
    local things = rs[thingsname] or {}
    for k, thing in pairs(things) do
      things[k]=table_copy(thing, excludes)
    end
  end
  
  return json.encode(rs, {indent=true})
end

function Ruleset:destroy()
  --clear phases
  self:setPhaseTable({})
  
  --clear lists
  for _, list in pairs(self.lists) do
    self:deleteList(list)
  end
  
  --clear rules
  for _, rule in pairs(self.rules) do
    self:deleteRule(rule)
  end
  
  --clear limiters
  for _, limiter in pairs(self.limiters) do
    self:deleteLimiter(limiter)
  end
  
  Binding.call("ruleset", "delete", self)
  
end

Module = {
  new = function(data)
    local ruleset = setmetatable({
      rules=setmetatable({}, mt.rules),
      lists=setmetatable({}, mt.lists),
      limiters=setmetatable({}, mt.limiters),
      phases={},
      name = data and data.name or nil
    }, mt.ruleset)

    if not ruleset.name then ruleset.name = ruleset:uniqueName("ruleset") end
    
    if data then
      --load data
      for _, v in pairs(data.limiters) do
        ruleset:addLimiter(v, data.limiters)
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
  end,
  newLimiter = mt.limiter.new,
  newPhase = mt.phase.new,
  newList = mt.list.new,
  newRule = mt.rule.new,
  
  uniqueName = function(thingname, checktbl, ruleset)
    error("uniqueName must be configured outside the Ruleset module")
  end
}

return Module

