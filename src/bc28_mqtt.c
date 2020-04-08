/*
 * Copyright (c) 2020, RudyLo <luhuadong@163.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-02-07     luhuadong    the first version
 */

#include <stdlib.h>
#include <string.h>

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <at.h>

#define LOG_TAG                   "at.bc28"
#define LOG_LVL                   LOG_LVL_DBG
#include <at_log.h>

#include "at_bc28.h"

#define BC28_ADC0_PIN             GET_PIN(C, 0)
#define BC28_POWER_EN_PIN         GET_PIN(A, 3)
#define BC28_RESET_N_PIN          GET_PIN(A, 5)

#define AT_CLIENT_DEV_NAME        "uart3"
#define AT_DEFAULT_TIMEOUT        5000

#define PRODUCT_KEY               "a1p8Pngb3oY"
#define DEVICE_NAME               "BC28"
#define DEVICE_SECRET             "miYe6iSBGKbYq71nhkd0cddVT2PSlPGs"

#define AT_OK                     "OK"
#define AT_ERROR                  "ERROR"

#define AT_TEST                   "AT"
#define AT_ECHO_OFF               "ATE0"
#define AT_QREGSWT_2              "AT+QREGSWT=2"
#define AT_AUTOCONNECT_DISABLE    "AT+NCONFIG=AUTOCONNECT,FALSE"
#define AT_REBOOT                 "AT+NRB"
#define AT_NBAND_B8               "AT+NBAND=8"
#define AT_FUN_ON                 "AT+CFUN=1"
#define AT_LED_ON                 "AT+QLEDMODE=1"
#define AT_EDRX_OFF               "AT+CEDRXS=0,5"
#define AT_PSM_OFF                "AT+CPSMS=0"
#define AT_RECV_AUTO              "AT+NSONMI=2"
#define AT_UE_ATTACH              "AT+CGATT=1"
#define AT_UE_DEATTACH            "AT+CGATT=0"
#define AT_QUERY_IMEI             "AT+CGSN=1"
#define AT_QUERY_IMSI             "AT+CIMI"
#define AT_QUERY_STATUS           "AT+NUESTATS"
#define AT_QUERY_REG              "AT+CEREG?"
#define AT_QUERY_IPADDR           "AT+CGPADDR"
#define AT_QUERY_ATTACH           "AT+CGATT?"
#define AT_UE_ATTACH_SUCC         "+CGATT:1"

#define AT_MQTT_AUTH              "AT+QMTCFG=\"aliauth\",0,\"%s\",\"%s\",\"%s\""
#define AT_MQTT_ALIVE             "AT+QMTCFG=\"keepalive\",0,%u"
#define AT_MQTT_OPEN              "AT+QMTOPEN=0,\"%s.iot-as-mqtt.cn-shanghai.aliyuncs.com\",1883"
#define AT_MQTT_OPEN_SUCC         "+QMTOPEN: 0,0"
#define AT_MQTT_CLOSE             "AT+QMTCLOSE=0"
//#define AT_MQTT_CONNECT           "AT+QMTCONN=0,\"%s\""
#define AT_MQTT_CONNECT           "AT+QMTCONN=0,\"867726037265602\""
#define AT_MQTT_CONNECT_SUCC      "+QMTCONN: 0,0,0"
#define AT_MQTT_DISCONNECT        "AT+QMTDISC=0"
#define AT_MQTT_SUB               "AT+QMTSUB=0,1,\"%s\",0"
#define AT_MQTT_SUB_SUCC          "+QMTSUB: 0,1,0,1"
#define AT_MQTT_UNSUB             "AT+QMTUNS=0,1, \"%s\""
#define AT_MQTT_PUB               "AT+QMTPUB=0,0,0,0,\"%s\""
#define AT_MQTT_PUB_SUCC          "+QMTPUB: 0,0,0"

#define AT_QMTSTAT_CLOSED             1
#define AT_QMTSTAT_PINGREQ_TIMEOUT    2
#define AT_QMTSTAT_CONNECT_TIMEOUT    3
#define AT_QMTSTAT_CONNACK_TIMEOUT    4
#define AT_QMTSTAT_DISCONNECT         5
#define AT_QMTSTAT_WORNG_CLOSE        6
#define AT_QMTSTAT_INACTIVATED        7

//#define AT_QMTRECV_DATA           "+QMTRECV: %d,%d,\"%s\",\"%s\""
#define AT_CLIENT_RECV_BUFF_LEN   256


/**
 * This function will send command and check the result.
 *
 * @param cmd       command to at client
 * @param resp_expr expected response expression
 * @param lines     response lines
 * @param timeout   waiting time
 *
 * @return match successful return RT_EOK, otherwise return error code
 */
static int check_send_cmd(const char* cmd, const char* resp_expr, 
                          const rt_size_t lines, const rt_int32_t timeout)
{
    at_response_t resp = RT_NULL;
    int result = 0;

    resp = at_create_resp(AT_CLIENT_RECV_BUFF_LEN, lines, rt_tick_from_millisecond(timeout));
    if (resp == RT_NULL)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    result = at_exec_cmd(resp, cmd);
    if (result != RT_EOK)
    {
        LOG_E("AT client send commands failed or return response error!");
        at_delete_resp(resp);
        return result;
    }

#if 0
    /* Print response line buffer */
    char *line_buffer = RT_NULL;

    for(rt_size_t line_num = 1; line_num <= resp->line_counts; line_num++)
    {
        if((line_buffer = at_resp_get_line(resp, line_num)) != RT_NULL)
            LOG_D("line %d buffer : %s", line_num, line_buffer);
        else
            LOG_D("Parse line buffer error!");
    }
#endif

    char resp_arg[AT_CMD_MAX_LEN] = { 0 };
    if (at_resp_parse_line_args_by_kw(resp, resp_expr, "%s", resp_arg) <= 0)
    {
        at_delete_resp(resp);
        LOG_E("# >_< Failed");
        return -RT_ERROR;
    }

    LOG_D("# ^_^ successed");
    at_delete_resp(resp);
    return RT_EOK;
}

static int bc28_mqtt_set_alive(rt_uint32_t keepalive_time)
{
    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_ALIVE, keepalive_time);

    return check_send_cmd(cmd, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

int bc28_mqtt_auth(void)
{
    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_AUTH, PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET);

    return check_send_cmd(cmd, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

int bc28_mqtt_open(void)
{
    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_OPEN, PRODUCT_KEY);

    return check_send_cmd(cmd, AT_MQTT_OPEN_SUCC, 4, 75000);
}

int bc28_mqtt_close(void)
{
    return check_send_cmd(AT_MQTT_CLOSE, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

int bc28_mqtt_connect(void)
{
    return check_send_cmd(AT_MQTT_CONNECT, AT_MQTT_CONNECT_SUCC, 4, 10000);
}

int bc28_mqtt_disconnect(void)
{
    return check_send_cmd(AT_MQTT_DISCONNECT, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

int bc28_mqtt_subscribe(const char *topic)
{
    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_SUB, topic);

    return check_send_cmd(cmd, AT_MQTT_SUB_SUCC, 4, AT_DEFAULT_TIMEOUT);
}

int bc28_mqtt_unsubscribe(const char *topic)
{
    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_UNSUB, topic);

    return check_send_cmd(cmd, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

int bc28_mqtt_publish(const char *topic, const char *msg)
{
    char cmd[AT_CMD_MAX_LEN] = {0};
    rt_sprintf(cmd, AT_MQTT_PUB, topic);

    check_send_cmd(cmd, ">", 3, AT_DEFAULT_TIMEOUT);
    LOG_D("go...");

    return check_send_cmd(msg, AT_MQTT_PUB_SUCC, 4, AT_DEFAULT_TIMEOUT);
}

/* For testing */
static int bc28_mqtt_upload(int argc, char **argv)
{
    if(argc != 6) {
        LOG_E("Usage: bc28_mqtt_upload <temp> <humi> <dust> <tvoc> <eco2>");
        return -RT_ERROR;
    }

    int result = 0;
    char json_pack[512] = {0};

    rt_sprintf(json_pack, JSON_DATA_PACK_TEST, argv[1], argv[2], argv[3], argv[4], argv[5]);

    result = bc28_mqtt_publish(MQTT_TOPIC_UPLOAD, json_pack);

    if(result == RT_EOK)
        LOG_D("Upload OK");
    else
        LOG_D("Upload Error");

    return result;
}

int at_client_attach(void)
{
    int result = 0;

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
        rt_thread_delay(1000);
    }

    /* 查询IMEI号 */
    result = check_send_cmd(AT_QUERY_IMEI, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 指定要搜索的频段 B8 */
    result = check_send_cmd(AT_NBAND_B8, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

    /* 打开模块的调试灯 */
    result = check_send_cmd(AT_LED_ON, AT_OK, 0, AT_DEFAULT_TIMEOUT);
    if (result != RT_EOK) return result;

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

    if (count > 0) 
        return RT_EOK;
    else 
        return -RT_ETIMEOUT;
}

int at_client_deattach(void)
{
    check_send_cmd(AT_UE_DEATTACH, AT_OK, 0, AT_DEFAULT_TIMEOUT);
}

/**
 * AT client initialize.
 *
 * @return 0 : initialize success
 *        -1 : initialize failed
 *        -5 : no memory
 */
int at_client_dev_init(void)
{
    rt_device_t serial = rt_device_find(AT_CLIENT_DEV_NAME);
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    config.baud_rate = BAUD_RATE_9600;
    config.data_bits = DATA_BITS_8;
    config.stop_bits = STOP_BITS_1;
    config.bufsz     = AT_CLIENT_RECV_BUFF_LEN;
    config.parity    = PARITY_NONE;

    rt_device_control(serial, RT_DEVICE_CTRL_CONFIG, &config);
    rt_device_close(serial);

    return at_client_init(AT_CLIENT_DEV_NAME, AT_CLIENT_RECV_BUFF_LEN);
}

static void bc28_reset(void)
{
    rt_pin_mode(BC28_RESET_N_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(BC28_RESET_N_PIN, PIN_HIGH);

    rt_thread_mdelay(300);

    rt_pin_write(BC28_RESET_N_PIN, PIN_LOW);
    //rt_thread_mdelay(300);
}

int at_client_port_init(void);

int bc28_init(void)
{
    bc28_reset();
    at_client_dev_init();
    at_client_port_init();
    
    return at_client_attach();
}

int build_mqtt_network(void)
{
    int result = 0;

    bc28_mqtt_set_alive(300);

    if((result = bc28_mqtt_auth()) < 0) {
        return result;
    }

    if((result = bc28_mqtt_open()) < 0) {
        return result;
    }

    if((result = bc28_mqtt_connect()) < 0) {
        return result;
    }
/*
    if((result = bc28_mqtt_subscribe(MQTT_TOPIC_HELLO)) < 0) {
        return result;
    }

    if((result = bc28_mqtt_auth()) < 0) {
        return result;
    }
*/
    return RT_EOK;
}

int rebuild_mqtt_network(void)
{
    int result = 0;

    bc28_mqtt_close();
    bc28_mqtt_set_alive(300);

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

static int deactivate_pdp(void)
{
    /* AT+CGACT=<state>,<cid> */

    return RT_EOK;
}

static void urc_mqtt_stat(struct at_client *client, const char *data, rt_size_t size)
{
    /* MQTT链路层的状态发生变化 */
    LOG_D("The state of the MQTT link layer changes");

    LOG_D(">> %s", data);
    //LOG_D(">> %c", data[size-1]);

    char err_code = data[size-1];
    switch (err_code)
    {
    /* connection closed by server */
    case '1':
    /* send CONNECT package timeout or failure */
    case '3':
    /* recv CONNECK package timeout or failure */
    case '4':
    /* send package failure and disconnect by client */
    case '6':
        rebuild_mqtt_network();
        break;
    /* send PINGREQ package timeout or failure */
    case '2':
        deactivate_pdp();
        rebuild_mqtt_network();
        break;
    /* disconnect by client */
    case '5':
        LOG_D("disconnect by client");
        break;
    /* network inactivated or server unavailable */
    case '7':
        LOG_D("please check network");
        break;
    default:
        break;
    }

}

static void urc_mqtt_recv(struct at_client *client, const char *data, rt_size_t size)
{
    /* 读取已从MQTT服务器接收的MQTT包数据 */
    LOG_D("AT client receive data from server");

    LOG_D(">> %s", data);
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
//INIT_APP_EXPORT(at_client_port_init);

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
MSH_CMD_EXPORT(bc28_mqtt_upload,      AT client MQTT upload);

MSH_CMD_EXPORT(at_client_attach, AT client attach to access network);
MSH_CMD_EXPORT_ALIAS(at_client_dev_init, at_client_init, initialize AT client);
#endif
