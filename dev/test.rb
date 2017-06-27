#!/usr/bin/ruby
require 'rubygems'
require 'bundler/setup'

require 'minitest'
require 'minitest/reporters'

require "minitest/autorun"
Minitest::Reporters.use! [Minitest::Reporters::SpecReporter.new(:color => true)]
require 'securerandom'
require "redis"
require "pry"
require "json"

SERVER=ENV["NGINX_SERVER"] || "127.0.0.1"
PORT=ENV["NGINX_PORT"] || "8082"

REDIS="redis://localhost:8537/2"


class WafflexTest <  Minitest::Test
  def redis_script(name, *args)
    scripthash = @scripts[name.to_sym]
    raise "unknown redis script #{name}" if not scripthash
    @redis.evalsha scripthash, *args
  end
  def setup
    @redis = Redis.new(url: REDIS)
    the_scripts = @redis.hgetall("wafflex:scripts")
    @scripts = {}
    the_scripts.each do |k, v|
      @scripts[k.to_sym]=v
    end
  end

  def test_scripts
    json = IO.read("./testruleset.json")
    
    parsed_in = JSON.parse(json)
    
    #ok, err = redis_script :ruleset_write, [], ["", "create", "ruleset", "rlst", json]
    #assert_equal 1, ok, err
    
    out = redis_script :ruleset_read, [], ["", "rlst", "ruleset"]
    
    parsed = JSON.parse(out)
    
    #binding.pry
    
    newrule = '{
      "if": {"limit-break" : {
        "name": "rate-limit",
        "key": "hello $ISthis $it $request_real_ip banana ${1}"
      }},
      "then": [
        {"#tag": "slowdown"}
      ]
    }'
    ok, err = redis_script :ruleset_write, [], ["", "update", "rule", "rlst", "fwoop", newrule]
    assert_equal 1, ok, err
    
    
    
    
  end  
end


