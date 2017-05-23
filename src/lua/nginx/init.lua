--lua environs initializer
return function(package_loader, init_bind_cfunc)
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
  
  _G.Parser = require "parser"
  _G.Ruleset = require "ruleset"
  _G.Binding = require "binding"
  Binding.require_create_userdata = true
  Binding.require_binding = true
  
  _G.mm = require "mm"
  
  if init_bind_cfunc then
    init_bind_cfunc(Binding.set)
  end
  
  local p = Parser.new()
  
  local parsed = p:parseFile("/home/leop/sandbox/wafflex/src/lua/nginx/ruleset1.json")

  --mm(parsed)

  local rs = Ruleset.new(parsed)

  mm(rs)
end
