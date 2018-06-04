#include "sdkconfig.h"

#include <stdexcept>

#include "HTTPSClient.hpp"


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
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            // ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            // ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            // printf("%.*s", evt->data_len, (char *)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // if (!esp_http_client_is_chunked_response(evt->client)) {
            //     printf("%.*s", evt->data_len, (char *)evt->data);
            // }

            break;
        case HTTP_EVENT_ON_FINISH:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            // ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
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
    : _connection_open(false), _user_agent(user_agent), _ca_pem(ca_pem), _timeout(timeout) {
    ESP_LOGD(TAG, "HTTPSClient(%p)", static_cast<void *>(this));
}

int HTTPSClient::get(const char *_url) {
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
        HTTP_METHOD_GET, /*!< HTTP Method */
        this->_timeout, /*!< Network timeout in milliseconds */
        false, /*!< Disable HTTP automatic redirects */
        0, /*!< Max redirection number, using default value if zero*/
        _http_event_handle, /*!< HTTP Event Handle */
        HTTP_TRANSPORT_OVER_SSL, /*!< HTTP transport type, see `esp_http_client_transport_t` */
        0, /*!< HTTP buffer size (both send and receive) */
        this,
    };
    this->_client = (esp_http_client_handle_t*)malloc(sizeof(esp_http_client_handle_t));

    esp_http_client_handle_t c = esp_http_client_init(&config);
    err = esp_http_client_perform(c);
#else
    this->_client = (struct mg_mgr*)malloc(sizeof(struct mg_mgr));

    memset(_client, 0, sizeof(struct mg_mgr));

    ESP_LOGD(TAG, "mgr_init");

    mg_mgr_init(_client, this);

    struct mg_connect_opts http_opts;
    memset(&http_opts, 0, sizeof(http_opts));

    //http_opts.ssl_ca_cert = this->_ca_pem;

    ESP_LOGD(TAG, "mg_connect_http");

    this->_nc = mg_connect_http_opt(_client, ev_handler, this, http_opts, _url, NULL, NULL);

    if (this->_nc == NULL) {
        throw std::runtime_error("connection failed");
    }
#endif


    if (err == ESP_OK) {
#if defined(CONFIG_USE_ESP_TLS)

        long start = millis();

        int length = -1;
        this->status_code = esp_http_client_get_status_code(c);
        length = esp_http_client_get_content_length(c);  
        ESP_LOGI(TAG, "Status = %d, content_length = %d", this->status_code, length);

        while (true) {
            int buff_len = this->read(text, _HTTPS_CLIENT_BUFFSIZE);
            if (buff_len < 0) {
                if (errno == EAGAIN) {
                    long now = millis();
                    if (_timeout <= 0 || (now - start) < _timeout) {
                        continue;
                    }
                }
                ESP_LOGI(TAG, "error: = %d", errno);
                break;
            } else if (buff_len > 0) {
                if (this->_read_cb && this->status_code == 200) {
                    this->_read_cb(text, buff_len);
                }
                ESP_LOGV(TAG, "read %d", buff_len);
            } else if (buff_len == 0) { /*packet over*/
                ESP_LOGD(TAG, "connection closed");
                break;
            } else {
                ESP_LOGD(TAG, "unexpected recv result");
                break;
            }
        }
#else
        ESP_LOGD(TAG, "mg_mgr_poll");
        while (this->exit_flag == 0) {
            mg_mgr_poll(_client, 100);
        }

        ESP_LOGD(TAG, "mg_mgr_poll finished");
        
        ESP_LOGD(TAG, "mg_mgr_free");

        mg_mgr_free(_client);
#endif
    } else {
        throw std::runtime_error("connection failed");
    }

    return this->status_code;
}


int HTTPSClient::write(const char *buf, int len) {
    if (!this->_connection_open) {
        throw std::runtime_error("connection not open");
    }

    ESP_LOGD(TAG, "writing");

    int ret = 0;
    int written_bytes = 0;

    do {
#if defined(CONFIG_USE_ESP_TLS)
        ret = esp_http_client_write(*this->_client, buf + written_bytes, len - written_bytes);
#else
        mg_send(_nc, buf, len);
#endif
        if (ret >= 0) {
            ESP_LOGD(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else {
            throw std::runtime_error("connection write failed");
        }
    } while (written_bytes < len);

    ESP_LOGD(TAG, "wrote %d", written_bytes);

    return written_bytes;
}


int HTTPSClient::read(char *buf, int count) {
    if (!this->_connection_open) {
        throw std::runtime_error("connection not open");
    }

    int recv_bytes = 0;

    // long start = millis();

    int ret = 0;
    memset(buf, 0, static_cast<size_t>(count));

    bool loop = true;

    do {
#if defined(CONFIG_USE_ESP_TLS)
        ret = esp_http_client_read(*this->_client, buf, count);
#else

#endif

        if (ret < 0) {
            ESP_LOGE(TAG, "esp_http_client_read  returned -0x%x", -ret);
            throw std::runtime_error("read from connection failed");
        }

        if (ret == 0) {
            // no more data to read
            return recv_bytes;
        }
        recv_bytes += ret;

        loop = false;

    } while (loop);

    return recv_bytes;
}


HTTPSClient::~HTTPSClient() {
    ESP_LOGD(TAG, "~HTTPSClient(%p)", static_cast<void *>(this));

    if (this->_client != nullptr) {
#if defined(CONFIG_USE_ESP_TLS)
        ESP_LOGD(TAG, "cleanup(%p)", static_cast<void *>(this->_client));
        esp_http_client_cleanup(*this->_client);
#else

#endif
        free(this->_client);
        if (this->_nc) {
            ESP_LOGW(TAG, "mg_connection still exists (%p)", static_cast<void *>(this->_nc));
        }
    }
}
