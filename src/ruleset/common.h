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

int ruleset_common_update_item_name(lua_State *L, char **nameptr);

#define ruleset_common_shm_alloc_init_item(type, data_sz, L) \
  ruleset_common_shm_alloc_init_custom_name_item(type, data_sz, L, name)

#define ruleset_common_shm_alloc_init_custom_name_item(type, data_sz, L, name_key) \
  __ruleset_common_shm_alloc_init_item(L, sizeof(type), data_sz, #name_key, offsetof(type, name_key), offsetof(type, luaref))
  
#define ruleset_common_shm_alloc_init_noname_item(type, data_sz, L) \
  __ruleset_common_shm_alloc_init_item_noname(L, sizeof(type), data_sz, offsetof(type, luaref))
  
void * __ruleset_common_shm_alloc_init_item(lua_State *L, size_t item_sz, size_t data_sz, char *str_key, off_t str_offset, off_t luaref_offset);
void * __ruleset_common_shm_alloc_init_item_noname(lua_State *L, size_t item_sz, size_t data_sz, off_t luaref_offset);


#define ruleset_common_shm_free_item(L, what) \
  ruleset_common_shm_free_custom_name_item(L, what, name)
  
#define ruleset_common_shm_free_noname_item(L, what) \
  wfx_lua_unref(L, (what)->luaref); \
  (what)->luaref = LUA_NOREF; \
  __ruleset_common_shm_free_item(L, what, NULL)

#define ruleset_common_shm_free_custom_name_item(L, what, name_key) \
  wfx_lua_unref(L, (what)->luaref); \
  (what)->luaref = LUA_NOREF; \
  __ruleset_common_shm_free_item(L, what, (what)->name_key)

void __ruleset_common_shm_free_item(lua_State *L, void *ptr, char *name_str);

int ruleset_common_delay_update(lua_State *L, wfx_readwrite_t *rw, lua_CFunction update);

ngx_int_t wfx_ruleset_init_runtime(lua_State *L, int manager);
ngx_int_t wfx_readwrite_init_runtime(lua_State *L, int manager);

int ruleset_common_reserve_read(wfx_evaldata_t *ed, wfx_readwrite_t *rw);
int ruleset_common_release_read(wfx_readwrite_t *rw);

int ruleset_common_reserve_write(wfx_readwrite_t *rw);
int ruleset_common_release_write(wfx_readwrite_t *rw);

#endif //WFX_RULESET_TYPES_H
