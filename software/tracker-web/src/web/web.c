#include "../main.h"
#include "../util/timing.h"
#include "web.h"
#include "web_status.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include <json-c/json.h>
#include <libwebsockets.h>

#define HTDOCS_DIR "./htdocs"

#define WEBSOCKET_OUTPUT_LENGTH 16384 // characters
typedef struct {
    uint8_t buffer[LWS_PRE+WEBSOCKET_OUTPUT_LENGTH];
    uint32_t length;
    uint32_t sequence_id;
    pthread_mutex_t mutex;
    bool new; // Not locked by mutex
} websocket_output_t;

websocket_output_t ws_monitor_output = {
    .length = WEBSOCKET_OUTPUT_LENGTH,
    .sequence_id = 1,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .new = false
};

typedef struct websocket_user_session_t websocket_user_session_t;

struct websocket_user_session_t {
    struct lws *wsi;
    websocket_user_session_t *websocket_user_session_list;
    uint32_t last_sequence_id;
};

typedef struct {
    struct lws_context *context;
    struct lws_vhost *vhost;
    const struct lws_protocols *protocol;
    websocket_user_session_t *websocket_user_session_list;
} websocket_vhost_session_t;

enum protocol_ids {
    HTTP = 0,
    WS_MONITOR = 1,
    WS_CONTROL = 2,
    _TERMINATOR = 99
};

static app_state_t *app_state_controlptr = NULL;
void control_set_azel(float az, float el)
{
    if(app_state_controlptr == NULL)
    {
        return;
    }

    app_state_controlptr->control_state = CONTROL_STATE_MANUAL;

    app_state_controlptr->demand_az_deg = az;
    app_state_controlptr->demand_el_deg = el;
    app_state_controlptr->demand_valid = true;
    app_state_controlptr->demand_lastupdated_ms = timestamp_ms();
}

void control_set_isstrack(void)
{
    if(app_state_controlptr == NULL)
    {
        return;
    }

    app_state_controlptr->control_state = CONTROL_STATE_ISSTRACK;

    if(app_state_controlptr->iss_valid == true)
    {
        app_state_controlptr->demand_az_deg = app_state_controlptr->iss_az_deg;
        app_state_controlptr->demand_el_deg = app_state_controlptr->iss_el_deg;
        app_state_controlptr->demand_valid = true;
        app_state_controlptr->demand_lastupdated_ms = timestamp_ms();
    }
    else
    {
        app_state_controlptr->demand_valid = false;
    }
}

void control_set_suntrack(void)
{
    if(app_state_controlptr == NULL)
    {
        return;
    }

    app_state_controlptr->control_state = CONTROL_STATE_SUNTRACK;

    if(app_state_controlptr->sun_valid == true)
    {
        app_state_controlptr->demand_az_deg = app_state_controlptr->sun_az_deg;
        app_state_controlptr->demand_el_deg = app_state_controlptr->sun_el_deg;
        app_state_controlptr->demand_valid = true;
        app_state_controlptr->demand_lastupdated_ms = timestamp_ms();
    }
    else
    {
        app_state_controlptr->demand_valid = false;
    }
}

void control_request_calibration(void)
{
    if(app_state_controlptr == NULL)
    {
        return;
    }

    app_state_controlptr->heading_calstatus = 1;
}

int callback_ws(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    int32_t n;
    websocket_user_session_t *user_session = (websocket_user_session_t *)user;

    websocket_vhost_session_t *vhost_session =
            (websocket_vhost_session_t *)
            lws_protocol_vh_priv_get(lws_get_vhost(wsi),
                    lws_get_protocol(wsi));

    switch (reason)
    {
        case LWS_CALLBACK_PROTOCOL_INIT:
            vhost_session = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
                    lws_get_protocol(wsi),
                    sizeof(websocket_vhost_session_t));
            vhost_session->context = lws_get_context(wsi);
            vhost_session->protocol = lws_get_protocol(wsi);
            vhost_session->vhost = lws_get_vhost(wsi);
            break;

        case LWS_CALLBACK_ESTABLISHED:
            /* add ourselves to the list of live pss held in the vhd */
            lws_ll_fwd_insert(
                user_session,
                websocket_user_session_list,
                vhost_session->websocket_user_session_list
            );
            user_session->wsi = wsi;
            //user_session->last = vhost_session->current;
            break;

        case LWS_CALLBACK_CLOSED:
            /* remove our closing pss from the list of live pss */
            lws_ll_fwd_remove(
                websocket_user_session_t,
                websocket_user_session_list,
                user_session,
                vhost_session->websocket_user_session_list
            );
            break;


        case LWS_CALLBACK_SERVER_WRITEABLE:
            /* Write output data, if data exists */
            /* Look up protocol */
            if(vhost_session->protocol->id == WS_MONITOR)
            {
                pthread_mutex_lock(&ws_monitor_output.mutex);
                if(ws_monitor_output.length != 0 && user_session->last_sequence_id != ws_monitor_output.sequence_id)
                {
                    n = lws_write(wsi, (unsigned char*)&ws_monitor_output.buffer[LWS_PRE], ws_monitor_output.length, LWS_WRITE_TEXT);
                    if (!n)
                    {
                        pthread_mutex_unlock(&ws_monitor_output.mutex);
                        lwsl_err("ERROR %d writing to socket\n", n);
                        return -1;
                    }
                    user_session->last_sequence_id = ws_monitor_output.sequence_id;
                }
                pthread_mutex_unlock(&ws_monitor_output.mutex);
            }
            break;

        case LWS_CALLBACK_RECEIVE:
            if(len >= 8 && strcmp((const char *)in, "closeme\n") == 0)
            {
                lws_close_reason(wsi, LWS_CLOSE_STATUS_GOINGAWAY,
                         (unsigned char *)"seeya", 5);
                return -1;
            }
            if(len >= 5 && strcmp((const char *)in, "ping\n") == 0)
            {
                lws_close_reason(wsi, LWS_CLOSE_STATUS_GOINGAWAY,
                         (unsigned char *)"seeya", 5);
                return -1;
            }
            if(vhost_session->protocol->id == WS_CONTROL)
            {
                if(len >= 2 && len < 32)
                {
                    char message_string[32];
                    memcpy(message_string, in, len);
                    message_string[len] = '\0';

                    printf("RX: %s\n", message_string);

                    if(message_string[0] == 'M')
                    {
                        /* Manual Az & El */
                        char *field2_ptr;
                        float az, el;

                        /* Find field divider */
                        field2_ptr = memchr(message_string, ',', len);
                        if(field2_ptr != NULL)
                        {
                            /* Divide the strings */
                            field2_ptr[0] = '\0';

                            az = (float)strtof(&message_string[1],NULL);
                            el = (float)strtof(&field2_ptr[1],NULL);

                            control_set_azel(az, el);
                        }
                    }
                    else if(message_string[0] == 'S' && message_string[1] == 'U' && message_string[2] == 'N')
                    {
                        /* Sun Track */
                        control_set_suntrack();
                    }
                    else if(message_string[0] == 'I' && message_string[1] == 'S' && message_string[2] == 'S')
                    {
                        /* ISS TLE Track */
                        control_set_isstrack();
                    }
                    else if(message_string[0] == 'C' && message_string[1] == 'A' && message_string[2] == 'L')
                    {
                        /* Heading Magneto Calibration */
                        control_request_calibration();
                    }
                }
            }
            break;
        
        default:
            break;
    }

    return 0;
}

static struct lws_protocols protocols[] = {
    {
        .id = 0,
        .name = "http",
        .callback = lws_callback_http_dummy,
        .per_session_data_size = 0,
        .rx_buffer_size = 0,
    },
    {
        .id = 1,
        .name = "monitor",
        .callback = callback_ws,
        .per_session_data_size = 128,
        .rx_buffer_size = 4096,
    },
    {
        .id = 2,
        .name = "control",
        .callback = callback_ws,
        .per_session_data_size = 128,
        .rx_buffer_size = 4096,
    },
    LWS_PROTOCOL_LIST_TERM
};

static const struct lws_http_mount mount_opts = {
    /* .mount_next */       NULL,       /* linked-list "next" */
    /* .mountpoint */       "/",        /* mountpoint URL */
    /* .origin */           HTDOCS_DIR, /* serve from dir */
    /* .def */              "index.html",   /* default filename */
    /* .protocol */         NULL,
    /* .cgienv */           NULL,
    /* .extra_mimetypes */      NULL,
    /* .interpret */        NULL,
    /* .cgi_timeout */      0,
    /* .cache_max_age */        0,
    /* .auth_mask */        0,
    /* .cache_reusable */       0,
    /* .cache_revalidate */     0,
    /* .cache_intermediaries */ 0,
    /* .cache_no */             0,
    /* .origin_protocol */      (unsigned char)LWSMPRO_FILE,   /* files in a dir */
    /* .mountpoint_len */       1,      /* char count */
    /* .basic_auth_login_file */    NULL
};

/* Websocket Service Thread */
static struct lws_context *context;
static pthread_t ws_service_thread;
void *ws_service(void *arg)
{
    app_state_t *app_state_ptr = (app_state_t *)arg;
    int lws_err = 0;

    while(!app_state_ptr->app_exit)
    {
        lws_err = lws_service(context, 0);
        if(lws_err < 0)
        {
            fprintf(stderr, "Web: lws_service() reported error: %d\n", lws_err);
        }
    }

    pthread_exit(NULL);
}

void *loop_web(void *arg)
{
    app_state_t *app_state_ptr = (app_state_t *)arg;
    app_state_controlptr = app_state_ptr;

    int status_json_length;
    char *status_json_str;

    struct lws_context_creation_info info;
    int logs = LLL_USER | LLL_ERR | LLL_WARN; // | LLL_NOTICE;

    lws_set_log_level(logs, NULL);

    memset(&info, 0, sizeof info);

    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8 | LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
    context = lws_create_context(&info); 
    if (!context)
    {
        lwsl_err("LWS: Init failed.\n");
        return NULL;
    }

    info.vhost_name = "localhost";
    info.port = 80; //config->web_port;
    info.mounts = &mount_opts;
    info.error_document_404 = "/404.html";
    info.protocols = protocols;

    if(!lws_create_vhost(context, &info))
    {
        lwsl_err("LWS: Failed to create vhost\n");
        lws_context_destroy(context);
        return NULL;
    }

    /* Create dedicated ws server thread */
    if(0 != pthread_create(&ws_service_thread, NULL, ws_service, (void *)app_state_ptr))
    {
        fprintf(stderr, "Error creating web_lws pthread\n");
    }
    else
    {
        pthread_setname_np(ws_service_thread, "Web - LWS");
    }

    //uint64_t last_status_sent_monotonic = 0;
    while(!app_state_ptr->app_exit)
    {
        web_status_service(app_state_ptr, &status_json_str);
        status_json_length = strlen(status_json_str);

        pthread_mutex_lock(&ws_monitor_output.mutex);
        memcpy(&ws_monitor_output.buffer[LWS_PRE], status_json_str, status_json_length);
        ws_monitor_output.length = status_json_length;
        ws_monitor_output.sequence_id++;
        pthread_mutex_unlock(&ws_monitor_output.mutex);

        lws_callback_on_writable_all_protocol(context, &protocols[WS_MONITOR]);

        sleep_ms(100);
    }

    lws_cancel_service(context);

    pthread_join(ws_service_thread, NULL);

    lws_context_destroy(context);

    pthread_exit(NULL);
}
