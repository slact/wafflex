local json = require "dkjson"
local Parser = require "parser"
local inspect = require "inspect"
--luacheck: globals redis cjson ARGV
local hmm = function(...)
  local out = inspect(...)
  for line in out:gmatch('[^\r\n]+') do
    redis.call("ECHO", line)
  end
end



local p = Parser:new()

hmm(p)

local ok, res = pcall(p.parseJSON, p, json, "testinput")
if not ok then
  return nil, (res:match("[^:]*:%d+: (.*)") or res)
end


redis.call("echo", tostring(Parser) .. tostring(json))
return 27
