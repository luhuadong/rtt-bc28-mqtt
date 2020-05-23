# rtt-bc28-mqtt
MQTT package based on Quectel BC28 AT model



## 1、介绍

bc28_mqtt 是基于移远 BC28 模块 AT 固件的 MQTT 软件包，使用 UART 与 MCU 通信，目前实现了与阿里云物联网平台的连接。

> 从手册来看，BC35-G、BC28 和 BC95 的 AT 指令是兼容的，因此本软件包应该同时支持这三类模组。



### 1.1 特性

- 接口简单易用
- 支持掉线重连
- 支持多实例
- 线程安全



### 1.2 目录结构

| 名称     | 说明       |
| -------- | ---------- |
| docs     | 文档目录   |
| examples | 例子目录   |
| inc      | 头文件目录 |
| src      | 源代码目录 |



### 1.3 许可证

bc28_mqtt 软件包遵循 LGPL-2.1 许可，详见 `LICENSE` 文件。



### 1.4 依赖

- RT-Thread 4.0+
- AT 组件



## 2、获取 bc28_mqtt 软件包

使用 bc28_mqtt 软件包需要在 RT-Thread 的包管理器中选择它，具体路径如下：

```
RT-Thread online packages --->
    internet of things --->
        [*] BC28 MQTT: connect to Aliyun with Quectel BC28 model --->
```

保存配置参数后，使用 `pkgs --update` 命令更新包到 BSP 中。



## 3、使用 bc28_mqtt 软件包

### 3.1 版本说明

| 版本   | 说明                                           |
| ------ | ---------------------------------------------- |
| latest | 目前支持 MQTT 连接阿里云物联网平台，将持续维护 |

目前处于公测阶段，建议开发者使用 latest 版本。



### 3.2 配置选项

目前有如下几个配置项：

| 配置项                | 数据类型 | 说明                                       |
| --------------------- | -------- | ------------------------------------------ |
| AT client device name | string   | AT Client 串口设备名称，如 uart3           |
| Select baud rate      | int      | 串口波特率，可选 4800、9600、115200        |
| Select operating band | int      | BC28 模块工作频段，可选 1、3、5、8、20、28 |
| Reset pin             | int      | BC28 复位引脚号                            |
| ADC pin               | int      | BC28 ADC 引脚号（暂时无效）                |
| Product Key           | string   | 阿里云三元组信息                           |
| Device Name           | string   | 阿里云三元组信息                           |
| Device Secret         | string   | 阿里云三元组信息                           |
| Keep-alive time       | int      | MQTT 保活时间                              |



## 4、API 说明

### 4.1 MQTT 功能接口

```c
int bc28_mqtt_auth(void);
int bc28_mqtt_open(void);
int bc28_mqtt_close(void);
int bc28_mqtt_connect(void);
int bc28_mqtt_disconnect(void);
int bc28_mqtt_subscribe(const char *topic);
int bc28_mqtt_unsubscribe(const char *topic);
int bc28_mqtt_publish(const char *topic, const char *msg);
```



### 4.2 网络附着和去附着

```c
int at_client_attach(void);
int at_client_deattach(void);
```



### 4.3 网络初始化接口

```c
int bc28_init(void);
int build_mqtt_network(void);
int rebuild_mqtt_network(void);
```



## 5、相关文档

见 docs 目录。



## 6、联系方式

- 维护：luhuadong@163.com
- 主页：<https://github.com/luhuadong/rtt-bc28-mqtt>

