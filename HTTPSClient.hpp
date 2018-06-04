#include "sdkconfig.h"

#ifndef HTTPSClient_HPP_
#define HTTPSClient_HPP_

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <utility>
#include <exception>

#include <functional>

#include <lwip/sockets.h>

#include <lwip/err.h>

#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>


#if defined(CONFIG_USE_ESP_TLS)
#include <esp_http_client.h>
#define _HTTPS_CLIENT_BUFFSIZE 1024
#else 
#include <mongoose.h>
#endif

class HTTPSClient {
  public:
    int get(const char* _url);

    HTTPSClient(const std::string &user_agent, const char* ca_pem, int timeout = 5000);
    virtual ~HTTPSClient();

    void close();

    bool is_open() const {
        return _connection_open;
    }

    void set_read_cb(std::function<void (const char*, int)>read_cb) {
        _read_cb = read_cb;
    }

    int read(char* buf, int len);
    int write(const char* buf, int len);
    std::function<void (const char*, int)> _read_cb;

    int exit_flag = 0;
    int status_code = 0;

  private:

    bool _connection_open;


#if defined(CONFIG_USE_ESP_TLS)
    esp_http_client_handle_t * _client;
    char text[_HTTPS_CLIENT_BUFFSIZE];
#else 
    struct mg_mgr * _client;
    mg_connection *_nc;
#endif

    std::string _user_agent;

    const char* _ca_pem;

    int _timeout;

};

#endif /* GUARD */
