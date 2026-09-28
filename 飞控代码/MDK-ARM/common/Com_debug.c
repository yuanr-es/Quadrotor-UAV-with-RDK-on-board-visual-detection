#include "Com_debug.h"
#include <stdarg.h>

extern USBD_HandleTypeDef hUsbDeviceFS; // 引入全局 USB 句柄

void usb_printf(const char *format, ...)
{
    uint8_t buffer[384];
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
    
    // 【核心修复】：必须加上 hcdc == NULL 的判断！防止USB没连好时内存越界死机！
    if (hcdc == NULL || hcdc->TxState != 0) {
        return; 
    }

    va_list args;
    va_start(args, format);
    int len = vsnprintf((char*)buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len > 0)
    {
        if (len >= (int)sizeof(buffer))
        {
            len = sizeof(buffer) - 1;
        }
        CDC_Transmit_FS(buffer, len);
    }
}

