/*
 * Copyright (c) 2020, RudyLo <luhuadong@163.com>
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-04-22     luhuadong    the first version
 */

#include <rtthread.h>
#include <bc28_mqtt.h>

static void bc28_mqtt_sample(void *parameter)
{
    if(RT_EOK != bc28_init())
    {
        rt_kprintf("init failed\n");
        return;
    }
    rt_kprintf("attach ok\n");

    while (RT_EOK != build_mqtt_network())
    {
        bc28_mqtt_close();
        rt_kprintf("rebuild mqtt network\n");
    }

    rt_kprintf("MQTT connect ok\n");

    /* user-code */
    char topic[256];
    rt_sprintf(topic, "%s/%s/user/hello", 
               PKG_USING_BC28_MQTT_PRODUCT_KEY, 
               PKG_USING_BC28_MQTT_DEVICE_NAME);

    bc28_mqtt_subscribe(topic);

    int cnt = 10;
    while (cnt--)
    {
        bc28_mqtt_publish(topic, "Hello, World!");
        rt_thread_mdelay(2000);
    }

    bc28_mqtt_close();
}

#ifdef FINSH_USING_MSH
MSH_CMD_EXPORT(bc28_mqtt_sample, BC28 MQTT sample);
#endif