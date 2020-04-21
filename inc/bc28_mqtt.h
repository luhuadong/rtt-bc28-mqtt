/*
 * Copyright (c) 2020, RudyLo <luhuadong@163.com>
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-04-08     luhuadong    the first version
 */

#ifndef __AT_BC28_H__
#define __AT_BC28_H__

typedef enum {

	AT_STAT_INIT = 0,
	AT_STAT_ATTACH,
	AT_STAT_DEATTACH,
	AT_STAT_CONNECTED,
} at_stat_t;

at_stat_t at_stat;

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
