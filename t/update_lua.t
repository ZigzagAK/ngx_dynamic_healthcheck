use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__


=== TEST 1: healthcheck http update lua
--- http_config
    lua_load_resty_core off;
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
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck"
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            local cjson = require "cjson"
            ngx.log(ngx.INFO, cjson.encode(data))
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
            hc.update("u1", {
                fall=100,
                rise=200,
                timeout=4000,
                interval=1,
                command = {
                    uri="/ping",
                    method="POST",
                    headers={a=1,b=2},
                    body="ping",
                    expected = {
                        codes={400,500},
                        body="pong"
                    }
                },
                disabled=1,
                off=1
            })
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            local cjson = require "cjson"
            ngx.log(ngx.INFO, cjson.encode(data))
            for u, h in pairs(data)
            do
               ngx.say(string.format("%s %s %d %d %d %d %d %d", u,
                                     h.type, h.fall, h.rise, h.timeout, h.interval, h.disabled, h.off))
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
u1 http 100 200 4000 1 1 1
a=1
b=2
/ping POST ping pong
400
500


=== TEST 2: healthcheck stream update lua
--- http_config
    lua_load_resty_core off;
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
        check fall=2 rise=1 timeout=1500 interval=60;
        check_request_body hello;
        check_response_body .*;
    }
--- stream_server_config
    proxy_pass u1;
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck.stream"
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            local cjson = require "cjson"
            ngx.log(ngx.INFO, cjson.encode(data))
            for u, h in pairs(data)
            do
               ngx.say(string.format("%s %d %d %d %d", u,
                                     h.fall, h.rise, h.timeout, h.interval))
               ngx.say(string.format("%s %s", h.command.body, h.command.expected.body))
            end
            hc.update("u1", {
                fall=100,
                rise=200,
                timeout=4000,
                interval=1,
                command = {
                    body="ping",
                    expected = {
                        body="pong"
                    }
                },
                disabled=1,
                off=1
            })
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            local cjson = require "cjson"
            ngx.log(ngx.INFO, cjson.encode(data))
            for u, h in pairs(data)
            do
               ngx.say(string.format("%s %d %d %d %d %d %d", u,
                                     h.fall, h.rise, h.timeout, h.interval, h.disabled, h.off))
               ngx.say(string.format("%s %s", h.command.body, h.command.expected.body))
            end
        }
    }
--- request
    GET /test
--- response_body
u1 2 1 1500 60
hello .*
u1 100 200 4000 1 1 1
ping pong



