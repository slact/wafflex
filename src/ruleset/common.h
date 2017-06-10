#ifndef WFX_RULESET_TYPES_H
#define WFX_RULESET_TYPES_H

#include <nginx.h>
#include <ngx_http.h>

/*
typedef struct wfx_binding_chain_s wfx_binding_chain_t;
struct wfx_binding_chain_s {
  wfx_binding_t        binding;
  wfx_binding_chain_t *next;
}; //wfx_binding_chain_t

void ruleset_subbinding_push(wfx_binding_chain_t **headptr, wfx_binding_t *binding);
void ruleset_subbinding_bind(lua_State *L, const char *name, wfx_binding_chain_t **headptr);
int ruleset_subbinding_call(lua_State *L,const char *binding_name, const char *subbinding_name, const char *call_name, void *ptr, int nargs);
int ruleset_subbinding_getname_call(lua_State *L, const char *binding_name, const char *call_name, int nargs);
*/

#define ruleset_common_shm_alloc_init_item(type, data_sz, L, name_key) \
  __ruleset_common_shm_alloc_init_item(L, sizeof(type), data_sz, #name_key, offsetof(type, name_key), offsetof(type, luaref))
  
#define ruleset_common_shm_alloc_init_item_noname(type, data_sz, L) \
  __ruleset_common_shm_alloc_init_item_noname(L, sizeof(type), data_sz, offsetof(type, luaref))
  
void * __ruleset_common_shm_alloc_init_item(lua_State *L, size_t item_sz, size_t data_sz, char *str_key, off_t str_offset, off_t luaref_offset);
void * __ruleset_common_shm_alloc_init_item_noname(lua_State *L, size_t item_sz, size_t data_sz, off_t luaref_offset);

#define ruleset_common_shm_free(L, what) \
  wfx_lua_unref(L, (what)->luaref); \
  (what)->luaref = LUA_NOREF; \
  __ruleset_common_shm_free(what)

void __ruleset_common_shm_free(void *);

#endif //WFX_RULESET_TYPES_H
