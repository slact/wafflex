#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "ruleset.h"
#include "list.h"
#include "rule.h"
#include "condition.h"
#include "action.h"
#include "limiter.h"
#include "phase.h"

static wfx_binding_t wfx_ruleset_binding;

static int ruleset_create(lua_State *L) {
  const char        *str;
  
  wfx_ruleset_t     *ruleset;
  wfx_phase_t       *phase = NULL;
  
  ruleset = ruleset_common_shm_alloc_init_item(wfx_ruleset_t, 0, L, name);
  if(ruleset == NULL) {
    ERR("failed to initialize ruleset: out of memory");
  }
  
  lua_getfield(L, -1, "phases");
  lua_pushnil(L);  // first key 
  while (lua_next(L, -2) != 0) {
    lua_getfield(L, -1, "__binding");
    phase = lua_touserdata(L, -1);
    lua_pop(L,1);
    
    str = lua_tostring(L, -2);
    
    if(strcmp(str, "request") == 0) {
      ruleset->phase.request = phase;
    }
    else {
      ERR("unknown phase %s", str);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  
  lua_pushlightuserdata (L, ruleset);
  return 1;
}

int wfx_ruleset_bindings_set(lua_State *L) {
  //rule bindings
  wfx_rule_bindings_set(L);
  
  //rule condition bindings
  wfx_condition_bindings_set(L);
  
  //rule action bindings
  wfx_action_bindings_set(L);
  
  //list bindings
  wfx_list_bindings_set(L);
  
  //limiter bindings
  wfx_limiter_bindings_set(L);
  
  //phase bindings
  wfx_phase_bindings_set(L);
  
  //and now, at last, ruleset bindings
  wfx_lua_binding_set(L, &wfx_ruleset_binding);
  
  return 0;
}

void * __ruleset_common_shm_alloc_init_item(lua_State *L, size_t item_sz, char *str_key, off_t str_offset, off_t luaref_offset) {
  const char *tmpstr;
  size_t      tmpstrlen;
  int         tmpref;
  void       *ptr;
  char       *str;
  char      **strptr;
  int        *refptr;
  
  lua_pushvalue(L, 1); //lua-self
  tmpref = luaL_ref(L, LUA_REGISTRYINDEX);
  
  lua_pushvalue(L, 1); //lua-self
  lua_getfield(L, -1, str_key);
  tmpstr = lua_tolstring(L, -1, &tmpstrlen);
  
  if((ptr = wfx_shm_calloc(item_sz + tmpstrlen+1)) != NULL) {
    str = (char *)ptr + item_sz;
    strptr = (char **)((char *)ptr + str_offset);
    strcpy((char *)ptr + item_sz, tmpstr);
    *strptr = str;
    refptr = (int *)((char *)ptr + luaref_offset);
    *refptr = tmpref;
  }
  
  lua_pop(L, 2);
  return ptr;
}

static wfx_binding_t wfx_ruleset_binding = {
  "ruleset",
  ruleset_create,
  NULL,
  NULL,
  NULL
};

/*
void ruleset_subbinding_push(wfx_binding_chain_t **headptr, wfx_binding_t *binding) {
  ERR("ruleset_subbinding_push %s", binding->name);
  wfx_binding_chain_t *link = ngx_alloc(sizeof(*link), ngx_cycle->log);
  if(!link) {
    return;
  }
  link->binding = *binding;
  link->next = *headptr;
  *headptr = link;
}
static wfx_binding_t *ruleset_subbinding_pop(wfx_binding_chain_t **headptr, wfx_binding_t *curdst) {
  wfx_binding_chain_t *cur = *headptr;
  if(cur) {
    *curdst = cur->binding;
    *headptr = cur->next;
    ngx_free(cur);
    return curdst;
  }
  else {
    return NULL;
  }
}

void ruleset_subbinding_bind(lua_State *L, const char *name, wfx_binding_chain_t **headptr) {
  wfx_binding_t     binding;
  char              buf[255];
  
  snprintf(buf, 255, "binding:%s", name);
  lua_getglobal(L, buf);
  
  while(ruleset_subbinding_pop(headptr, &binding)) {
    lua_newtable(L);
    if(binding.create) {
      snprintf(buf, 255, "%s_%s_%s", name, binding.name, "create");
      wfx_lua_register_named(L, binding.create, buf);
      lua_setfield(L, -2, "create");
    }
    if(binding.replace) {
      snprintf(buf, 255, "%s_%s_%s", name, binding.name, "replace");
      wfx_lua_register_named(L, binding.replace, buf);
      lua_setfield(L, -2, "replace");
    }
    if(binding.update) {
      snprintf(buf, 255, "%s_%s_%s", name, binding.name, "update");
      wfx_lua_register_named(L, binding.update, buf);
      lua_setfield(L, -2, "update");
    }
    if(binding.delete) {
      snprintf(buf, 255, "%s_%s_%s", name, binding.name, "delete");
      wfx_lua_register_named(L, binding.delete, buf);
      lua_setfield(L, -2, "delete");
    }
    
    lua_setfield(L, -2, binding.name);
  }
  lua_pop(L, 1);
}
int ruleset_subbinding_call(lua_State *L, const char *binding_name, const char *subbinding_name, const char *call_name, void *ptr, int nargs) {
  char        buf[255];
  snprintf(buf, 255, "binding:%s", binding_name);
  ERR("nargs: %i", nargs);
  lua_printstack(L);
  lua_getglobal(L, buf);
  if(lua_isnil(L, -1)) {
    luaL_error(L, "subbinding %s not found", binding_name);
    return 0;
  }
  
  lua_getfield(L, -1, subbinding_name);
  if(lua_isnil(L, -1)) {
    luaL_error(L, "subbinding %s.%s not found", binding_name, subbinding_name);
    return 0;
  }
  lua_printstack(L);
  
  lua_getfield(L, -1, call_name);
  if(lua_isnil(L, -1)) {
    luaL_error(L, "subbinding %s.%s call \"%s\" not found", binding_name, subbinding_name, call_name);
    return 0;
  }
  
  lua_printstack(L);
  
  lua_remove(L, -3);
  lua_pushlightuserdata(L, ptr);
  lua_insert(L, -(nargs+1));
  lua_printstack(L);
  lua_call(L, nargs+1, 0);
  return 1;
}

int ruleset_subbinding_getname_call(lua_State *L, const char *binding_name, const char *call_name, int nargs) {
  const char *subbinding_name;
  int         rc;
  void       *ptr;
  lua_printstack(L);
  lua_getfield(L, 1, binding_name);
  lua_insert(L, 1);
  subbinding_name = lua_tostring(L, 1);
  lua_getfield(L, 2, "__binding");
  ptr = lua_touserdata(L, -1);
  lua_pop(L, 1);
  rc = ruleset_subbinding_call(L, binding_name, subbinding_name, call_name, ptr, nargs);
  lua_remove(L, 1);
  return rc;
}
*/
