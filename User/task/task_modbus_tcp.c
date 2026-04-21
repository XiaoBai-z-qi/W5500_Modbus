#include "task_modbus_tcp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "port_wizchip.h"
#include "mb.h"
#include "w5500.h"
#include "socket.h"
/* ----------------------- Holding register Defines ------------------------------------------*/

#define REG_HOLDING_START 1000
#define REG_HOLDING_NREGS 4

/* ----------------------- Static variables ---------------------------------*/
static unsigned short usRegHoldingStart = REG_HOLDING_START;
static unsigned short usRegHoldingBuf[REG_HOLDING_NREGS]={1,2,3,4};

extern SemaphoreHandle_t xW5500RxSemaphore;
void Modbus_TCP_Task(void *pvParameters)
{
    uint8_t sockStatus;
    xW5500RxSemaphore = xSemaphoreCreateBinary();
    W5500_Init();
    vTaskDelay(200);
    eMBTCPInit(MB_TCP_PORT_USE_DEFAULT);
    vTaskDelay(200);
    eMBEnable();
    while (1)
    {
        if (xSemaphoreTake(xW5500RxSemaphore, pdTICKS_TO_MS(10)) == pdTRUE)
        {
            vMBTCPPortIR();
        }
        sockStatus = getSn_SR(MB_TCP_SOCKET);
        if (sockStatus == SOCK_ESTABLISHED)
            eMBPoll();
    }
}

eMBErrorCode
eMBRegHoldingCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs,
                 eMBRegisterMode eMode )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    int             iRegIndex;

    if( ( usAddress >= REG_HOLDING_START ) &&
        ( usAddress + usNRegs <= REG_HOLDING_START + REG_HOLDING_NREGS ) )
    {
        iRegIndex = ( int )( usAddress - usRegHoldingStart );
        switch ( eMode )
        {
            /* Pass current register values to the protocol stack. */
        case MB_REG_READ:
            while( usNRegs > 0 )
            {
                *pucRegBuffer++ =
                    ( unsigned char )( usRegHoldingBuf[iRegIndex] >> 8 );
                *pucRegBuffer++ =
                    ( unsigned char )( usRegHoldingBuf[iRegIndex] &
                                       0xFF );
                iRegIndex++;
                usNRegs--;
            }
            break;

            /* Update current register values with new values from the
             * protocol stack. */
        case MB_REG_WRITE:
            while( usNRegs > 0 )
            {
                usRegHoldingBuf[iRegIndex] = *pucRegBuffer++ << 8;
                usRegHoldingBuf[iRegIndex] |= *pucRegBuffer++;
                iRegIndex++;
                usNRegs--;
            }
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    return eStatus;
}

eMBErrorCode
eMBRegInputCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs )
{
    eMBErrorCode    eStatus = MB_ENOERR;

    return eStatus;
}



eMBErrorCode
eMBRegCoilsCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNCoils,
               eMBRegisterMode eMode )
{
    eMBErrorCode    eStatus = MB_ENOERR;

    return eStatus;
}

eMBErrorCode
eMBRegDiscreteCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNDiscrete )
{
    eMBErrorCode    eStatus = MB_ENOERR;

    return eStatus;
}