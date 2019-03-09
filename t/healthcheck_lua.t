use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: healthcheck http lua
--- http_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
        check type=http fall=2 rise=1 timeout=1500 interval=60;
        check_request_uri GET /heartbeat;
        check_request_headers aaa=333 bbb=444;
        check_request_body hello;
        check_response_codes 200 201;
        check_response_body .*;
    }
    upstream u2 {
        zone shm-u2 128k;
        server 127.0.0.1:6001;
        check type=http fall=4 rise=2 timeout=15000 interval=10;
        check_request_uri POST /health;
        check_request_headers aaa=555 bbb=666;
        check_request_body hello;
        check_response_codes 200 201 204;
        check_response_body .*;
    }
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck"
            local data, err = hc.get()
            if not data then
              ngx.say(err)
            end
            for u, h in pairs(data)
            do
               ngx.say(string.format("%s %s %d %d %d %d", u,
                                     h.type, h.fall, h.rise, h.timeout, h.interval))
               for k,v in pairs(h.command.headers)
               do
                 ngx.say(k,"=",v)
               end
               ngx.say(string.format("%s %s %s %s", h.command.uri, h.command.method,
                                     h.command.body, h.command.expected.body))
               for _,c in ipairs(h.command.expected.codes)
               do
                 ngx.say(c)
               end
            end
        }
    }
--- request
    GET /test
--- response_body
u1 http 2 1 1500 60
aaa=333
bbb=444
/heartbeat GET hello .*
200
201
u2 http 4 2 15000 10
aaa=555
bbb=666
/health POST hello .*
200
201
204


=== TEST 2: healthcheck stream lua
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
        check fall=2 rise=1 timeout=1500 interval=60;
        check_request_body ping;
        check_response_body pong;
    }
    upstream u2 {
        zone shm-u2 128k;
        server 127.0.0.1:6001;
        check fall=4 rise=2 timeout=15000 interval=10;
        check_request_body ping2;
        check_response_body pong2;
    }
--- stream_server_config
    proxy_pass u1;
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck.stream"
            local data, err = hc.get()
            if not data then
              ngx.say(err)
            end
            for u, h in pairs(data)
            do
               ngx.say(string.format("%s %d %d %d %d", u,
                                     h.fall, h.rise, h.timeout, h.interval))
               ngx.say(string.format("%s %s",
                                     h.command.body, h.command.expected.body))
            end
        }
    }
--- request
    GET /test
--- response_body_like
u1 2 1 1500 60
ping pong
u2 4 2 15000 10
ping2 pong2


=== TEST 3: healthcheck http tcp lua
--- http_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
        check type=tcp fall=2 rise=1 timeout=1500 interval=60;
    }
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck"
            local data, err = hc.get()
            if not data then
              ngx.say(err)
            end
            for u, h in pairs(data)
            do
               ngx.say(string.format("%s %s %d %d %d %d", u,
                                     h.type, h.fall, h.rise, h.timeout, h.interval))
            end
        }
    }
--- request
    GET /test
--- response_body_like
u1 tcp 2 1 1500 60


=== TEST 4: healthcheck http ssl lua
--- http_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
        check type=ssl fall=2 rise=1 timeout=1500 interval=60;
    }
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck"
            local data, err = hc.get()
            if not data then
              ngx.say(err)
            end
            for u, h in pairs(data)
            do
               ngx.say(string.format("%s %s %d %d %d %d", u,
                                     h.type, h.fall, h.rise, h.timeout, h.interval))
            end
        }
    }
--- request
    GET /test
--- response_body_like
u1 ssl 2 1 1500 60


=== TEST 5: healthcheck stream tcp lua
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
        check fall=2 rise=1 timeout=1500 interval=60;
    }
--- stream_server_config
    proxy_pass u1;
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck.stream"
            local data, err = hc.get()
            if not data then
              ngx.say(err)
            end
            for u, h in pairs(data)
            do
               ngx.say(string.format("%s %s %d %d %d %d", u,
                                     h.type, h.fall, h.rise, h.timeout, h.interval))
            end
        }
    }
--- request
    GET /test
--- response_body_like
u1 tcp 2 1 1500 60


=== TEST 6: healthcheck stream ssl lua
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
        check type=ssl fall=2 rise=1 timeout=1500 interval=60;
    }
--- stream_server_config
    proxy_pass u1;
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck.stream"
            local data, err = hc.get()
            if not data then
              ngx.say(err)
            end
            for u, h in pairs(data)
            do
               ngx.say(string.format("%s %s %d %d %d %d", u,
                                     h.type, h.fall, h.rise, h.timeout, h.interval))
            end
        }
    }
--- request
    GET /test
--- response_body_like
u1 ssl 2 1 1500 60


=== TEST 7: no healthcheck
--- http_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
    }
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck"
            local data, err = hc.get("u1")
            ngx.say(err)
        }
    }
--- request
    GET /test
--- response_body
no healthcheck


=== TEST 8: stream no healthcheck
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
    }
--- stream_server_config
    proxy_pass u1;
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck.stream"
            local data, err = hc.get("u1")
            ngx.say(err)
        }
    }
--- request
    GET /test
--- response_body
no healthcheck


=== TEST 9: healthcheck + no healthcheck
--- http_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
    }
    upstream u2 {
        zone shm-u2 128k;
        server 127.0.0.1:6001;
        check type=tcp fall=2 rise=1 timeout=1500 interval=60;
    }
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck"
            local data, err = hc.get()
            if not data then
              return ngx.say(err)
            end
            if not data["u2"] then
              return ngx.say("u2 not found")
            end
            if data["u1"] then
              return ngx.say("u1 present")
            end
            ngx.say(data["u2"].type)
        }
    }
--- request
    GET /test
--- response_body
tcp


=== TEST 10: stream healthcheck + no healthcheck
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
    }
    upstream u2 {
        zone shm-u2 128k;
        server 127.0.0.1:6001;
        check type=tcp fall=2 rise=1 timeout=1500 interval=60;
    }
--- stream_server_config
    proxy_pass u1;
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck.stream"
            local data, err = hc.get()
            if not data then
              return ngx.say(err)
            end
            if not data["u2"] then
              return ngx.say("u2 not found")
            end
            if data["u1"] then
              return ngx.say("u1 present")
            end
            ngx.say(data["u2"].type)
        }
    }
--- request
    GET /test
--- response_body
tcp


=== TEST 11: upstream not found
--- http_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
    }
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck"
            local data, err = hc.get("notfound")
            ngx.say(err)
        }
    }
--- request
    GET /test
--- response_body
upstream not found


=== TEST 12: stream upstream not found
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
    }
--- stream_server_config
    proxy_pass u1;
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck.stream"
            local data, err = hc.get("notfound")
            ngx.say(err)
        }
    }
--- request
    GET /test
--- response_body
upstream not found
