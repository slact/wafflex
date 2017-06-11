local json = require "dkjson"
local Parser = require "parser"

--luacheck: globals redis


redis.command("echo", tostring(Parser) .. tostring(json))
return 27
