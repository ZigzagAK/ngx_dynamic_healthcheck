use Test::Nginx::Socket 'no_plan';

no_shuffle();
run_tests();

__DATA__


=== STEP 1: init
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
        }
    }
--- request
    GET /test
--- response_body_like chomp
u1 http 2 1 1500 60
aaa=333
bbb=444
/heartbeat GET hello \.\*
200
201$


=== STEP 2: update
--- request
    GET /update?upstream=u1&fall=100&timeout=4000&interval=1&request_uri=/ping&request_method=POST&request_headers=a:1|b:2&request_body=ping&response_codes=400|500
--- response_body chomp
updated


=== STEP 3: check
--- request
    GET /test
--- response_body_like chomp
u1 http 100 1 4000 1
a=1
b=2
/ping POST ping \.\*
400
500$


=== STEP 4: after hup
--- http_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001;
        check type=http fall=2 rise=999 timeout=1500 interval=60;
        check_request_uri GET /heartbeat;
        check_request_headers aaa=333 bbb=444;
        check_request_body hello;
        check_response_codes 200 201;
        check_response_body pong;
    }
--- config
    location /get {
      healthcheck_get;
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
        }
    }
--- request
    GET /test
--- response_body_like chomp
u1 http 100 999 4000 1
a=1
b=2
/ping POST ping pong
400
500$

