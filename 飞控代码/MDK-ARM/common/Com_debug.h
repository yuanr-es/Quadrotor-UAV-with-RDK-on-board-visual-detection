#ifndef __COM_DEBUG
#define __COM_DEBUG
#include <string.h>
//#include "usart.h"
#include "stdio.h"
#include "usbd_cdc_if.h"
#include "FreeRTOS.h"
#include "task.h"
//日志输出打印运行在cpu上非常占用资源，在后续飞机飞行时，关闭日志输出，减少cpu占用率
#define COM_DEBUG_ENABLE 1

#ifdef COM_DEBUG_ENABLE
#define __FILE_NAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define __FILE_NAME (strrchr(__FILE_NAME__, '/') ? strrchr(__FILE_NAME__, '/') + 1 : __FILE_NAME__)
void usb_printf(const char *format, ...);
//使用宏定义实现打印日志前先加文件名和行号
#define debug_printf(format, ...) usb_printf("[%s:%d] " format, __FILE_NAME, __LINE__, ##__VA_ARGS__)
#else
#define debug_printf(format, ...)

#endif
#endif
