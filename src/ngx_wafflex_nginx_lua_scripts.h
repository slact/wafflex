// don't edit this please, it was auto-generated by hsss
// https://github.com/slact/hsss

typedef struct {
  char *name;
  char *script;
} wfx_lua_script_t;

typedef struct {
  //lua environs initializer
  wfx_lua_script_t init;

  wfx_lua_script_t ipc;

  wfx_lua_script_t limiter;

  wfx_lua_script_t redis;

  wfx_lua_script_t tag;

  wfx_lua_script_t tracer;

  wfx_lua_script_t util;

} wfx_lua_scripts_t;
wfx_lua_scripts_t wfx_lua_scripts;
#define WFX_LUA_SCRIPTS_EACH(script) \
for((script)=(wfx_lua_script_t *)&wfx_lua_scripts; (script) < (wfx_lua_script_t *)(&wfx_lua_scripts + 1); (script)++) 
// don't edit this please, it was auto-generated by hsss
// https://github.com/slact/hsss

typedef struct {
  char *name;
  char *script;
} wfx_module_lua_script_t;

typedef struct {
  wfx_module_lua_script_t binding;

  //David Kolf's JSON module for Lua 5.1/5.2
  // small hack to generate object and array metatables, 
  // and key ordering callbacks by slact
  wfx_module_lua_script_t dkjson;

  wfx_module_lua_script_t mm;

  wfx_module_lua_script_t parser;

  wfx_module_lua_script_t redis;

  wfx_module_lua_script_t rule;

  wfx_module_lua_script_t ruleset;

} wfx_module_lua_scripts_t;
wfx_module_lua_scripts_t wfx_module_lua_scripts;
#define WFX_MODULE_LUA_SCRIPTS_EACH(script) \
for((script)=(wfx_module_lua_script_t *)&wfx_module_lua_scripts; (script) < (wfx_module_lua_script_t *)(&wfx_module_lua_scripts + 1); (script)++) 
