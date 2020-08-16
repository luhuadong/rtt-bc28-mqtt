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
 * 2020-08-16     luhuadong    uniform function name
 */

#ifndef __AT_BC28_H__
#define __AT_BC28_H__

#include <at.h>

typedef enum bc28_stat
{
    BC28_STAT_INIT = 0,
    BC28_STAT_ATTACH,
    BC28_STAT_DEATTACH,
    BC28_STAT_CONNECTED,
    BC28_STAT_DISCONNECTED

} bc28_stat_t;

struct bc28_device
{
    rt_base_t         reset_pin;
    rt_base_t         adc_pin;
    bc28_stat_t       stat;
    rt_mutex_t        lock;
    char              imei[16];
    char              ipaddr[16];

    struct at_client *client;
    void (*parser)(const char *json);
};
typedef struct bc28_device *bc28_device_t;

/* NB-IoT */
int  bc28_client_attach(void);
int  bc28_client_deattach(void);

/* MQTT */
int  bc28_mqtt_auth(void);
int  bc28_mqtt_open(void);
int  bc28_mqtt_close(void);
int  bc28_mqtt_connect(void);
int  bc28_mqtt_disconnect(void);
int  bc28_mqtt_subscribe(const char *topic);
int  bc28_mqtt_unsubscribe(const char *topic);
int  bc28_mqtt_publish(const char *topic, const char *msg);
void bc28_bind_parser(void (*callback)(const char *json));

/* NB-IoT Network */
int  bc28_init(void);
int  bc28_build_mqtt_network(void);
int  bc28_rebuild_mqtt_network(void);

#endif /* __AT_BC28_H__ */
