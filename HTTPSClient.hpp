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

#if defined(CONFIG_USE_ESP_TLS)
#include <esp_http_client.h>
#define _HTTPS_CLIENT_BUFFSIZE 1024
#else 
#include <mongoose.h>
#endif

class HTTPSClient {
  public:
    int get(const char* _url);
    int post(const char* _url, const char* _body);

    HTTPSClient(const std::string &user_agent, const char* ca_pem, int timeout = 5000);
    virtual ~HTTPSClient();

    void set_read_cb(std::function<void (const char*, int)>read_cb) {
        _read_cb = read_cb;
    }

    std::function<void (const char*, int)> _read_cb;

    int exit_flag = 0;
    int status_code = 0;

  private:
    std::string _user_agent;

    const char* _ca_pem;

    int _timeout;

};

#endif /* GUARD */
