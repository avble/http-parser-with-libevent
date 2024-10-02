# http-parser-with-libevent
A mini C++ http server backed by I/O event libraries: [libevent](https://github.com/libevent/libevent) and [http-parser](https://github.com/nodejs/llhttp)

It is a fun project, creating a http server from scratch. 

# [http server](https://github.com/avble/http-parser-with-libevent/)

you can quick start creating a http server by the below source code.

``` cpp
#include "http.hpp"
using namespace std::placeholders;
using namespace http;
int main(int argc, char * args[])
{
    if (argc != 3)
    {
        std::cerr << "\nUsage: " << args[0] << " address port\n" << "Example: \n" << args[0] << " 0.0.0.0 12345" << std::endl;
        return -1;
    }

    std::string addr(args[1]);
    uint16_t port = static_cast<uint16_t>(std::atoi(args[2]));

    http::start_server(port, [](response & res) { res.body() = "hello world\n"; });
}
```

## compilation and run
The program has been tested under the `Linux 5.15.153.1-microsoft-standard-WSL2`
``` shell
$ mkdir build && cd build && cmake ..
$ make        # complation
$ ./http_srv 0.0.0.0 12345 # run server
```

# Performance
There are several criterias for measuring the performance of a http server.

The performance is measured on environment: `11th Gen Intel(R) Core(TM) i5-1135G7 @ 2.40GHz` and `microsoft-standard-WSL2`

The ab tool is used to measure the performance. It is also understood that the evaluation result may vary on the environment (OS and hardware). 

## Request per second
This criteria is to measure the responsive of a http server.
It is tested by 50 concurrency active connection and 100,000 request in total.
``` shell
$ ab -k -c 50 -n 100000 127.0.0.1:12345/route_01
```

| http server | Request per second | Remark |
| http-parser-with-libevent  |      ~150,000      |  using llhttp-parser (not internal http of libevent) |
| internal http libevent  |      ~95,000      |  [bench_http](https://github.com/libevent/libevent/blob/master/test/bench_http.c) of libevent (release-2.1.12-stable) |
| nodejs   |    12,000 rps  | v12.22.9 |
| asiohttp | 11,000 rps | 3.10.6 |
| flask   | 697 rps | 3.0.3 |

# Appendix
nodejs server code test
``` javascript
const { createServer } = require('http');

const hostname = '127.0.0.1';
const port = 12345;

const server = createServer((req, res) => {
    res.statusCode = 200;
    res.setHeader('Content-Type', 'text/plain');
    res.end('Hello World');
});


server.listen(port, hostname, () => {
    console.log(`Server running at http://${hostname}:${port}/`);
});
```

asionhttp server code test
``` python
from aiohttp import web

async def handle(request):
    name = request.match_info.get('name', "Anonymous")
    text = "Hello, " + name
    return web.Response(text=text)

app = web.Application()
app.add_routes([web.get('/', handle),
                web.get('/{name}', handle)])

if __name__ == '__main__':
    web.run_app(app, port=12345)
```

flask server code test
``` python
from flask import Flask
app = Flask(__name__)
@app.route("/")
def hello_world():
    return "Hello, World"

if __name__ == "__main__":
    app.run(port=12345)
```
