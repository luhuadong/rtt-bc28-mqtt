/*
 * Copyright (c) 2020, RudyLo <luhuadong@163.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-02-07     luhuadong    the first version
 */

#ifndef __AT_BC28_H__
#define __AT_BC28_H__

#define JSON_DATA_PACK_TEST      "{\"id\":\"125\",\"version\":\"1.0\",\"params\":{\"Temp\":%s,\"Humi\":%s,\"Dust\":%s,\"TVOC\":%s,\"eCO2\":%s},\"method\":\"thing.event.property.post\"}\x1A"
#define JSON_DATA_PACK           "{\"id\":\"125\",\"version\":\"1.0\",\"params\":{\"Temp\":%d.%02d,\"Humi\":%d.%02d,\"Dust\":%d,\"TVOC\":%d,\"eCO2\":%d},\"method\":\"thing.event.property.post\"}\x1A"
#define JSON_DATA_PACK_STR       "{\"id\":\"125\",\"version\":\"1.0\",\"params\":{\"Temp\":%s,\"Humi\":%s,\"Dust\":%d,\"TVOC\":%d,\"eCO2\":%d},\"method\":\"thing.event.property.post\"}\x1A"
#define AIR_MSG                  "[Air] Temp: %d.%02d'C, Humi: %d.%02d%, Dust: %dug/m3, TVOC: %dppb, eCO2: %dppm\n"
#define MQTT_TOPIC_HELLO         "/a1p8Pngb3oY/BC28/user/hello"
#define MQTT_TOPIC_UPLOAD        "/sys/a1p8Pngb3oY/BC28/thing/event/property/post"

typedef enum {

	AT_STAT_INIT = 0,
	AT_STAT_ATTACH,
	AT_STAT_DEATTACH,
	AT_STAT_CONNECTED,
} at_stat_t;

at_stat_t at_stat;

void user_btn_init(void);

/* NB-IoT */

int at_client_dev_init(void);
int at_client_attach(void);
int at_client_deattach(void);

int bc28_mqtt_auth(void);
int bc28_mqtt_open(void);
int bc28_mqtt_close(void);
int bc28_mqtt_connect(void);
int bc28_mqtt_disconnect(void);
int bc28_mqtt_subscribe(const char *topic);
int bc28_mqtt_unsubscribe(const char *topic);
int bc28_mqtt_publish(const char *topic, const char *msg);

int bc28_init(void);
int build_mqtt_network(void);
int rebuild_mqtt_network(void);

#endif /* __AT_BC28_H__ */
