/*
 * Copyright (c) 2020, RudyLo <luhuadong@163.com>
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-04-08     luhuadong    the first version
 * 2020-06-04     luhuadong    v0.0.1
 * 2020-07-25     luhuadong    support state transition
 */

#ifndef __AT_BC28_H__
#define __AT_BC28_H__

#include <at.h>

typedef enum 
{
    AT_STAT_INIT = 0,
    AT_STAT_ATTACH,
    AT_STAT_DEATTACH,
    AT_STAT_CONNECTED,
    
} bc28_state_t;

struct bc28_device
{
    bc28_state_t      stat;
    struct at_client *client;
    rt_mutex_t        lock;
};
typedef struct bc28_device *bc28_device_t;

/* NB-IoT */
int at_client_attach(void);
int at_client_deattach(void);

/* MQTT */
int bc28_mqtt_auth(void);
int bc28_mqtt_open(void);
int bc28_mqtt_close(void);
int bc28_mqtt_connect(void);
int bc28_mqtt_disconnect(void);
int bc28_mqtt_subscribe(const char *topic);
int bc28_mqtt_unsubscribe(const char *topic);
int bc28_mqtt_publish(const char *topic, const char *msg);

/* Network */
int bc28_init(void);
int build_mqtt_network(void);
int rebuild_mqtt_network(void);

#endif /* __AT_BC28_H__ */
