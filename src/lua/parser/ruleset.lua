local mm = require "mm"
local Rule = require "rule"

local function inject_event_listeners(mt)
  mt.__index.addListener = function(self, event_name, callback)
    if not self.__event_listeners then 
      self.__event_listeners = {}
    end
    if not self.__event_listeners[event_name] then
      self.__event_listeners[event_name] = {}
    end
    table.insert(self.__event_listeners[event_name], callback)
    return self
  end
  
  mt.__index.removeListener = function(self, event_name, callback)
    if not self.__event_listeners or not self.__event_listeners[event_name] then 
      return nil
    end
    for i,v in ipairs(self.__event_listeners[event_name]) do
      if v == callback then
        table.remove(self.__event_listeners[event_name], i)
        return self
      end
    end
    return self
  end
  
  mt.__index.emit = function(self, event_name, ...)
    if not self.__event_listeners or not self.__event_listeners[event_name] then
      return nil
    end
    for i,listener in ipairs(self.__event_listeners[event_name]) do
      listener(self, ...)
    end
    return self
  end
  return mt
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
    error(("a thing has no name (wrong type %s)"):format(type(thing)))
  end
end

local ruleset_meta = inject_event_listeners({ __index = {
  type="ruleset",
  
  findLimiter = function(self, name)
    return self.limiters[thing_name(name)]
  end,
  
  addLimiter = function(self, data)
    assert_unique_name("limiter", self.limiters, data)
    local limiter = setmetatable(data, self.__submeta.limiter)
    self.limiters[data.name]=limiter
    return self
  end,
  
  findRule = function(self, name)
    return self.rules[thing_name(name)]
  end,
  addRule = function(self, data)
    assert_unique_name("rule", self.rules, data)
    
    local rule =setmetatable(data, self.__submeta.rule)
    rule.ruleset = nil
    if data["if"] then 
      data["if"] = Rule.condition.new(rule["if"], rule)
    end
    if data["then"] then
      local actions = {}
      for i,v in pairs(data["then"]) do
        table.insert(actions, Rule.action.new(v, self))
      end
      data["then"]=actions
    end
    self.rules[data.name]=rule
    return rule
  end,
  
  findList = function(self, name)
    return self.lists[thing_name(name)]
  end,
  addList = function(self, data)
    assert_unique_name("list", self.lists, data)
    
    for i, rule_data in ipairs(data.rules) do
      if not rule_data.name then
        rule_data.name = self:uniqueName()
      end
      data.rules[i]= self:findRule(rule_data.name) or self:addRule(rule_data)
    end
    local list = setmetatable(data, self.__submeta.list)
    self.lists[data.name] = list
    return list
  end,
  
  setTable = function(self, data)
    self.table = {}
    for k,v in pairs(data) do
      for i, list in pairs(v) do
        v[i]=self:findList(list) or self:addList(list)
      end
      self.table[k]=v
    end
    return self.table
  end,
  
  uniqueName = function(self)
    if not self.__n then 
      self.__n = 0
    else 
      self.__n = self.__n + 1
    end
    return tostring(self.__n)
  end
  
}})

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
      ruleset = ruleset
    }},
    list = {__index = {
      type="list",
      ruleset = ruleset
    }},
    limiter = {__index = {
      type="limiter",
      ruleset = ruleset
    }}
  }

  if not ruleset.name then ruleset.name = ruleset:uniqueName() end

  if data then
    --load data
    for k, v in pairs(data.limiters) do
      ruleset:addLimiter(v)
    end
    
    for k, v in pairs(data.rules) do
      ruleset:addRule(v)
    end
    
    for k, v in pairs(data.lists) do
      ruleset:addList(v)
    end
    
    ruleset:setTable(data.table)
    
  end
  return ruleset
end

_G['Ruleset']={new = newRuleset}
