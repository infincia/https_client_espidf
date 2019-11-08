#include "sdkconfig.h"

#include <stdexcept>

#include "HTTPSClient.hpp"


#include <errno.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <http_parser.h>

static const char *TAG = "HTTPSClient";



#if defined(CONFIG_USE_ESP_TLS)
static unsigned long IRAM_ATTR millis() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static esp_err_t _http_event_handle(esp_http_client_event_t *evt) {
    HTTPSClient * instance = static_cast<HTTPSClient *>(evt->user_data);

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGV(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGV(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGV(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGV(TAG, "HTTP_EVENT_ON_HEADER");
            // printf("%.*s", evt->data_len, (char *)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGV(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (instance->_read_cb) {
                    instance->_read_cb((char*)evt->data, evt->data_len);
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGV(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGV(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            ESP_LOGV(TAG, "UNKNOWN HTTP EVENT: %d", evt->event_id);
            break;
    }
    return ESP_OK;
}
#else
static void ev_handler(struct mg_connection *c, int ev, void *p, void *user_data) {    
    HTTPSClient * instance = static_cast<HTTPSClient *>(c->mgr->user_data);

    if (instance == NULL) {
        ESP_LOGE(TAG, "instance missing");
        return;
    }

    if (ev == MG_EV_CONNECT) {

    } else if (ev == MG_EV_RECV) {

    } else if (ev == MG_EV_SEND) {

    } else if (ev == MG_EV_POLL) {

    } else if (ev == MG_EV_TIMER) {

    } else if (ev == MG_EV_HTTP_REPLY) {
        struct http_message *hm = (struct http_message *)p;

        ESP_LOGD(TAG, "MG_EV_HTTP_REPLY <code:%d,body length:%d>", hm->resp_code, hm->body.len);

        c->flags |= MG_F_CLOSE_IMMEDIATELY;
        instance->status_code = hm->resp_code;
    } else if (ev == MG_EV_HTTP_CHUNK) {
        struct http_message *hm = (struct http_message *)p;

        ESP_LOGD(TAG, "MG_EV_HTTP_CHUNK <code:%d,chunk length:%d>", hm->resp_code, hm->body.len);

        c->flags |= MG_F_DELETE_CHUNK;

        if (instance->_read_cb && hm->resp_code == 200) {
           instance->_read_cb(hm->body.p, hm->body.len);
        }
    } else if (ev == MG_EV_CLOSE) {
        ESP_LOGD(TAG, "MG_EV_CLOSE");
        instance->exit_flag = 1;
    } else {
        ESP_LOGD(TAG, "unknown mg event: %d", ev);
    }
}
#endif


HTTPSClient::HTTPSClient(const std::string &user_agent, const char *ca_pem, int timeout)
    : _user_agent(user_agent), _ca_pem(ca_pem), _timeout(timeout) {
    ESP_LOGD(TAG, "HTTPSClient(%p)", static_cast<void *>(this));
}

int HTTPSClient::get(const char *_url) {
    exit_flag = 0;
    status_code = 0;
    
    esp_err_t err = ESP_OK;

#if defined(CONFIG_USE_ESP_TLS)
    esp_http_client_config_t config = {
        _url, /*!< HTTP URL, the information on the URL is most important, it overrides the
                        other fields below, if any */
        nullptr, /*!< Domain or IP as string */
        0,
        nullptr, /*!< Using for Http authentication */
        nullptr, /*!< Using for Http authentication */
        HTTP_AUTH_TYPE_NONE, /*!< Http authentication type, see `esp_http_client_auth_type_t` */
        nullptr, /*!< HTTP Path, if not set, default is `/` */
        nullptr, /*!< HTTP query */
        this->_ca_pem, /*!< SSL Certification, PEM format as string, if the client requires to
                         verify server */
        nullptr,  /*!< SSL client certification, PEM format as string, if the server requires to verify client */
        nullptr, /*!< SSL client key, PEM format as string, if the server requires to verify client */
        HTTP_METHOD_GET, /*!< HTTP Method */
        this->_timeout, /*!< Network timeout in milliseconds */
        false, /*!< Disable HTTP automatic redirects */
        0, /*!< Max redirection number, using default value if zero*/
        _http_event_handle, /*!< HTTP Event Handle */
        HTTP_TRANSPORT_OVER_SSL, /*!< HTTP transport type, see `esp_http_client_transport_t` */
        0, /*!< HTTP buffer size (both send and receive) */
        this,  /*!< HTTP user_data context */
        false,  /*!< Set asynchronous mode, only supported with HTTPS for now */
        true /*!< Use a global ca_store for all the connections in which this bool is set. */
    };

    esp_http_client_handle_t _client = esp_http_client_init(&config);
    size_t s = sizeof(esp_http_client_handle_t);
    ESP_LOGD(TAG, "size of _client is %d", s);

    err = esp_http_client_perform(_client);

#else
    size_t s = sizeof(struct mg_mgr);

    ESP_LOGD(TAG, "size of _client is %d", s);

    struct mg_mgr _client;
    memset(&_client, 0, s);

    ESP_LOGD(TAG, "mgr_init");

    mg_mgr_init(&_client, this);

    struct mg_connect_opts http_opts;
    memset(&http_opts, 0, sizeof(http_opts));

    //http_opts.ssl_ca_cert = this->_ca_pem;

    ESP_LOGD(TAG, "mg_connect_http");

    struct mg_connection *_nc = mg_connect_http_opt(&_client, ev_handler, this, http_opts, _url, NULL, NULL);

    if (_nc == NULL) {
        ESP_LOGD(TAG, "mg_mgr_free");
        mg_mgr_free(&_client);
        throw std::runtime_error("connection failed");
    }

#endif


    if (err == ESP_OK) {
#if defined(CONFIG_USE_ESP_TLS)
        int length = -1;
        this->status_code = esp_http_client_get_status_code(_client);
        length = esp_http_client_get_content_length(_client);  
        ESP_LOGD(TAG, "Status = %d, content_length = %d", this->status_code, length);
        esp_http_client_cleanup(_client);
#else
        ESP_LOGD(TAG, "mg_mgr_poll");
        while (this->exit_flag == 0) {
            mg_mgr_poll(&_client, 100);
        }

        ESP_LOGD(TAG, "mg_mgr_poll finished");
        
        ESP_LOGD(TAG, "mg_mgr_free");

        mg_mgr_free(&_client);
#endif
    } else {
        throw std::runtime_error("connection failed");
    }

    return this->status_code;
}

int HTTPSClient::post(const char* _url, const char* _body) {
    exit_flag = 0;
    status_code = 0;
    
    esp_err_t err = ESP_OK;

    #if defined(CONFIG_USE_ESP_TLS)

    #else

    size_t s = sizeof(struct mg_mgr);

    ESP_LOGD(TAG, "size of _client is %d", s);

    struct mg_mgr _client;
    memset(&_client, 0, s);

    ESP_LOGD(TAG, "mgr_init");

    mg_mgr_init(&_client, this);

    struct mg_connect_opts http_opts;
    memset(&http_opts, 0, sizeof(http_opts));

    //http_opts.ssl_ca_cert = this->_ca_pem;

    ESP_LOGD(TAG, "mg_connect_http");

    struct mg_connection *_nc  = mg_connect_http_opt(&_client, ev_handler, this, http_opts, _url, NULL, _body);

    if (_nc == NULL) {
        ESP_LOGD(TAG, "mg_mgr_free");
        mg_mgr_free(&_client);
        throw std::runtime_error("connection failed");
    }

    if (err == ESP_OK) {
        ESP_LOGD(TAG, "mg_mgr_poll");
        while (this->exit_flag == 0) {
            mg_mgr_poll(&_client, 100);
        }

        ESP_LOGD(TAG, "mg_mgr_poll finished");
        
        ESP_LOGD(TAG, "mg_mgr_free");

        mg_mgr_free(&_client);
    } else {
        throw std::runtime_error("connection failed");
    }
    #endif

    return this->status_code;
}


HTTPSClient::~HTTPSClient() {
    ESP_LOGD(TAG, "~HTTPSClient(%p)", static_cast<void *>(this));
}
