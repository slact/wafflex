// don't edit this please, it was auto-generated by hsss
// https://github.com/slact/hsss

typedef struct {
  char *name;
  char *hash;
  char *script;
} wfx_redis_lua_script_t;

typedef struct {
  wfx_redis_lua_script_t _parser_main;

  wfx_redis_lua_script_t init;

  //autogenerated script, do not edit
  wfx_redis_lua_script_t parser;

} wfx_redis_lua_scripts_t;
wfx_redis_lua_scripts_t wfx_redis_lua_scripts;
const int wfx_redis_lua_scripts_count;
#define WFX_REDIS_LUA_SCRIPTS_EACH(script) \
for((script)=(wfx_redis_lua_script_t *)&wfx_redis_lua_scripts; (script) < (wfx_redis_lua_script_t *)(&wfx_redis_lua_scripts + 1); (script)++) 
