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
 * 2020-08-16     luhuadong    support bind recv parser
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <at.h>

#define LOG_TAG                       "pkg.bc28_mqtt"
#define LOG_LVL                       LOG_LVL_DBG
#include <ulog.h>

#include "bc28_mqtt.h"

#define BC28_ADC0_PIN                 PKG_USING_BC28_ADC0_PIN
#define BC28_RESET_N_PIN              PKG_USING_BC28_RESET_PIN
#define BC28_OP_BAND                  PKG_USING_BC28_MQTT_OP_BAND

#define AT_CLIENT_DEV_NAME            PKG_USING_BC28_AT_CLIENT_DEV_NAME
#define AT_CLIENT_BAUD_RATE           PKG_USING_BC28_MQTT_BAUD_RATE

#define PRODUCT_KEY                   PKG_USING_BC28_MQTT_PRODUCT_KEY
#define DEVICE_NAME                   PKG_USING_BC28_MQTT_DEVICE_NAME
#define DEVICE_SECRET                 PKG_USING_BC28_MQTT_DEVICE_SECRET

#define KEEP_ALIVE_TIME               PKG_USING_BC28_MQTT_KEEP_ALIVE

#define AT_OK                         "OK"
#define AT_ERROR                      "ERROR"

#define AT_TEST                       "AT"
#define AT_ECHO_OFF                   "ATE0"
#define AT_QREGSWT_2                  "AT+QREGSWT=2"
#define AT_AUTOCONNECT_DISABLE        "AT+NCONFIG=AUTOCONNECT,FALSE"
#define AT_REBOOT                     "AT+NRB"
#define AT_NBAND                      "AT+NBAND=%d"
#define AT_FUN_ON                     "AT+CFUN=1"
#define AT_LED_ON                     "AT+QLEDMODE=1"
#define AT_EDRX_OFF                   "AT+CEDRXS=0,5"
#define AT_PSM_OFF                    "AT+CPSMS=0"
#define AT_RECV_AUTO                  "AT+NSONMI=2"
#define AT_UE_ATTACH                  "AT+CGATT=1"
#define AT_UE_DEATTACH                "AT+CGATT=0"
#define AT_QUERY_IMEI                 "AT+CGSN=1"
#define AT_QUERY_IMSI                 "AT+CIMI"
#define AT_QUERY_STATUS               "AT+NUESTATS"
#define AT_QUERY_REG                  "AT+CEREG?"
#define AT_QUERY_IPADDR               "AT+CGPADDR"
#define AT_QUERY_ATTACH               "AT+CGATT?"
#define AT_UE_ATTACH_SUCC             "+CGATT:1"

#define AT_MQTT_AUTH                  "AT+QMTCFG=\"aliauth\",0,\"%s\",\"%s\",\"%s\""
#define AT_MQTT_ALIVE                 "AT+QMTCFG=\"keepalive\",0,%u"
#define AT_MQTT_OPEN                  "AT+QMTOPEN=0,\"%s.iot-as-mqtt.cn-shanghai.aliyuncs.com\",1883"
#define AT_MQTT_OPEN_SUCC             "+QMTOPEN: 0,0"
#define AT_MQTT_CLOSE                 "AT+QMTCLOSE=0"
#define AT_MQTT_CONNECT               "AT+QMTCONN=0,\"%s\""
#define AT_MQTT_CONNECT_SUCC          "+QMTCONN: 0,0,0"
#define AT_MQTT_DISCONNECT            "AT+QMTDISC=0"
#define AT_MQTT_SUB                   "AT+QMTSUB=0,1,\"%s\",0"
#define AT_MQTT_SUB_SUCC              "+QMTSUB: 0,1,0,1"
#define AT_MQTT_UNSUB                 "AT+QMTUNS=0,1, \"%s\""
#define AT_MQTT_PUB                   "AT+QMTPUB=0,0,0,0,\"%s\""
#define AT_MQTT_PUB_SUCC              "+QMTPUB: 0,0,0"

#define AT_QMTSTAT_CLOSED             1
#define AT_QMTSTAT_PINGREQ_TIMEOUT    2
#define AT_QMTSTAT_CONNECT_TIMEOUT    3
#define AT_QMTSTAT_CONNACK_TIMEOUT    4
#define AT_QMTSTAT_DISCONNECT         5
#define AT_QMTSTAT_WORNG_CLOSE        6
#define AT_QMTSTAT_INACTIVATED        7

#define AT_QMTRECV_DATA               "+QMTRECV: %d,%d,\"%s\",\"%s\""

#define AT_CLIENT_RECV_BUFF_LEN       256
#define AT_DEFAULT_TIMEOUT            5000

static struct bc28_device bc28 = {
    .reset_pin = PKG_USING_BC28_RESET_PIN,
    .adc_pin   = PKG_USING_BC28_ADC0_PIN,
    .stat      = BC28_STAT_DISCONNECTED
};

static char   buf[AT_CLIENT_RECV_BUFF_LEN];

/**
 * This function will show response information.
 *
 * @param resp  the response
 *
 * @return void
 */
static void show_resp_info(at_response_t resp)
{
    RT_ASSERT(resp);
    
    /* Print response line buffer */
    const char *line_buffer = RT_NULL;

    for(rt_size_t line_num = 1; line_num <= resp->line_counts; line_num++)
    {
        if((line_buffer = at_resp_get_line(resp, line_num)) != RT_NULL)
            LOG_I("line %d buffer : %s", line_num, line_buffer);
        else
            LOG_I("Parse line buffer error!");
    }
}

/**
 * This function will send command and check the result.
 *
 * @param client    at client handle
 * @param cmd       command to at client
 * @param resp_expr expected response expression
 * @param lines     response lines
 * @param timeout   waiting time
 *
 * @return  RT_EOK       send success
 *         -RT_ERROR     send failed
 *         -RT_ETIMEOUT  response timeout
 *         -RT_ENOMEM    alloc memory failed
 */
static int check_send_cmd(const char* cmd, const char* resp_expr, 
                          const rt_size_t lines, const rt_int32_t timeout)
{
    at_response_t resp = RT_NULL;
    int result = 0;
    char resp_arg[AT_CMD_MAX_LEN] = { 0 };

    resp = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, lines, rt_tick_from_millisecond(timeout));
    if (resp == RT_NULL)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    result = at_obj_exec_cmd(bc28.client, resp, cmd);
    if (result < 0)
    {
        LOG_E("AT client send commands failed or wait response timeout!");
        goto __exit;
    }

    //show_resp_info(resp);

    if (resp_expr) 
    {
        if (at_resp_parse_line_args_by_kw(resp, resp_expr, "%s", resp_arg) <= 0)
        {
            LOG_E("# >_< Failed");
            result = -RT_ERROR;
            goto __exit;
        }
        else
        {
            LOG_D("# ^_^ successed");
        }
    }
    
    result = RT_EOK;

__exit:
    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}

static char *bc28_get_imei(bc28_device_t device)
{
    at_response_t resp = RT_NULL;

    resp = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, 0, rt_tick_from_millisecond(AT_DEFAULT_TIMEOUT));
    if (resp == RT_NULL)
    {
        LOG_E("No memory for response structure!");
        return RT_NULL;
    }

    /* send "AT+CGSN=1" commond to get device IMEI */
    if (at_obj_exec_cmd(device->client, resp, AT_QUERY_IMEI) != RT_EOK)
    {
        at_delete_resp(resp);
        return RT_NULL;
    }
    
    if (at_resp_parse_line_args(resp, 2, "+CGSN:%s", device->imei) <= 0)
    {
        LOG_E("device parse \"%s\" cmd error.", AT_QUERY_IMEI);
        at_delete_resp(resp);
        return RT_NULL;
    }
    LOG_D("IMEI code: %s", device->imei);

    at_delete_resp(resp);
    return device->imei;
}

static char *bc28_get_ipaddr(bc28_device_t device)
{
    at_response_t resp = RT_NULL;

    resp = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, 0, rt_tick_from_millisecond(AT_DEFAULT_TIMEOUT));
    if (resp == RT_NULL)
    {
        LOG_E("No memory for response structure!");
        return RT_NULL;
    }

    /* send "AT+CGPADDR" commond to get IP address */
    if (at_obj_exec_cmd(device->client, resp, AT_QUERY_IPADDR) != RT_EOK)
    {
        at_delete_resp(resp);
        return RT_NULL;
    }

    /* parse response data "+CGPADDR: 0,<IP_address>" */
    if (at_resp_parse_line_args_by_kw(resp, "+CGPADDR:", "+CGPADDR:%*d,%s", device->ipaddr) <= 0)
    {
        LOG_E("device parse \"%s\" cmd error.", AT_QUERY_IPADDR);
        at_delete_resp(resp);
        return RT_NULL;
    }
    LOG_D("IP address: %s", device->ipaddr);

    at_delete_resp(resp);
    return device->ipaddr;
}

static int bc28_mqtt_set_alive(rt_uint32_t keepalive_time)
{
    LOG_D("MQTT set alive.");

    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_ALIVE, keepalive_time);

    return check_send_cmd(cmd, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

int bc28_mqtt_auth(void)
{
    LOG_D("MQTT set auth info.");

    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_AUTH, PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET);

    return check_send_cmd(cmd, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

/**
 * Open MQTT socket.
 *
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int bc28_mqtt_open(void)
{
    LOG_D("MQTT open socket.");

    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_OPEN, PRODUCT_KEY);

    return check_send_cmd(cmd, AT_MQTT_OPEN_SUCC, 4, 75000);
}

/**
 * Close MQTT socket.
 *
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int bc28_mqtt_close(void)
{
    LOG_D("MQTT close socket.");

    return check_send_cmd(AT_MQTT_CLOSE, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

/**
 * Connect MQTT socket.
 *
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int bc28_mqtt_connect(void)
{
    LOG_D("MQTT connect...");

    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_CONNECT, bc28.imei);
    LOG_D("%s", cmd);

    if (check_send_cmd(AT_MQTT_CONNECT, AT_MQTT_CONNECT_SUCC, 4, 10000) < 0)
    {
        LOG_D("MQTT connect failed.");
        return -RT_ERROR;
    }
    
    bc28.stat = BC28_STAT_CONNECTED;
    return RT_EOK;
}

/**
 * Disconnect MQTT socket.
 *
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int bc28_mqtt_disconnect(void)
{
    if (check_send_cmd(AT_MQTT_DISCONNECT, AT_OK, 0, AT_DEFAULT_TIMEOUT) < 0)
    {
        LOG_D("MQTT disconnect failed.");
        return -RT_ERROR;
    }
    
    bc28.stat = BC28_STAT_CONNECTED;
    return RT_EOK;
}

/**
 * Subscribe MQTT topic.
 *
 * @param  topic : mqtt topic
 * 
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int bc28_mqtt_subscribe(const char *topic)
{
    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_SUB, topic);

    return check_send_cmd(cmd, AT_MQTT_SUB_SUCC, 4, AT_DEFAULT_TIMEOUT);
}

/**
 * Unsubscribe MQTT topic.
 *
 * @param  topic : mqtt topic
 * 
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int bc28_mqtt_unsubscribe(const char *topic)
{
    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_UNSUB, topic);

    return check_send_cmd(cmd, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

/**
 * Publish MQTT message to topic.
 *
 * @param  topic : mqtt topic
 * @param  msg   : message
 * 
 * @return 0 : exec at cmd success
 *        <0 : exec at cmd failed
 */
int bc28_mqtt_publish(const char *topic, const char *msg)
{
    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_PUB, topic);

    /* set AT client end sign to deal with '>' sign.*/
    at_set_end_sign('>');

    //check_send_cmd(cmd, ">", 3, AT_DEFAULT_TIMEOUT);
    check_send_cmd(cmd, RT_NULL, 2, AT_DEFAULT_TIMEOUT);
    LOG_D("publish...");

    /* reset the end sign for data conflict */
    at_set_end_sign(0);

    return check_send_cmd(msg, AT_MQTT_PUB_SUCC, 4, AT_DEFAULT_TIMEOUT);
}

int bc28_client_attach(void)
{
    int result = 0;
    char cmd[AT_CMD_MAX_LEN] = {0};

    /* close echo */
    check_send_cmd(AT_ECHO_OFF, AT_OK, 0, AT_DEFAULT_TIMEOUT);

    /* 关闭电信自动注册功能 */
    result = check_send_cmd(AT_QREGSWT_2, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 禁用自动连接网络 */
    result = check_send_cmd(AT_AUTOCONNECT_DISABLE, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 重启模块 */
    check_send_cmd(AT_REBOOT, AT_OK, 0, 10000);

    while(RT_EOK != check_send_cmd(AT_TEST, AT_OK, 0, AT_DEFAULT_TIMEOUT))
    {
        rt_thread_mdelay(1000);
    }

    /* 查询IMEI号 */
    result = check_send_cmd(AT_QUERY_IMEI, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    if (RT_NULL == bc28_get_imei(&bc28))
    {
        LOG_E("Get IMEI code failed.");
        return -RT_ERROR;
    }

    /* 指定要搜索的频段 */
    rt_sprintf(cmd, AT_NBAND, BC28_OP_BAND);
    result = check_send_cmd(cmd, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 打开模块的调试灯 */
    //result = check_send_cmd(AT_LED_ON, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    //if (result != RT_EOK) return result;

    /* 将模块设置为全功能模式(开启射频功能) */
    result = check_send_cmd(AT_FUN_ON, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 接收到TCP数据时，自动上报 */
    result = check_send_cmd(AT_RECV_AUTO, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 关闭eDRX */
    result = check_send_cmd(AT_EDRX_OFF, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 关闭PSM */
    result = check_send_cmd(AT_PSM_OFF, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 查询卡的国际识别码(IMSI号)，用于确认SIM卡插入正常 */
    result = check_send_cmd(AT_QUERY_IMSI, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 触发网络连接 */
    result = check_send_cmd(AT_UE_ATTACH, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 查询模块状态 */
    //at_exec_cmd(resp, "AT+NUESTATS");

    /* 查询网络注册状态 */
    //at_exec_cmd(resp, "AT+CEREG?");

    /* 查看信号强度 */
    //at_exec_cmd(resp, "AT+CSQ");

    /* 查询网络是否被激活，通常需要等待30s */
    int count = 60;
    while(count > 0 && RT_EOK != check_send_cmd(AT_QUERY_ATTACH, AT_UE_ATTACH_SUCC, 0, AT_DEFAULT_TIMEOUT))
    {
        rt_thread_mdelay(1000);
        count--;
    }

    /* 查询模块的 IP 地址 */
    //at_exec_cmd(resp, "AT+CGPADDR");
    bc28_get_ipaddr(&bc28);

    if (count > 0) 
    {
        bc28.stat = BC28_STAT_ATTACH;
        return RT_EOK;
    }
    else 
    {
        return -RT_ETIMEOUT;
    }
}

int bc28_client_deattach(void)
{
    check_send_cmd(AT_UE_DEATTACH, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    bc28.stat = BC28_STAT_DEATTACH;
}

/**
 * AT client initialize.
 *
 * @return 0 : initialize success
 *        -1 : initialize failed
 *        -5 : no memory
 */
static int at_client_dev_init(void)
{
    int result = RT_EOK;

    rt_device_t serial = rt_device_find(AT_CLIENT_DEV_NAME);
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    config.baud_rate = AT_CLIENT_BAUD_RATE;
    config.data_bits = DATA_BITS_8;
    config.stop_bits = STOP_BITS_1;
    config.bufsz     = AT_CLIENT_RECV_BUFF_LEN;
    config.parity    = PARITY_NONE;

    rt_device_control(serial, RT_DEVICE_CTRL_CONFIG, &config);
    rt_device_close(serial);

    /* initialize AT client */
    result = at_client_init(AT_CLIENT_DEV_NAME, AT_CLIENT_RECV_BUFF_LEN);
    if (result < 0)
    {
        LOG_E("at client (%s) init failed.", AT_CLIENT_DEV_NAME);
        return result;
    }

    bc28.client = at_client_get(AT_CLIENT_DEV_NAME);
    if (bc28.client == RT_NULL)
    {
        LOG_E("get AT client (%s) failed.", AT_CLIENT_DEV_NAME);
        return -RT_ERROR;
    }

    return RT_EOK;
}

static void bc28_reset(void)
{
    rt_pin_mode(BC28_RESET_N_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(BC28_RESET_N_PIN, PIN_HIGH);

    rt_thread_mdelay(300);

    rt_pin_write(BC28_RESET_N_PIN, PIN_LOW);

    rt_thread_mdelay(1000);
}

int at_client_port_init(void);

/**
 * BC28 device initialize.
 *
 * @return 0 : initialize success
 *        -1 : initialize failed
 */
int bc28_init(void)
{
    LOG_D("Init at client device.");
    at_client_dev_init();
    at_client_port_init();

    LOG_D("Reset BC28 device.");
    bc28_reset();

    bc28.stat = BC28_STAT_INIT;
    return RT_EOK;
}

void bc28_bind_parser(void (*callback)(const char *json))
{
    bc28.parser = callback;
}

int bc28_build_mqtt_network(void)
{
    int result = 0;

    bc28_mqtt_set_alive(KEEP_ALIVE_TIME);

    if((result = bc28_mqtt_auth()) < 0) {
        return result;
    }

    if((result = bc28_mqtt_open()) < 0) {
        return result;
    }

    if((result = bc28_mqtt_connect()) < 0) {
        return result;
    }

    return RT_EOK;
}

int bc28_rebuild_mqtt_network(void)
{
    int result = 0;

    bc28_mqtt_close();
    bc28_mqtt_set_alive(KEEP_ALIVE_TIME);

    if((result = bc28_mqtt_auth()) < 0) {
        return result;
    }

    if((result = bc28_mqtt_open()) < 0) {
        return result;
    }

    if((result = bc28_mqtt_connect()) < 0) {
        return result;
    }

    return RT_EOK;
}

static int bc28_deactivate_pdp(void)
{
    /* AT+CGACT=<state>,<cid> */

    return RT_EOK;
}

static void urc_mqtt_stat(struct at_client *client, const char *data, rt_size_t size)
{
    /* MQTT链路层的状态发生变化 */
    LOG_D("The state of the MQTT link layer changes");
    LOG_D("%s", data);

    int tcp_conn_id = 0, err_code = 0;
    sscanf(data, "+QMTSTAT: %d,%d", &tcp_conn_id, &err_code);

    switch (err_code)
    {
    /* connection closed by server */
    case AT_QMTSTAT_CLOSED:

    /* send CONNECT package timeout or failure */
    case AT_QMTSTAT_CONNECT_TIMEOUT:

    /* recv CONNECK package timeout or failure */
    case AT_QMTSTAT_CONNACK_TIMEOUT:

    /* send package failure and disconnect by client */
    case AT_QMTSTAT_WORNG_CLOSE:
        bc28_rebuild_mqtt_network();
        break;

    /* send PINGREQ package timeout or failure */
    case AT_QMTSTAT_PINGREQ_TIMEOUT:
        bc28_deactivate_pdp();
        bc28_rebuild_mqtt_network();
        break;

    /* disconnect by client */
    case AT_QMTSTAT_DISCONNECT:
        LOG_D("disconnect by client");
        break;

    /* network inactivated or server unavailable */
    case AT_QMTSTAT_INACTIVATED:
        LOG_D("please check network");
        break;
    default:
        break;
    }
}

static void urc_mqtt_recv(struct at_client *client, const char *data, rt_size_t size)
{
    /* 读取已从MQTT服务器接收的MQTT包数据 */
    LOG_D("AT client receive %d bytes data from server", size);
    LOG_D("%s", data);

    sscanf(data, "+QMTRECV: %*d,%*d,\"%*[^\"]\",%s", buf);
    bc28.parser(buf);
}

static const struct at_urc urc_table[] = {

    { "+QMTSTAT:", "\r\n", urc_mqtt_stat },
    { "+QMTRECV:", "\r\n", urc_mqtt_recv },
};

int at_client_port_init(void)
{
    /* 添加多种 URC 数据至 URC 列表中，当接收到同时匹配 URC 前缀和后缀的数据，执行 URC 函数  */
    at_set_urc_table(urc_table, sizeof(urc_table) / sizeof(urc_table[0]));

    return RT_EOK;
}

#ifdef FINSH_USING_MSH
MSH_CMD_EXPORT(bc28_mqtt_set_alive,   AT client MQTT auth);
MSH_CMD_EXPORT(bc28_mqtt_auth,        AT client MQTT auth);
MSH_CMD_EXPORT(bc28_mqtt_open,        AT client MQTT open);
MSH_CMD_EXPORT(bc28_mqtt_close,       AT client MQTT close);
MSH_CMD_EXPORT(bc28_mqtt_connect,     AT client MQTT connect);
MSH_CMD_EXPORT(bc28_mqtt_disconnect,  AT client MQTT disconnect);
MSH_CMD_EXPORT(bc28_mqtt_subscribe,   AT client MQTT subscribe);
MSH_CMD_EXPORT(bc28_mqtt_unsubscribe, AT client MQTT unsubscribe);
MSH_CMD_EXPORT(bc28_mqtt_publish,     AT client MQTT publish);

MSH_CMD_EXPORT(bc28_client_attach, AT client attach to access network);
MSH_CMD_EXPORT_ALIAS(at_client_dev_init, at_client_init, initialize AT client);
#endif
