#!/usr/bin/ruby
require 'minitest'
require 'minitest/reporters'
require "minitest/autorun"
Minitest::Reporters.use! [Minitest::Reporters::SpecReporter.new(:color => true)]
require 'securerandom'
require "redis"
require "pry"

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
    
    ok, err = redis_script :ruleset_write, [], ["", "create", "ruleset", "", json]
    assert_equal ok, 1, err
    
    
  end  
end


