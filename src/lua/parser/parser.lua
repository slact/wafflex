local function parseRulesetThing(data_in, ruleset, opt)
  local data = data_in[opt.key]
  if data then
    if type(data) ~= opt.type then
      return nil, "wrong type for ruleset " .. opt.key
    end
    local ret, err
    for k,v in pairs(data) do
      ret, err = opt.parser_method(opt.parser, v, k)
      if not ret then
        return nil, err
      elseif ruleset[opt.key][ret.name] then
        return nil, ("%s %s already exists"):format(opt.thing, ret.name)
      else
        ruleset[opt.key][ret.name]=ret
      end
    end
  end
  return true
end

local function newparser()
  local parser = {}
  setmetatable(parser, {__index = {
    error = function(self, err)
      print(err)
      return nil, err
    end,
  
    parseRuleSet = function(self, data, name)
      
      self.ruleset = {
        limiters= {},
        rules= {},
        lists= {},
        table= {}
      }
      
      if type(data) ~= "table" then
        return self:error("wrong type for ruleset")
      end
      
      local ret, err
      
      ret, err = parseRulesetThing(data, self.ruleset, {
        thing="limiter", key="limiters", type="table",
        parser= self, parser_method= self.parseLimiter
      })
      if not ret then return nil, err end
      
      ret, err = parseRulesetThing(data, self.ruleset, {
        thing="rule", key="rules",  type="table",
        parser= self, parser_method= self.parseRule
      })
      if not ret then return nil, err end
      
      ret, err = parseRulesetThing(data, self.ruleset, {
        thing="list", key="lists",  type="table",
        parser= self, parser_method= self.parseRuleList
      })
      if not ret then return nil, err end
      
      ret, err = parseRulesetThing(data, self.ruleset, {
        thing="table", key="table",  type="table",
        parser= self, parser_method= self.parseRuleTable
      })
      if not ret then return nil, err end
      
    end,
    parseRuleTable = function(self, data)
      print("parsetable", data)
    end,
    parseRuleList = function(self, data, name)
      print("parseRuleList", data, name)
    end,
    parseRule = function(self, data, name)
      print("parseRule", data, name)
    end,
    parseLimiter = function(self, data, name)
      print("parseLimiter", data, name)
    end
  }})
  
  return parser
end


_G['parser']={new = newparser}
