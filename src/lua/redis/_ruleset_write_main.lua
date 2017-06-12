local Parser = require "parser"
local inspect = require "inspect"
--luacheck: globals redis cjson ARGV
local hmm = function(thing)
  local out = inspect(thing)
  for line in out:gmatch('[^\r\n]+') do
    redis.call("ECHO", line)
  end
end

local data = [[
{
  "name":"banana"
  "phases": {}
}
]]



local p = Parser:new()

local res, err = p:parseJSON(data, "testinput")
if not res then error(err) end
hmm(res)
return 27
