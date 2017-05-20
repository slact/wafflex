--lua environs initializer
return function(package_loader, initializers)
  package.path = ""
  package.cpath= ""
  setmetatable(package.preload, {__index = function(self, name)
    local pkg = package_loader(name)
    local loader
    if pkg then
      loader = function()
        return pkg
      end
      self[name]=loader
    end
    return loader
  end})
  
  local mm = require "mm"
  local Parser = require "parser"
  local Ruleset = require "ruleset"
  local p = Parser.new()

  --[[
  for k,v in pairs(initializers) do
    if k == "conditions" then
      for kk, vv in pairs(v) do
        
      end
    elseif k=="actions" then
      for kk, vv in pairs(v) do
        
      end
    else
      
    end
    
  end
  ]]
  
  
  
  local parsed = p:parseFile("/home/leop/sandbox/wafflex/src/lua/nginx/ruleset1.json")

  mm(parsed)

  local rs = Ruleset.new(parsed)

  mm(rs)
end
