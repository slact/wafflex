local mm = require "mm"
local Rule = require "rule"

local function jsontype(var)
  if type(var) == "table" then
    local m = getmetatable(var)
    return m and m.__jsontype or nil
  else
    return type(var)
  end
end

local function assert_type(var, expected_type, err)
  assert(type(var) == expected_type, err)
end

local function assert_jsontype(var, expected_type, err)
  assert(jsontype(var) == expected_type, err)
end

local function assert_table_size(var, sz, err)
  local n = 0
  for _, _ in pairs(var) do
    n=n+1
  end
  assert(sz == n, err)
end

local function parseRulesetThing(data_in, ruleset, opt)
  local data = data_in[opt.key]
  if data then
    assert_type(data, opt.type, "wrong type for ruleset " .. opt.key)
    local ret, err
    for k,v in pairs(data) do
      print("yep", k,v)
      ret, err = opt.parser_method(opt.parser, v, k)
      assert(ret, err)
      --assert(ret.id, ("failed to generate id for %s"):format(opt.thing))
      print("name is", ret.name)
      assert(ruleset[opt.key][ret.name] == nil, ("%s %s already exists"):format(opt.thing, ret.name))
      print(opt.key, ret.name)
      ruleset[opt.key][ret.name]=ret
    end
  end
  return true
end

local function newparser()
  local parser = {}
  setmetatable(parser, {__index = {
    
    assert_type = assert_type,
    jsontype = jsontype,
    assert_jsontype = assert_jsontype,
    
    parseRuleSet = function(self, data, name)
      self.ruleset = {
        limiters= {},
        rules= {},
        lists= {},
        table= {},
        name = name
      }
      
      assert_type(data, "table", "wrong type for ruleset")
      
      parseRulesetThing(data, self.ruleset, {
        thing="limiter", key="limiters", type="table",
        parser= self, parser_method= self.parseLimiter
      })
      
      parseRulesetThing(data, self.ruleset, {
        thing="rule", key="rules",  type="table",
        parser= self, parser_method= self.parseRule
      })
      
      parseRulesetThing(data, self.ruleset, {
        thing="list", key="lists",  type="table",
        parser= self, parser_method= self.parseRuleList
      })
      
      self.ruleset.table = self:parseRuleTable(data.table)
      return self.ruleset
    end,
    
    parseRuleTable = function(self, data)
      mm(data)
      assert_jsontype(data, "object", "rule table must be an object")
      local rule_table = {}
      
      for k,v in pairs(data) do
        assert_type(k, "string", "rule table entries must be strings")
        if jsontype(v) == "array" then
          --array of lists
          local lists = {}
          for _, vv in ipairs(v) do
            if type(vv)=="string" or jsontype(vv) == "array" or jsontype(vv) == "object" then
              table.insert(lists, self:parseRuleList(vv))
            else
              error(("invalid rule list type: %s"):format(jsontype(vv)))
            end
          end
          rule_table[k]=lists
        elseif type(v) == "string" then
          --singe named list
          rule_table[k]={ self:parseRuleList(v) }
        elseif jsontype(v)=="object" then
          --single long-form list
          rule_table[k]=self:parseRuleList(v)
        end
      end
      
      return rule_table
    end,
    
    parseRuleList = function(self, data, name)
      if type(data)=="string" then
        assert(self.ruleset.lists[data], ([[named list "%s" not found]]):format(data))
        return self.ruleset.lists[data]
      end
      
      if jsontype(data) == "object" then
        if name then
          assert(name == data.name, "rule list 'name' attribute must match outside list name")
        else
          name = tostring(data.name)
        end
        data = data.rules
      end
      assert_jsontype(data, "array", "rule list must be an array")
      local rules = {}
      for _,v in ipairs(data) do
        table.insert(rules, self:parseRule(v))
      end
      return {name=name, rules=rules}
    end,
    
    parseRule = function(self, data, name)
      if type(data) == "string" then
        local rule = self.ruleset.rules[data]
        assert(rule, ([[named rule "%s" not found]]):format(data))
        return rule
      end
      assert_type(data, "table", "invalid rule data type: " .. type(data))
      assert_jsontype(data, "object", ("invalid rule data type: %s"):format(jsontype(data)))
      
      if ((data["if"] or data["if-any"] or data["if-all"]) or data["then"]) then
        assert(not data["always"], [["always" clause can't be present in if/then rule]])
        assert(not data["switch"], [["switch" clause can't be present in if/then rule]])
      end
      
      if data["then"] then
        if (data["if"] and (data["if-any"] or data["if-all"])) or (data["if-any"] and data["if-all"]) then
          return nil, "only one of \"if\", \"if-any\" or \"if-all\" allowed in if/then rule"
        end
        
        local condition
        if data["if"] then
          condition = self:parseCondition(data["if"])
        elseif data["if-any"] or data["if-all"] then
          local conditions = {}
          for _, v in ipairs(data["if-any"] or data["if-all"]) do
            condition = assert(self:parseCondition(v))
            table.insert(conditions, condition)
          end
          condition = {[(data["if-any"] and "any" or "all")]=conditions}
        end
        return {["if"]=condition, ["then"]=data["then"], name=data["name"] or name, key=data["key"]}
        
      elseif data["always"] then
        return {["if"]={always={}}, ["then"]=data["always"], name=data["name"] or name, key=data["key"]}
      else
        error("something went wrong parsing rule")
      end
    end,
    
    parseCondition = function(self, data)
      local condition
      if type(data) == "string" then
        condition = {[data]={}}
      elseif type(data) == "table" then
        assert_jsontype(data, "object", "condition cannot be an array, must be an object")
        assert_table_size(data, 1, "condition object must have exactly one attribute (the condition name)")
        condition = data
      else
        error(("wrong type (%s) for condition"):format(type(data)))
      end
      return Rule.condition.parse(condition, self)
    end,
    
    parseAction = function(self, data)
      local action
      if type(data) == "string" then
        action = {[data]={}}
      elseif jsontype(data) == "object" then
        assert(next(data, next(data)) == nil, "action object must have only 1 attribute -- the action name")
        action = data
      else
        error(("action must be string on 1-attribute object, but instead was a %s"):format(jsontype(data)))
      end
      return Rule.action.parse(action, self)
    end,
    
    parseActions = function(self, data)
      local actions = {}
      if jsontype(data) == "array" then
        for _, v in ipairs(data) do
          table.insert(actions, self:parseAction(v))
        end
        return actions
      else
        return { self:parseAction(data) }
      end
    end,
    
    parseLimiter = function(self, data, name)
      print("parseLimiter", data, name)
      return nil, "nonono"
    end
  }})
  
  return parser
end

local Parser = {new = newparser}

return Parser
