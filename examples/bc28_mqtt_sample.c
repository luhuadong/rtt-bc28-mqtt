/*
 * Copyright (c) 2020, RudyLo <luhuadong@163.com>
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-04-22     luhuadong    the first version
 * 2020-08-16     luhuadong    add json parse
 */

#include <rtthread.h>
#include <bc28_mqtt.h>
#include <cJSON.h>


/* Alink JSON example

{
    "method":"thing.service.property.set",
    "id":"1686377270",
    "params":
    {
        "powerstate":1
    },
    "version":"1.0.0"
}

*/
static void mqtt_recv_cb(const char *json)
{
    cJSON *obj = cJSON_Parse(json);

    cJSON *powerstate = cJSON_GetObjectItem(cJSON_GetObjectItem(obj, "params"), "powerstate");

    if (powerstate->valueint == 1)
    {
        //light_on();
        rt_kprintf("switch on");
    }
    else if (powerstate->valueint == 0)
    {
        //light_off();
        rt_kprintf("switch off");
    }
}

static void bc28_mqtt_sample(void *parameter)
{
    /* init bc28 model */
    if (bc28_init() < 0)
    {
        rt_kprintf("(BC28) init failed\n");
        return;
    }

    /* attach network */
    if (bc28_client_attach() < 0)
    {
        rt_kprintf("(BC28) attach failed\n");
        return;
    }

    rt_kprintf("(BC28) attach ok\n");

    /* build mqtt network */
    while (bc28_build_mqtt_network() < 0)
    {
        bc28_mqtt_close();
        rt_kprintf("(BC28) rebuild mqtt network\n");
    }
    rt_kprintf("(BC28) MQTT connect ok\n");

    /* bind parser callback function */
    bc28_bind_parser(mqtt_recv_cb);

    /* subscribe a topic */
    char topic[256];
    rt_sprintf(topic, "%s/%s/user/hello", 
               PKG_USING_BC28_MQTT_PRODUCT_KEY, 
               PKG_USING_BC28_MQTT_DEVICE_NAME);

    bc28_mqtt_subscribe(topic);

    int cnt = 10;
    while (cnt--)
    {
        /* publish a topic */
        bc28_mqtt_publish(topic, "Hello, World!");
        rt_thread_mdelay(2000);
    }

    bc28_mqtt_close();
}

#ifdef FINSH_USING_MSH
MSH_CMD_EXPORT(bc28_mqtt_sample, BC28 MQTT sample);
#endif