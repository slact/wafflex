local Rule = require "rule"
local Binding = require "binding"
local json = require "dkjson"

local tcopy = function(tbl)
  local cpy = {}
  for k,v in pairs(tbl) do
    cpy[k]=v
  end
  return setmetatable(cpy, getmetatable(tbl))
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

local ruleset_meta = { __index = {
  type="ruleset",
  
  findLimiter = function(self, name)
    return self.limiters[thing_name(name)]
  end,
  
  addLimiter = function(self, data, limiters_in)
    if data.__already_loaded_as_burst_limiter then
      data.__already_loaded_as_burst_limiter = nil
      return nil
    end
    assert_unique_name("limiter", self.limiters, data)
    local limiter = setmetatable(data, self.__submeta.limiter)
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
  end,
  deleteLimiter = function(self, limiter)
    assert(self.limiters[limiter.name] == limiter, "tried deleting unexpected list of the same name")
    --TODO: check which rules use this limiter
    self.limiters[limiter.name] = nil
    
    Binding.call("limiter", "delete", limiter)
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
      data["if"] = Rule.condition.new(rule["if"], self)
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
  deleteRule = function(self, rule)
    assert(self.rules[rule.name] == rule, "tried deleting unexpected list of the same name")
    for list_name, list in pairs(self.lists) do
      for _, list_rule in ipairs(list) do
        assert(list_rule ~= rule, ("can't delete rule \"%s\", it's used in list \"%s\""):format(rule.name, list_name))
      end
    end
    
    self.rules[rule.name] = nil
    
    if rule["if"] then
      Rule.condition.delete(rule["if"], self)
    end
    
    for _,clause in pairs{"then", "else"} do
      if rule[clause] then
        local actions = rule[clause]
        for _,action in pairs(actions) do
          Rule.action.delete(action, self)
        end
        rule[clause]={}
      end
    end
    Binding.call("rule", "delete", rule)
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
  deleteList = function(self, list)
    assert(self.lists[list.name] == list, "tried deleting unexpected list of the same name")
    for phase_name, phase in pairs(self.phases) do
      for _, phase_list in ipairs(phase) do
        assert(phase_list ~= list, ("can't delete list \"%s\", it's used in phase \"%s\""):format(list.name, phase_name))
      end
    end
    self.lists[list.name] = nil
    Binding.call("list", "delete", list)
  end,
  
  setPhaseTable = function(self, data)
    local prev_phases = self.phases
    if self.phases then
      for _, phase in pairs(self.phases) do
        Binding.call("phase", "delete", phase)
      end
    end
    self.phases = {}
    for k,v in pairs(data) do
      local phase = setmetatable({name=k, lists={}}, self.__submeta.phase)
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
  end,
  
  toJSON = function(self)
    local rs = {
      name = self.name,
      info = self.info,
      rules = tcopy(self.rules),
      lists = tcopy(self.lists),
      limiters = tcopy(self.limiters),
      phases = self.phases,
    }
    setmetatable(rs, self.__submeta.ruleset)
    
    --remove inlined names from rules, lists, and limiters
    local excludes = {name=true}
    for _, thingsname in ipairs({"rules", "lists", "limiters", "phases"}) do
      local things = rs[thingsname] or {}
      local cpy
      for k, thing in pairs(things) do
        things[k]=table_copy(thing, excludes)
      end
    end
    
    setmetatable(rs.lists, {__index=listorder})
    
    return json.encode(rs, {indent=true})
  end,
  
  destroy = function(self)
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
}}

local function sorted_keys(tbl)
  local keys = {}
  for k, v in pairs(tbl) do
    table.insert(keys, k)
  end
  table.sort(keys)
  return keys
end

local function newRuleset(data)
  local ruleset = setmetatable({
    rules=setmetatable({}, {__jsonorder=sorted_keys}),
    lists=setmetatable({}, {__jsonorder=sorted_keys}),
    limiters=setmetatable({}, {__jsonorder=sorted_keys}),
    phases={},
    name = data and data.name or nil
  }, ruleset_meta)
  
  ruleset.__submeta = {
    ruleset = {
      __jsonorder = {"name", "info", "phases", "limiters", "lists", "rules"}
    },
    phase = {
      __jsonval = function(self) 
        local lists = {}
        for _, list in pairs(self.lists) do
          table.insert(lists, list.name)
        end
        return lists
      end
    },
    list = {
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
    },
    
    rule = {
      __jsonorder = {"name", "info", "key", "if", "if-any", "if-all", "then", "else"},
      __jsonval = function(self)
        if #self["else"] == 0 or #self["then"] == 0 or self.key then
          local ret = tcopy(self)
          if #self["then"] == 0 then ret["then"] = nil end
          if #self["else"] == 0 then ret["else"] = nil end
          if self.key then self.key = self.key.string end
          return ret
        end
        return self
      end
    },
    limiter = {
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
  }

  if not ruleset.name then ruleset.name = ruleset:uniqueName({}, "ruleset") end
  
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
end

local Ruleset = {new = newRuleset}

return Ruleset

