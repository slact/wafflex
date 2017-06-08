CfCmd.new do
  wafflex_shared_memory_size [:main],
      :wfx_conf_set_size_slot,
      [:main_conf, :shm_size],
      
      group: "storage",
      value: "<size>",
      default: "128M",
      info: "Shared memory slab pre-allocated for Wafflex. Used to store rule evaluation data."
  
  wafflex_load_ruleset [:main, :srv],
      :wfx_conf_load_ruleset,
      :main_conf,
      args: 1..2,
      value: "[<ruleset_name>] </path/to/ruleset.json>",
      info: "Use this ruleset here."
  
  wafflex_ruleset [:main, :srv, :loc],
      :wfx_conf_ruleset,
      :loc_conf,
      args: 1..5,
      value: "ruleset_name ruleset_name2 ruleset_name3",
      info: "Use these rulesets here."
  
  wafflex_redis_url [:main, :srv, :loc],
      :ngx_conf_set_redis_url,
      [:loc_conf, :"redis.url"],
      
      group: "storage",
      tags: ['redis'],
      default: "127.0.0.1:6379",
      info: "The path to a redis server, of the form 'redis://:password@hostname:6379/0'. Shorthand of the form 'host:port' or just 'host' is also accepted."
end
