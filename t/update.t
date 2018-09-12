use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__


=== TEST 1: healthcheck http update
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
--- config
    location /get {
      healthcheck_get;
    }
    location /update {
      healthcheck_update;
    }
    location /test {
        content_by_lua_block {
            local resp = assert(ngx.location.capture("/get"))
            if resp.status ~= ngx.HTTP_OK then
              ngx.say(resp.status)
            end
            ngx.log(ngx.INFO, resp.body)
            local cjson = require "cjson"
            local data = cjson.decode(resp.body)
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
            assert(ngx.location.capture("/update?upstream=u1&fall=100&rise=200&timeout=4000&interval=1&request_uri=/ping&request_method=POST&request_headers=a:1|b:2&request_body=ping&response_codes=400|500&response_body=pong"))
            local resp = assert(ngx.location.capture("/get"))
            if resp.status ~= ngx.HTTP_OK then
              ngx.say(resp.status)
            end
            ngx.log(ngx.INFO, resp.body)
            local cjson = require "cjson"
            local data = cjson.decode(resp.body)
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
u1 http 100 200 4000 1
a=1
b=2
/ping POST ping pong
400
500


=== TEST 2: healthcheck stream update
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
    location /get {
      healthcheck_get;
    }
    location /update {
      healthcheck_update;
    }
    location /test {
        content_by_lua_block {
            local resp = assert(ngx.location.capture("/get?stream="))
            if resp.status ~= ngx.HTTP_OK then
              ngx.say(resp.status)
            end
            ngx.log(ngx.INFO, resp.body)
            local cjson = require "cjson"
            local data = cjson.decode(resp.body)
            for u, h in pairs(data)
            do
               ngx.say(string.format("%s %d %d %d %d", u,
                                     h.fall, h.rise, h.timeout, h.interval))
               ngx.say(string.format("%s %s", h.command.body, h.command.expected.body))
            end
            assert(ngx.location.capture("/update?stream=&upstream=u1&fall=100&rise=200&timeout=4000&interval=1&request_body=ping&response_body=pong"))
            local resp = assert(ngx.location.capture("/get?stream="))
            if resp.status ~= ngx.HTTP_OK then
              ngx.say(resp.status)
            end
            ngx.log(ngx.INFO, resp.body)
            local cjson = require "cjson"
            local data = cjson.decode(resp.body)
            for u, h in pairs(data)
            do
               ngx.say(string.format("%s %d %d %d %d", u,
                                     h.fall, h.rise, h.timeout, h.interval))
               ngx.say(string.format("%s %s", h.command.body, h.command.expected.body))
            end
        }
    }
--- request
    GET /test
--- response_body
u1 2 1 1500 60
hello .*
u1 100 200 4000 1
ping pong


