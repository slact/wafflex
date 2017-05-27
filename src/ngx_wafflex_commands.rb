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
      [:loc_conf, :ruleset],
      args: 1..5,
      value: "[<ruleset_name>] </path/to/ruleset.json>",
      info: "Use this ruleset here."
  
end
