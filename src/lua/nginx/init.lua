--lua environs initializer
return function(package_loader)
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
end
