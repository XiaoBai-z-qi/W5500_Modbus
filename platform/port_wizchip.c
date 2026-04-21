/*
 * wizchip_port.c
 *
 *  Created on: Oct 27, 2025
 *      Author: controllerstech
 */

#include "stm32f4xx.h"
#include "port_wizchip.h"
#include "stdio.h"
#include "string.h"
#include "socket.h"
#include "stdbool.h"
#include "DHCP/dhcp.h"
#include "DNS/dns.h"
#include "FreeRTOS.h"
#include "task.h"

#define USE_DHCP  0

static void port_cs_rst_gpioinit(void);
static void port_w5500_spiinit(void);
static uint8_t port_w5500_readbyte(void);
static void port_w5500_writebyte(uint8_t byte);


wiz_NetInfo netInfo = {
    .mac = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
    .ip = {192, 168, 137, 10},
    .sn = {255, 255, 255, 0},
    .gw = {192, 168, 137, 1},
    .dns = {8, 8, 8, 8},
#if USE_DHCP
	.dhcp = NETINFO_DHCP
#else
    .dhcp = NETINFO_STATIC
#endif
};

/*************************************************   NO Changes After This   ***************************************************************/

#define W5500_CS_LOW()     GPIO_ResetBits(GPIOA, GPIO_Pin_0)
#define W5500_CS_HIGH()    GPIO_SetBits(GPIOA, GPIO_Pin_0)
#define W5500_RST_LOW()    GPIO_ResetBits(GPIOA, GPIO_Pin_1)
#define W5500_RST_HIGH()   GPIO_SetBits(GPIOA, GPIO_Pin_1)


// SPI transmit/receive
void W5500_Select(void)   { W5500_CS_LOW(); }
void W5500_Unselect(void) { W5500_CS_HIGH(); }




#if USE_DHCP
volatile bool ip_assigned = false;
#define DHCP_SOCKET   7  // last available socket

uint8_t DHCP_buffer[548];


void Callback_IPAssigned(void) {
    ip_assigned = true;
}

void Callback_IPConflict(void) {
    ip_assigned = false;
}
#endif

#define DNS_SOCKET	  6  // 2nd last socket
uint8_t DNS_buffer[512];

int W5500_Init(void)
{
    port_cs_rst_gpioinit();
    port_w5500_spiinit();
    uint8_t memsize[2][8] = {{2,2,2,2,2,2,2,2},{2,2,2,2,2,2,2,2}};

    /***** Reset Sequence  *****/
    W5500_RST_LOW();
    vTaskDelay(50);
    W5500_RST_HIGH();
    vTaskDelay(200);

    /***** Register callbacks  *****/
    reg_wizchip_cs_cbfunc(W5500_Select, W5500_Unselect);
    reg_wizchip_spi_cbfunc(port_w5500_readbyte, port_w5500_writebyte);

    /***** Initialize the chip  *****/
    if (ctlwizchip(CW_INIT_WIZCHIP, (void*)memsize) == -1){
    	BSP_Printf("Error while initializing WIZCHIP\r\n");
    	return -1;
    }
    BSP_Printf("WIZCHIP Initialized\r\n");

    /***** check communication by reading Version  *****/
    uint8_t ver = getVERSIONR();
    if (ver != 0x04){
    	BSP_Printf("Error Communicating with W5500\t Version: 0x%02X\r\n", ver);
    	return -2;
    }
    BSP_Printf("Checking Link Status..\r\n");

 	/*****  CHeck Link Status  *****/
    uint8_t link = PHY_LINK_OFF;
    uint8_t retries = 10;
    while ((link != PHY_LINK_ON) && (retries > 0)){
        ctlwizchip(CW_GET_PHYLINK, &link);
        if (link == PHY_LINK_ON) BSP_Printf("Link: UP\r\n");
        else BSP_Printf("Link: DOWN Retrying : %d\r\n", 10-retries);
        retries--;
        vTaskDelay(500);
    }
    if (link != PHY_LINK_ON){
    	BSP_Printf ("Link is Down,please reconnect and retry\nExiting Setup..\r\n");
    	return 3;
    }

    /***** Use DHCP or Static IP  *****/
#if USE_DHCP
    BSP_Printf ("Using DHCP.. Please Wait..\r\n");
    setSHAR(netInfo.mac);
    DHCP_init(DHCP_SOCKET, DHCP_buffer);

    reg_dhcp_cbfunc(Callback_IPAssigned, Callback_IPAssigned, Callback_IPConflict);

    retries = 20;
    while((!ip_assigned) && (retries > 0)) {
        DHCP_run();
        vTaskDelay(500);
        retries--;
    }
    if(!ip_assigned) {
    	// DHCP Failed, switch to static IP
    	BSP_Printf ("DHCP Failed, switching to static IP\r\n");
    	ctlnetwork(CN_SET_NETINFO, (void*)&netInfo);
    }
    else {
    	// if IP is allocated, read it
        getIPfromDHCP(netInfo.ip);
        getGWfromDHCP(netInfo.gw);
        getSNfromDHCP(netInfo.sn);
        getDNSfromDHCP(netInfo.dns);

        // Now apply them to the chip
        ctlnetwork(CN_SET_NETINFO, (void*)&netInfo);
        BSP_Printf("DHCP IP assigned successfully\r\n");
    }

#else
    // use static IP (Not DHCP)
    BSP_Printf ("Using Static IP..\r\n");
    ctlnetwork(CN_SET_NETINFO, (void*)&netInfo);
#endif

    /***** Configure DNS  *****/
    vTaskDelay(500);
    BSP_Printf("Configuring DNS..\r\n");
    DNS_init(DNS_SOCKET, DNS_buffer);

    /***** Print assigned IP on the console  *****/
    wiz_NetInfo tmpInfo;
    ctlnetwork(CN_GET_NETINFO, &tmpInfo);
    BSP_Printf("IP: %d.%d.%d.%d\r\n", tmpInfo.ip[0], tmpInfo.ip[1], tmpInfo.ip[2], tmpInfo.ip[3]);
    BSP_Printf("SUBNET: %d.%d.%d.%d\r\n", tmpInfo.sn[0], tmpInfo.sn[1], tmpInfo.sn[2], tmpInfo.sn[3]);
    BSP_Printf("GATEWAY: %d.%d.%d.%d\r\n", tmpInfo.gw[0], tmpInfo.gw[1], tmpInfo.gw[2], tmpInfo.gw[3]);
    BSP_Printf("DNS: %d.%d.%d.%d\r\n", tmpInfo.dns[0], tmpInfo.dns[1], tmpInfo.dns[2], tmpInfo.dns[3]);

    return 0;
}

static void port_cs_rst_gpioinit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    W5500_CS_HIGH();
    W5500_RST_HIGH();
}

static void port_w5500_spiinit(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_SPI1);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);


    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    SPI_Cmd(SPI1, ENABLE);
}

static uint8_t port_w5500_readbyte(void)
{
    const uint8_t tx = 0xff;
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_FLAG_TXE) != SET);
    SPI_I2S_SendData(SPI1, tx);
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_FLAG_RXNE) != SET);
    return SPI_I2S_ReceiveData(SPI1);
}

static void port_w5500_writebyte(uint8_t byte)
{
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_FLAG_TXE) != SET);
    SPI_I2S_SendData(SPI1, byte);
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_FLAG_RXNE) != SET);
    SPI_I2S_ReceiveData(SPI1);
}
