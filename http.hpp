#include "llhttp.h"

#include "event2/buffer.h"
#include "event2/bufferevent.h"
#include "event2/event.h"
#include "event2/listener.h"

#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sys/queue.h>

class Event
{
public:
    ///////////////////////////////////////////////////////////////////////
    // Schedule a callback
    static int run_forever() { return event_base_dispatch(Event::event_base_global()); }

    ///////////////////////////////////////////////////////////////////////
    // Schedule a callback
    template <class F, class... Args>
    static void call_soon(F f, Args... args)
    {
        struct wrapper
        {
            wrapper(F func_, Args... args) : func(std::bind(func_, args...)) {}

            void operator()() { func(); }

            std::function<void()> func;
        };

        static event_callback_fn event_base_once_cb = [](evutil_socket_t fd, short what, void * ptr) {
            wrapper * p = (wrapper *) ptr;
            (*p)();
            delete p;
        };

        event_base_once(Event::event_base_global(), -1, EV_TIMEOUT, event_base_once_cb,
                        new wrapper(std::move(f), std::forward<Args>(args)...), NULL);
    }

    static event_base * event_base_global()
    {
        if (event_base_ == NULL)
            event_base_ = event_base_new();

        return event_base_;
    }

private:
    static event_base * event_base_;
    static std::mutex event_mutex_;
};

event_base * Event::event_base_ = NULL;
std::mutex Event::event_mutex_;

namespace http {

class response
{
public:
    response() {}
    response(const response & other) { body_ = other.body_; }
    response(const response && other) { body_ = other.body_; }
    std::string & body() { return body_; }

private:
    std::string body_;
};

void start_server(unsigned short port, std::function<void(response &)> _handler)
{

    class http_session : public std::enable_shared_from_this<http_session>
    {
        struct internal_wrapper
        {
            internal_wrapper(std::shared_ptr<http_session> _p) { p_session = _p; }

            internal_wrapper(const internal_wrapper & other) { p_session = other.p_session; }

            std::shared_ptr<http_session> p_session;
        };

    public:
        http_session(bufferevent * _bev, std::function<void(response &)> _handler)
        {
            bev     = _bev;
            handler = _handler;
        }

        void start() { do_read_request(); }

    private:
        http_session()                                  = delete;
        http_session(const http_session &)              = delete;
        http_session(const http_session &&)             = delete;
        http_session & operator=(const http_session &)  = delete;
        http_session & operator=(const http_session &&) = delete;

    public:
        void do_read_request()
        {
            static llhttp_cb llhttp_on_message = [](llhttp_t * req) -> int {
                internal_wrapper * wrapper_ = (internal_wrapper *) req->data;
                auto self                   = wrapper_->p_session;

                response res;
                self->handler(res);

                auto do_write_ = [func_ = std::bind(&http_session::do_write_response, self, res)]() { func_(); };
                Event::call_soon(do_write_);

                evbuffer * input = bufferevent_get_input(self->bev);
                evbuffer_drain(input, evbuffer_get_length(input));

                return 0;
            };

            static bufferevent_data_cb bev_read_cb = [](bufferevent * bev, void * arg) {
                evbuffer * input = bufferevent_get_input(bev);
                int buff_len     = evbuffer_get_length(input);
                char * buff      = (char *) malloc(buff_len);

                llhttp_t parser;
                llhttp_settings_t settings;

                /*Initialize user callbacks and settings */
                llhttp_settings_init(&settings);

                /*Set user callback */
                settings.on_message_complete = llhttp_on_message;

                llhttp_init(&parser, HTTP_BOTH, &settings);
                parser.data = arg;

                /*Parse request! */
                evbuffer_copyout(input, buff, buff_len);

                enum llhttp_errno err = llhttp_execute(&parser, buff, buff_len);
                if (err == HPE_OK)
                {
                }
                free(buff);
            };

            static bufferevent_event_cb bev_err_cb = [](struct bufferevent * bev, short what, void * ctx) {
                void * arg = NULL;
                bufferevent_getcb(bev, NULL, NULL, NULL, &arg);
                internal_wrapper * wrapper_ = (internal_wrapper *) arg;
                delete wrapper_;
            };

            void * arg = NULL;
            bufferevent_getcb(bev, NULL, NULL, NULL, &arg);
            arg = arg != NULL ? arg : new internal_wrapper(shared_from_this());

            bufferevent_setcb(bev, bev_read_cb, NULL, bev_err_cb, arg);

            bufferevent_enable(bev, EV_READ | EV_WRITE);
        }

        void do_write_response(response res)
        {

            static bufferevent_data_cb continue_read_ = [](bufferevent * bev, void * arg) {
                internal_wrapper * wrapper_ = (internal_wrapper *) arg;
                auto self                   = wrapper_->p_session;
                auto do_read                = [func_ = std::bind(&http_session::do_read_request, self)]() { func_(); };

                Event::call_soon(do_read);
            };

            // std::string body   = "hello world";
            std::string header = "HTTP/1.0 200 OK";
            header += "\n";
            header += "Connection: keep-alive";
            header += "\n";
            header += "Content-Length: ";
            header += std::to_string(res.body().size());
            header += "\r\n\r\n";

            evbuffer * output = bufferevent_get_output(bev);
            evbuffer_add(output, header.data(), header.size());
            evbuffer_add(output, res.body().c_str(), res.body().size());

            bufferevent_data_cb readcb_ptr   = NULL;
            bufferevent_data_cb writecb_ptr  = NULL;
            bufferevent_event_cb eventcb_ptr = NULL;
            void * arg;
            bufferevent_getcb(bev, &readcb_ptr, &writecb_ptr, &eventcb_ptr, &arg);
            bufferevent_setcb(bev, readcb_ptr, continue_read_, eventcb_ptr, arg);

            bufferevent_disable(bev, EV_READ);
            bufferevent_enable(bev, EV_WRITE);
        }

    public:
        bufferevent * bev;
        std::function<void(response &)> handler;
    };

    static evconnlistener_cb on_accept = [](struct evconnlistener * listener, evutil_socket_t fd, struct sockaddr * sa, int socklen,
                                            void * arg) {
        bufferevent * bev = bufferevent_socket_new(Event::event_base_global(), fd, BEV_OPT_CLOSE_ON_FREE);

        std::tuple<std::function<void(response &)>> * p = (std::tuple<std::function<void(response &)>> *) arg;
        std::shared_ptr<http_session>
        {
            new http_session(bev, std::get<0>(*p)), [](http_session * p) {
                // std::cout << "[DEBUG] http_session is deleted.\n";
                delete p;
            }
        } -> start();

        // delete p;
    };

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    int flags            = LEV_OPT_CLOSE_ON_FREE | LEV_OPT_CLOSE_ON_EXEC | LEV_OPT_REUSEABLE;
    struct sockaddr * sa = (struct sockaddr *) &addr;

    evconnlistener * listener = evconnlistener_new_bind(
        Event::event_base_global(), on_accept, new std::tuple<decltype(_handler)>(_handler), flags, -1, sa, sizeof(sockaddr_in));

    if (listener != NULL)
    {
        std::cout << "server has started on port: " << port << " using " << event_base_get_method(Event::event_base_global())
                  << std::endl;
        Event::run_forever();
    }
    else
        std::cout << "server failed at starting on port: " << port << std::endl;
}
} // namespace http
