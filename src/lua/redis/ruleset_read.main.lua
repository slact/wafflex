--main script
--luacheck: globals redis cjson ARGV
local prefix =        ARGV[1]
local ruleset_name =  ARGV[2]
local item =          ARGV[3]
local item_name =     ARGV[4]

local get_ruleset_thing = require("ruleset_read")

return assert(get_ruleset_thing(prefix, ruleset_name, item, item_name, true))
