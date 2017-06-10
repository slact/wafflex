#include <ngx_wafflex_redis_lua_scripts.h>

wfx_redis_lua_scripts_t wfx_redis_lua_scripts = {
  {"init", "4f7d624f703483b37d89af715d7230c2ea24a95c",
   "\n"
   "--luacheck: globals redis\n"
   "\n"
   "redis.echo(\"hi there\")\n"
   "return \"hello\"\n"}
};
const int wfx_redis_lua_scripts_count=1;
