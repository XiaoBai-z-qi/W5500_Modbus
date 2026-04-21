/*
 * FreeModbus Libary: lwIP Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id$
 */

/* ----------------------- System includes ----------------------------------*/
#include <stdio.h>
#include "port_modbus.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

/* ----------------------- MBAP Header --------------------------------------*/

#define MB_TCP_UID          6
#define MB_TCP_LEN          4
#define MB_TCP_FUNC         7

/* ----------------------- Defines  -----------------------------------------*/
#define MB_TCP_DEFAULT_PORT 502 /* TCP listening port. */
#define MB_TCP_BUF_SIZE     ( 256 + 7 ) /* Must hold a complete Modbus TCP frame. */

/* ----------------------- Static variables ---------------------------------*/

SemaphoreHandle_t xW5500RxSemaphore;
static UCHAR    aucTCPBuf[MB_TCP_BUF_SIZE];
static USHORT   usTCPBufPos;



/* ----------------------- Begin implementation -----------------------------*/
BOOL
xMBTCPPortInit( USHORT usTCPPort )
{
    BOOL            bOkay = FALSE;
    USHORT          usPort;

    //使能Socket的接收中断
    setSn_IR(MB_TCP_SOCKET, 0xFF);              //先清除中断标志
    setSn_IMR(MB_TCP_SOCKET, (Sn_IR_RECV | Sn_IR_CON | Sn_IR_DISCON | Sn_IR_TIMEOUT));
    setSIMR(getSIMR() | (1 << MB_TCP_SOCKET));

    if( usTCPPort == 0 )
    {
        usPort = MB_TCP_DEFAULT_PORT;
    }
    else
    {
        usPort = ( USHORT ) usTCPPort;
    }

    // 侦听端口 Modbus-TCP 端口
    if (socket(MB_TCP_SOCKET, Sn_MR_TCP, usPort, 0) != SOCK_OK)
    {
        BSP_Printf("socket is false\r\n");
        return bOkay;
    }
    BSP_Printf("socket is true\r\n");
    if (listen(MB_TCP_SOCKET) != SOCK_OK)
    {
        close(MB_TCP_SOCKET);
        return bOkay;
    }
    bOkay = TRUE;
    return bOkay;
}


void
vMBTCPPortClose(  )
{

}

void
vMBTCPPortDisable( void )
{

}

BOOL
xMBTCPPortGetRequest( UCHAR ** ppucMBTCPFrame, USHORT * usTCPLength )
{
    *ppucMBTCPFrame = &aucTCPBuf[0];
    *usTCPLength = usTCPBufPos;

    /* Reset the buffer. */
    usTCPBufPos = 0;
    return TRUE;
}

BOOL
xMBTCPPortSendResponse( const UCHAR * pucMBTCPFrame, USHORT usTCPLength )
{
    BOOL            bOkay = FALSE;
    int32_t iSendLen = 0;           // 实际发送的字节数
    uint8_t sockStatus = 0;         // 当前Socket状态

    sockStatus = getSn_SR(MB_TCP_SOCKET);

    if (sockStatus == SOCK_ESTABLISHED)
    {
        iSendLen = send(MB_TCP_SOCKET, (uint8_t *)pucMBTCPFrame, usTCPLength);

        if (iSendLen == usTCPLength)
            bOkay = TRUE;
        else
            bOkay = FALSE;
    }
    else
    {
        bOkay = FALSE;
    }
    return bOkay;
}
uint8_t ir_status = 0;
void vMBTCPPortIR(void)
{
    ir_status = getSIR();
    if (ir_status & (1 << MB_TCP_SOCKET))
    {
        uint8_t sn_ir_val = getSn_IR(MB_TCP_SOCKET);

        if (sn_ir_val & Sn_IR_RECV)
        {
            uint16_t rx_len = getSn_RX_RSR(MB_TCP_SOCKET);
            if (rx_len > 0)
            {
                rx_len = recv(MB_TCP_SOCKET, aucTCPBuf, rx_len);
                if (rx_len > 0)
                {
                    usTCPBufPos = rx_len;
                    xMBPortEventPost(EV_FRAME_RECEIVED);
                }
            }
            setSn_IR(MB_TCP_SOCKET, Sn_IR_RECV);
        }
        if(sn_ir_val & Sn_IR_CON)
        {
            // 连接已建立，可进行相应处理
            setSn_IR(MB_TCP_SOCKET, Sn_IR_CON);
        }
        if(sn_ir_val & Sn_IR_DISCON)
        {
            // 对端断开，重新进入监听模式
            close(MB_TCP_SOCKET);
            socket(MB_TCP_SOCKET, Sn_MR_TCP, MB_TCP_DEFAULT_PORT, 0);
            listen(MB_TCP_SOCKET);
            setSn_IMR(MB_TCP_SOCKET, (Sn_IR_RECV | Sn_IR_CON | Sn_IR_DISCON | Sn_IR_TIMEOUT));
            setSIMR(getSIMR() | (1 << MB_TCP_SOCKET));
            setSn_IR(MB_TCP_SOCKET, Sn_IR_DISCON);
        }
        if(sn_ir_val & Sn_IR_TIMEOUT)
        {
            // TCP通信超时，可执行重新连接等操作
            setSn_IR(MB_TCP_SOCKET, Sn_IR_TIMEOUT);
        }
    }
}

void port_w5500_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line2) != RESET)
    {
        // 清除中断标志位
        EXTI_ClearITPendingBit(EXTI_Line2);
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xW5500RxSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

