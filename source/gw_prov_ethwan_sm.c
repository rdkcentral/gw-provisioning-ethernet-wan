/* 
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
/*#######################################################################
#   Copyright [2015] [ARRIS Corporation]
#
#   Licensed under the Apache License, Version 2.0 (the \"License\");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an \"AS IS\" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#######################################################################*/

#ifndef _GW_PROV_ETHWAN_SM_C_
#define _GW_PROV_ETHWAN_SM_C_

/*! \file gw_prov_ethwan_sm.c
    \brief gw epon provisioning
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#if !defined(_PLATFORM_IPQ) && !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
#include <sys/types.h>
#endif
#include <unistd.h>
#include <sysevent/sysevent.h>
#include <syscfg/syscfg.h>
#include <pthread.h>
#include "stdbool.h"
#include "gw_prov_ethwan.h"
#ifdef AUTOWAN_ENABLE
#include "autowan.h"
#endif
#include "ansc_wrapper_base.h"
#include "ccsp_hal_ethsw.h"
#include "platform_hal.h"
#ifdef FEATURE_SUPPORT_RDKLOG
#include "rdk_debug.h"
#endif

#ifndef UNIT_TEST_DOCKER_SUPPORT
#define STATIC static
#else
#define STATIC
#endif

/* Global Variables*/
//char log_buff[1024];

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static int sysevent_fd;
static token_t sysevent_token;
#ifdef AUTOWAN_ENABLE
int sysevent_fd_gs;
token_t sysevent_token_gs;
#else
static int sysevent_fd_gs;
static token_t sysevent_token_gs;
#endif
static pthread_t sysevent_tid;
#if defined(_PLATFORM_IPQ_) || defined(_PLATFORM_RASPBERRYPI_) || defined(_PLATFORM_TURRIS_)
static pthread_t linkstate_tid;
#endif
#if defined(_PLATFORM_IPQ_)
static uint32_t cb_registration_cnt;
#endif
#define INFO  0
#define WARNING  1
#define ERROR 2

#define DEBUG_INI_NAME  "/etc/debug.ini"
#define COMP_NAME "LOG.RDK.GWPROVETHWAN"
#define LOG_INFO 4
#define ER_NETDEVNAME "erouter0"
#define NETUTILS_IPv6_GLOBAL_ADDR_LEN             128

/* ETH WAN Fallback Interface Name - Should eventually move away from Compile Time */
#if defined (_XB7_PRODUCT_REQ_) && defined (_COSA_BCM_ARM_)
#define ETHWAN_DEF_INTF_NAME "eth3"
#elif defined (INTEL_PUMA7)
#define ETHWAN_DEF_INTF_NAME "nsgmii0"
#elif defined(_SCER11BEL_PRODUCT_REQ_)
#define ETHWAN_DEF_INTF_NAME "eth4"
#elif defined(_PLATFORM_TURRIS_)
#define ETHWAN_DEF_INTF_NAME "eth2"
#elif defined(_CBR2_PRODUCT_REQ_)
#define ETHWAN_DEF_INTF_NAME "eth5"
#else
#define ETHWAN_DEF_INTF_NAME "eth0"
#endif


/* Syscfg keys used for calculating mac addresses of local interfaces and bridges */
#define BASE_MAC_SYSCFG_KEY                  "base_mac_address"
/* Offset at which LAN bridge mac addresses will start */
#define BASE_MAC_BRIDGE_OFFSET_SYSCFG_KEY    "base_mac_bridge_offset"
#define BASE_MAC_BRIDGE_OFFSET               0
/* Offset at which wired LAN mac addresses will start */
#define BASE_MAC_LAN_OFFSET_SYSCFG_KEY       "base_mac_lan_offset"
#define BASE_MAC_LAN_OFFSET                  129
/* Offset at which WiFi AP mac addresses will start */
#define BASE_MAC_WLAN_OFFSET_SYSCFG_KEY      "base_mac_wlan_offset"
#define BASE_MAC_WLAN_OFFSET                 145

#define BRG_INST_SIZE 5
#define BUF_SIZE 256
#if !defined(AUTOWAN_ENABLE)
#ifdef FEATURE_SUPPORT_RDKLOG
#define GWPROVETHWANLOG(fmt ...)    {\
                                    char log_buff[1024];\
				    				snprintf(log_buff, 1023, fmt);\
                                    RDK_LOG(LOG_INFO, COMP_NAME, "%s", log_buff);\
                                    fflush(stdout);\
                                 }
#else
#define GWPROVETHWANLOG printf
#endif
#endif


//Note: By Moving this headerfile inclusion to "INCLUDES:" block, may run into build issues
#include "mso_mgmt_hal.h"

#if 0
#define IF_WANBRIDGE "erouter0"
#endif

#define _DEBUG 1
#define THREAD_NAME_LEN 16 //length is restricted to 16 characters, including the terminating null byte

/* Maximum callback registrations supported is 32 */
#define CB_REG_CNT_MAX 32

/* For LED behavior */
#define WHITE 0
#define YELLOW 1
#define SOLID   0
#define BLINK   1
#define RED	3

#if defined(INTEL_PUMA7) || defined(_INTEL_BUG_FIXES_)
void getNetworkDeviceMacAddress(macaddr_t* macAddr);
#endif

static int pnm_inited = 0;
static int netids_inited = 0;
static int webui_started = 0;
static int once = 0;

STATIC void check_lan_wan_ready();
STATIC void LAN_start();
static int hotspot_started = 0;
#if defined(_PLATFORM_IPQ_)
static appCallBack *obj_l[CB_REG_CNT_MAX];
#endif
unsigned char ethwan_ifname[ 64 ];
#if !defined(_PLATFORM_IPQ_) && (!defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_))
#if !defined(FEATURE_RDKB_WAN_MANAGER)
STATIC unsigned char tftpserverStarted = FALSE;
#endif
#endif
#if !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
void SME_CreateEventHandler(appCallBack *pAppCallBack);
int
GwProvSetLED
    (
        int color,
        int state,
        int interval
    )
{
    LEDMGMT_PARAMS ledMgmt;
    memset(&ledMgmt, 0, sizeof(LEDMGMT_PARAMS));

        ledMgmt.LedColor = color;
        ledMgmt.State    = state;
        ledMgmt.Interval = interval;
#if defined(_XB6_PRODUCT_REQ_) || defined(_CBR2_PRODUCT_REQ_)
        if(RETURN_ERR == platform_hal_setLed(&ledMgmt)) {
                GWPROVETHWANLOG("platform_hal_setLed failed\n");
                return 1;
        }
#endif
    return 0;
}
#endif



#define BRMODE_ROUTER 0
#define BRMODE_PRIMARY_BRIDGE   3
#define BRMODE_GLOBAL_BRIDGE 2

static int bridge_mode = BRMODE_ROUTER;
static int active_mode = BRMODE_ROUTER;

STATIC void GWPEthWan_EnterBridgeMode(void);
STATIC void GWPEthWan_EnterRouterMode(void);

static int bridgeModeInBootup = 0;

void 
validate_mode(int* bridge_mode)
{
    if((*bridge_mode != BRMODE_ROUTER) && (*bridge_mode != BRMODE_PRIMARY_BRIDGE) && (*bridge_mode != BRMODE_GLOBAL_BRIDGE))
    {
        GWPROVETHWANLOG(" SYSDB_CORRUPTION: bridge_mode = %d \n", *bridge_mode);
        GWPROVETHWANLOG(" SYSDB_CORRUPTION: Switching to Default Router Mode \n");
        *bridge_mode = BRMODE_ROUTER;

        if (syscfg_set_u_commit(NULL, "bridge_mode", BRMODE_ROUTER) != 0) {
            GWPROVETHWANLOG(" %s: syscfg_set brideg_mode failed\n", __FUNCTION__);
        }
    }
    GWPROVETHWANLOG(" %s : bridge_mode = %d\n", __FUNCTION__, *bridge_mode);
 }

STATIC int getSyseventBridgeMode(int bridgeMode) {
        
    //Erouter mode takes precedence over bridge mode. If erouter is disabled, 
    //global bridge mode is returned. Otherwise partial bridge or router  mode
    //is returned based on bridge mode. Partial bridge keeps the wan active
    //for networks other than the primary.
    // router = 0
    // global bridge = 2
    // partial (pseudo) = 3

    /*
     * Router/Bridge settings from utopia
        typedef enum {
            BRIDGE_MODE_OFF    = 0,
            BRIDGE_MODE_DHCP   = 1,
            BRIDGE_MODE_STATIC = 2,
            BRIDGE_MODE_FULL_STATIC = 3
           
        } bridgeMode_t;
     */ 
     
        switch( bridgeMode )
        {
            case 2:
            {
                return BRMODE_GLOBAL_BRIDGE;
            }
        
            case 3:
            {
                return BRMODE_PRIMARY_BRIDGE;
            }
        
            default: /* 0 */
            {
                return BRMODE_ROUTER;
            }
        }
        return BRMODE_ROUTER;
}
/**************************************************************************/
/*! \fn int STATUS GWPEthWan_SyseventGetStr
 **************************************************************************
 *  \brief Set sysevent string Value
 *  \return 0:success, <0: failure
 **************************************************************************/

int GWPEthWan_SyseventGetStr(const char *name, unsigned char *out_value, int outbufsz)
{
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(out_value);
    UNREFERENCED_PARAMETER(outbufsz);
  //Need to Implement

   return 0;		
}

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*! \fn static STATUS GWPETHWAN_SysCfgGetInt
 **************************************************************************
 *  \brief Get Syscfg Integer Value
 *  \return int/-1
 **************************************************************************/
STATIC int GWPEthWan_SysCfgGetInt(const char *name)
{
   char out_value[20];
   int outbufsz = sizeof(out_value);
        printf(" %s : name = %s \n", __FUNCTION__, name);
   if (!syscfg_get(NULL, name, out_value, outbufsz))
   {
        printf(" value = %s \n", out_value);
      return atoi(out_value);
   }
   else
   {
        printf(" syscfg get failed \n");
      return -1;
   }
}

#if defined(_PLATFORM_IPQ_)
/**************************************************************************/
/*! \fn int STATUS GWP_GetEthWanLinkStatus
 **************************************************************************
 *  \brief Get Wan Link Status
 *  \return 0:Link down, 1: Link up, <0: failure, errno. 
 **************************************************************************/
int GWP_GetEthWanLinkStatus()
{
	char buf[8] = {0};

	GWPROVETHWANLOG(" Entry %s \n", __FUNCTION__);
	GWPROVETHWANLOG("Info: Entry %s \n", __FUNCTION__);

	GWPROVETHWANLOG("\n**************************\n");
	GWPROVETHWANLOG("\nGWP_GetEthWanLinkStatus\n");
	GWPROVETHWANLOG("\n**************************\n\n");
	
	return 0;	

	sysevent_get(sysevent_fd_gs, sysevent_token_gs, "phylink_wan_state", buf, sizeof(buf));

	if (!strcmp(buf, "up")) {
		return 1;
	}
	else if (!strcmp(buf, "down")) {
		return 0;
	}

	/* We should not reach here */
	GWPROVETHWANLOG("Error: Invalid value for event \'phylink_wan_state\' %s \n", __FUNCTION__);
	return -EINVAL;
}
#endif
#if !defined(FEATURE_RDKB_WAN_MANAGER)
STATIC int GWP_EthWanLinkDown_callback()
{
	GWPROVETHWANLOG(" Entry %s \n", __FUNCTION__);
        system("sysevent set phylink_wan_state down");
	GWPROVETHWANLOG("\n**************************\n");
	GWPROVETHWANLOG("\n GWP_EthWanLinkDown_callback \n");
	GWPROVETHWANLOG("\n**************************\n\n");
	GWPROVETHWANLOG(" Stopping wan service\n");
#if !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
        #if defined (_CBR2_PRODUCT_REQ_)
        GWPROVETHWANLOG("Stopping wan service, Setting LED to WHITE FAST BLINK\n");
	GwProvSetLED(WHITE, BLINK, 5) ;
        #else
	GwProvSetLED(YELLOW, BLINK, 1);
        #endif
#endif
        system("sysevent set wan-stop");
	return 0;
}
#endif
#if !defined(_PLATFORM_IPQ_) && !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
#if !defined(FEATURE_RDKB_WAN_MANAGER)
STATIC int ethGetPHYRate
    (
        CCSP_HAL_ETHSW_PORT PortId
    )
{
    INT status                              = RETURN_ERR;
    CCSP_HAL_ETHSW_LINK_RATE LinkRate       = CCSP_HAL_ETHSW_LINK_NULL;
    CCSP_HAL_ETHSW_DUPLEX_MODE DuplexMode   = CCSP_HAL_ETHSW_DUPLEX_Auto;
    INT PHYRate                             = 0;

    status = CcspHalEthSwGetPortCfg(PortId, &LinkRate, &DuplexMode);
    if (RETURN_OK == status)
    {
     	GWPROVETHWANLOG(" Entry %s \n", __FUNCTION__);
        switch (LinkRate)
        {
            case CCSP_HAL_ETHSW_LINK_10Mbps:
            {
                PHYRate = 10;
                break;
            }
            case CCSP_HAL_ETHSW_LINK_100Mbps:
            {
                PHYRate = 100;
                break;
            }
            case CCSP_HAL_ETHSW_LINK_1Gbps:
            {
                PHYRate = 1000;
                break;
            }
#ifdef _2_5G_ETHERNET_SUPPORT_
            case CCSP_HAL_ETHSW_LINK_2_5Gbps:
            {
                PHYRate = 2500;
                break;
            }
            case CCSP_HAL_ETHSW_LINK_5Gbps:
            {
                PHYRate = 5000;
                break;
            }
#endif // _2_5G_ETHERNET_SUPPORT_
            case CCSP_HAL_ETHSW_LINK_10Gbps:
            {
                PHYRate = 10000;
                break;
            }
            case CCSP_HAL_ETHSW_LINK_Auto:
            {
	        GWPROVETHWANLOG(" Entry %s LINK_Auto \n", __FUNCTION__);
                PHYRate = 1000;
                break;
            }
            default:
            {
                PHYRate = 0;
                break;
            }
        }
    }
    return PHYRate;
}
#endif
#endif
#if !defined(FEATURE_RDKB_WAN_MANAGER)
STATIC int GWP_EthWanLinkUp_callback()
{
	GWPROVETHWANLOG(" Entry %s \n", __FUNCTION__);
	system("sysevent set phylink_wan_state up");
	GWPROVETHWANLOG("\n**************************\n");
	GWPROVETHWANLOG("\nGWP_EthWanLinkUp_callback\n");
	GWPROVETHWANLOG("\n**************************\n\n");

#if 0    
#if !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
	system("sysevent set bridge_mode 0"); // to boot in router mode
#endif
#endif
        char wanPhyName[20];
        char out_value[20];
        int outbufsz = sizeof(out_value);
//        char redirFlag[10]={0};
//        char captivePortalEnable[10]={0};

//#if !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
#if 0    
        if (!syscfg_get(NULL, "redirection_flag", redirFlag, sizeof(redirFlag)) && !syscfg_get(NULL, "CaptivePortal_Enable", captivePortalEnable, sizeof(captivePortalEnable))){
          if (!strcmp(redirFlag,"true") && !strcmp(captivePortalEnable,"true"))
	    GwProvSetLED(WHITE, BLINK, 1);
//Cox: Cp is disabled and need to show solid white
         if(!strcmp(redirFlag,"true") && !strcmp(captivePortalEnable,"false"))
         {
                #if !defined (_CBR2_PRODUCT_REQ_)
		GWPROVETHWANLOG("%s: CP disabled case and set led to solid white \n", __FUNCTION__);
		GwProvSetLED(WHITE, SOLID, 1);
                #endif 
         }
        }
#endif
        if (!syscfg_get(NULL, "wan_physical_ifname", out_value, outbufsz))
        {
           strcpy(wanPhyName, out_value);
           printf("wanPhyName = %s\n", wanPhyName);
        }
        else
        {
           return -1;
        }

        printf("Starting wan service\n");
        GWPROVETHWANLOG(" Starting wan service\n");
#if defined(_PLATFORM_RASPBERRYPI_) || defined(_PLATFORM_TURRIS_)
	system("sysevent set wan-start;sysevent set sshd-restart");
        sleep(50);
        system("sysevent set current_ipv4_link_state up");
        system("sysevent set ipv4_wan_ipaddr `ifconfig erouter0 | grep \"inet addr\" | cut -d':' -f2 | awk '{print$1}'`");
        system("sysevent set ipv4_wan_subnet `ifconfig erouter0 | grep \"inet addr\" | cut -d':' -f4 | awk '{print$1}'`");
        system("sysevent set wan_service-status started");
        system("sysevent set bridge_mode `syscfg get bridge_mode`");
#else
        system("sysevent set wan-start");
#endif
	    system("sysevent set ethwan-initialized 1");
		system("syscfg set eth_wan_enabled true"); // to handle Factory reset case
		system("syscfg set last_wan_mode 1"); // to handle Factory reset case
		system("syscfg set curr_wan_mode 1"); // to handle Factory reset case
		system("syscfg set ntp_enabled 1"); // Enable NTP in case of ETHWAN
		system("syscfg commit");
#if !defined(_PLATFORM_IPQ_) && !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
        unsigned int ethWanPort = 0xffffffff;

        if (RETURN_OK == CcspHalExtSw_getEthWanPort(&ethWanPort) )
        {
        	ethWanPort += CCSP_HAL_ETHSW_EthPort1; /* ethWanPort starts from 0*/
        	GWPROVETHWANLOG("WAN_MODE: Ethernet %d\n", ethGetPHYRate(ethWanPort));
        }
        else
        {
        	GWPROVETHWANLOG("WAN_MODE: Ethernet WAN Port Couldn't Be Retrieved\n");
        }
#endif
        return 0;
}
#endif
#if defined(_PLATFORM_IPQ_)
/**************************************************************************/
/*! \fn void *GWP_linkstate_threadfunc(void *)
 **************************************************************************
 *  \brief Thread function to check the link state
 *  \return
 **************************************************************************/
static void *GWPEthWan_linkstate_threadfunc(void *data)
{
    char *temp;
    char command[50] = {0};
    char wanPhyName[20] = {0};
    char out_value[20] = {0};
    int outbufsz = sizeof(out_value);
    int is_mode_switch = 0;
    int is_wan_restart = 0;
    char buf[8] = {0};
    int i;

    char* buff = NULL;
    buff = malloc(sizeof(char)*50);
    if(buff == NULL)
    {
        return -1;
    }
    char previousLinkStatus[10] = "down";
    if (!syscfg_get(NULL, "wan_physical_ifname", out_value, outbufsz))
    {
        strcpy(wanPhyName, out_value);
        printf("wanPhyName = %s\n", wanPhyName);
    }
    else
    {
        if(buff != NULL)
            free(buff);
        return -1;
    }
    snprintf(command, sizeof(command), "cat /sys/class/net/%s/operstate", wanPhyName);

    while(1)
    {
        FILE *fp;
        memset(buff,0,sizeof(buff));

       /*
        * utopia_restart values :
        *      0 : mode switch is not in progress
        *      1 : mode switch is in progress
        */
       sysevent_get(sysevent_fd_gs, sysevent_token_gs, "utopia_restart", buf, sizeof(buf));
       is_mode_switch = atoi(buf);
       sysevent_get(sysevent_fd_gs, sysevent_token_gs, "wan-restarting", buf, sizeof(buf));
       is_wan_restart = atoi(buf);

       if ( !( is_mode_switch || is_wan_restart) )
       {
        /* Open the command for reading. */
        fp = popen(command, "r");
        if (fp == NULL)
        {
            printf("<%s>:<%d> Error popen\n", __FUNCTION__, __LINE__);
            continue;
        }

        /* Read the output a line at a time - output it. */
        while (fgets(buff, 50, fp) != NULL)
        {
            /*printf("Ethernet status :%s", buff);*/
            temp = strchr(buff, '\n');
            if(temp)
                *temp = '\0';
        }

        /* close */
        pclose(fp);
        if(!strcmp(buff, (const char *)previousLinkStatus))
        {
            /*printf("Link status not changed\n");*/
        }
        else
        {
            if(!strcmp(buff, "up"))
            {
                /*printf("Ethernet status :%s\n", buff);*/
                GWP_EthWanLinkUp_callback();
#if defined(_PLATFORM_IPQ_)
		/* We call all the registered callbacks */
		for (i = 0; i < cb_registration_cnt; i++) {
			if (obj_l[i]->pGWP_act_EthWanLinkUP)
				obj_l[i]->pGWP_act_EthWanLinkUP();
		}
#endif

            }
            else if(!strcmp(buff, "down"))
            {
                /*printf("Ethernet status :%s\n", buff);*/
                GWP_EthWanLinkDown_callback();
#if defined(_PLATFORM_IPQ_)
		/* We call all the registered callbacks */
		for (i = 0; i < cb_registration_cnt; i++) {
			if (obj_l[i]->pGWP_act_EthWanLinkDown)
				obj_l[i]->pGWP_act_EthWanLinkDown();
		}
#endif

            }
            else
            {
                sleep(5);
                continue;
            }
            memset(previousLinkStatus,0,sizeof(previousLinkStatus));
            strcpy((char *)previousLinkStatus, buff);
            /*printf("Previous Ethernet status :%s\n", (char *)previousLinkStatus);*/
        }
       }
        sleep(5);
    }
    if(buff != NULL)
        free(buff);

    return 0;
}
#elif defined(_PLATFORM_RASPBERRYPI_) || defined(_PLATFORM_TURRIS_)
static void *GWPEthWan_linkstate_threadfunc(void *data)
{
        char *temp;
    char command[100] = {0};
    char wanPhyName[20] = {0};
    char out_value[20] = {0};
    int outbufsz = sizeof(out_value);

    char* buff = NULL;
    buff = malloc(sizeof(char)*50);
    if(buff == NULL)
    {
        return (void *) -1;
    }
    char previousLinkStatus[10] = "down";
    char previousLinkDetected[10] = "no";
    if (!syscfg_get(NULL, "wan_physical_ifname", out_value, outbufsz))
    {
        strcpy(wanPhyName, out_value);
        printf("wanPhyName = %s\n", wanPhyName);
    }
    else
    {
        if(buff != NULL)
            free(buff);
        return (void *) -1;
    }
    //sprintf(command, "cat /sys/class/net/%s/operstate", wanPhyName);
    snprintf(command, sizeof(command), "ethtool %s | grep \"Link detected\" | cut -d ':' -f2 | cut -d ' ' -f2", wanPhyName);

    while(1)
    {
        FILE *fp;
        memset(buff,0,sizeof(buff));

        /* Open the command for reading. */
        fp = popen(command, "r");
        if (fp == NULL)
        {
            printf("<%s>:<%d> Error popen\n", __FUNCTION__, __LINE__);
	    continue;
        }
         /* Read the output a line at a time - output it. */
        while (fgets(buff, 50, fp) != NULL)
        {
            /*printf("Ethernet status :%s", buff);*/
            temp = strchr(buff, '\n');
            if(temp)
                *temp = '\0';
        }

        /* close */
        pclose(fp);
        if(!strcmp(buff, (const char *)previousLinkDetected))
        {
            printf("Link status not changed\n");
        }
        else
        {
            if(!strcmp(buff, "yes"))
            {
                GWP_EthWanLinkUp_callback();
            }
            else if(!strcmp(buff, "no"))
            {
                GWP_EthWanLinkDown_callback();
            }
            else
            {
                sleep(5);
                continue;
            }
            memset(previousLinkStatus,0,sizeof(previousLinkStatus));
	    if(strcmp(buff,"yes") == 0)
	            strcpy((char *)previousLinkStatus, "up");
	    else	
	            strcpy((char *)previousLinkStatus, "down");
            memset(previousLinkDetected,0,sizeof(previousLinkDetected));
            strcpy((char *)previousLinkDetected, buff);
	    printf("Previous Ethernet status :%s\n", (char *)previousLinkStatus);
        }
        sleep(5);
    }
if(buff != NULL)
        free(buff);

    return 0;
}
#endif

/** 
*@brief  Create state machine for event handling from DOCSIS stack 
*\n Prototype :
		int GWP_RegisterEthWan_Callback
	(
		appCallBack *pAppCallBack
	)
*\n Caller : int main(int argc, char *argv[])
*
*
* @param[in] appCallBack *pAppCallBack.
* @param[out] None.
* @retval 0: Success, <0: Failure,errno.
* 	     ENOTSUP: Registration count exceeds 32
* 	     EINVAL: NULL Function Pointers passed
*/
#if defined(_PLATFORM_IPQ_)
int GWP_RegisterEthWan_Callback(appCallBack *pAppCallBack)
 {
	GWPROVETHWANLOG("Info: Entering into %s\n",__FUNCTION__);

	/* We support maximum 32 callback Registrations, beyond which 
	 * the Registration fails*/
	if (cb_registration_cnt == CB_REG_CNT_MAX) {
		fprintf(stderr, "Error: %s Max callback registrations allowed is 32. \n",__FUNCTION__);
		return -ENOTSUP;
	}

	/* We return Failure if both callbacks are NULL */
	if ((!pAppCallBack->pGWP_act_EthWanLinkDown) && (!pAppCallBack->pGWP_act_EthWanLinkUP)) {
		fprintf(stderr, "Error: %s NULL function pointers passed as arg\n",__FUNCTION__);
		return -EINVAL;
	}

	obj_l[cb_registration_cnt]->pGWP_act_EthWanLinkDown =  pAppCallBack->pGWP_act_EthWanLinkDown;
	obj_l[cb_registration_cnt]->pGWP_act_EthWanLinkUP =  pAppCallBack->pGWP_act_EthWanLinkUP;

	cb_registration_cnt++;
 	return 0;
 }
#endif
STATIC void check_lan_wan_ready()
{
        char lan_st[16];
        char wan_st[16];
        sysevent_get(sysevent_fd_gs, sysevent_token_gs, "lan-status", lan_st, sizeof(lan_st));
        sysevent_get(sysevent_fd_gs, sysevent_token_gs, "wan-status", wan_st, sizeof(wan_st));

        printf("****************************************************\n");
        printf("       %s   %s\n", lan_st, wan_st);
        printf("****************************************************\n");

	if (!strcmp(lan_st, "started") && !strcmp(wan_st, "started"))
        {
           sysevent_set(sysevent_fd_gs, sysevent_token_gs, "start-misc", "ready", 0);
           once = 1;
        }

	return;
}

STATIC void GWPEthWan_EnterBridgeMode(void)
{
    //GWP_UpdateEsafeAdminMode(DOCESAFE_ENABLE_DISABLE);
    //DOCSIS_ESAFE_SetErouterOperMode(DOCESAFE_EROUTER_OPER_DISABLED);
    /* Reset Switch, to remove all VLANs */ 
    // GSWT_ResetSwitch();
    //DOCSIS_ESAFE_SetEsafeProvisioningStatusProgress(DOCSIS_EROUTER_INTERFACE, ESAFE_PROV_STATE_NOT_INITIATED);
#if !defined (NO_MOCA_FEATURE_SUPPORT)
    char MocaStatus[16]  = {0};
#endif
    GWPROVETHWANLOG(" Entry %s \n", __FUNCTION__);
#if !defined (NO_MOCA_FEATURE_SUPPORT)
    syscfg_get(NULL, "MoCA_current_status", MocaStatus, sizeof(MocaStatus));
    GWPROVETHWANLOG(" MoCA_current_status = %s \n", MocaStatus);
    if ((syscfg_set_commit(NULL, "MoCA_previous_status", MocaStatus) != 0)) 
    {
        printf("syscfg_set failed\n");
    }
    system("dmcli eRT setv Device.MoCA.Interface.1.Enable bool false");
#endif
    char command[256];
    snprintf(command,sizeof(command),"sysevent set bridge_mode %d",active_mode) ;
    system(command);
    system("dmcli eRT setv Device.X_CISCO_COM_DeviceControl.ErouterEnable bool false");
    
    system("sysevent set forwarding-restart");
}

//Actually enter router mode
STATIC void GWPEthWan_EnterRouterMode(void)
{
         /* Coverity Issue Fix - CID:71381 : UnInitialised varible */
#if !defined (NO_MOCA_FEATURE_SUPPORT)
    char MocaPreviousStatus[16] = {0};
        int prev;
#endif
    GWPROVETHWANLOG(" Entry %s \n", __FUNCTION__);

//    bridge_mode = 0;
    char command[256];
    snprintf(command,sizeof(command),"sysevent set bridge_mode %d",BRMODE_ROUTER) ;
    system(command);
#if !defined (NO_MOCA_FEATURE_SUPPORT)
    syscfg_get(NULL, "MoCA_previous_status", MocaPreviousStatus, sizeof(MocaPreviousStatus));
    prev = atoi(MocaPreviousStatus);
    GWPROVETHWANLOG(" MocaPreviousStatus = %d \n", prev);
    if(prev == 1)
    {
        system("dmcli eRT setv Device.MoCA.Interface.1.Enable bool true");
    }
    else
    {
        system("dmcli eRT setv Device.MoCA.Interface.1.Enable bool false");
    }
#endif
    system("dmcli eRT setv Device.X_CISCO_COM_DeviceControl.ErouterEnable bool true");
    
    system("sysevent set forwarding-restart");
}

STATIC void GWPEthWan_ProcessUtopiaRestart(void)
{
    // This function is called when "system-restart" event is received, This
    // happens when WEBUI change bridge configuration. We do not restart the
    // whole system, only routing/bridging functions only

    int oldActiveMode = active_mode;

    bridge_mode = GWPEthWan_SysCfgGetInt("bridge_mode");
    //int loc_eRouterMode = GWP_SysCfgGetInt("last_erouter_mode");
    
    active_mode = getSyseventBridgeMode(bridge_mode);
 
    printf("bridge_mode = %d, active_mode = %d\n", bridge_mode, active_mode);
    GWPROVETHWANLOG(" bridge_mode = %d, active_mode = %d\n", bridge_mode, active_mode);

    if (oldActiveMode == active_mode) return; // Exit if no transition

    webui_started = 0;
    switch ( active_mode) 
    {
        case BRMODE_ROUTER:
            GWPEthWan_EnterRouterMode();
            break;

        case BRMODE_GLOBAL_BRIDGE:
        case BRMODE_PRIMARY_BRIDGE:
            GWPEthWan_EnterBridgeMode();
            break;
        default:
        break;
    }

}
/**************************************************************************/
/*! \fn void *GWPEthWan_sysevent_handler(void *data)
 **************************************************************************
 *  \brief Function to process sysevent event
 *  \return 0
**************************************************************************/
static void *GWPEthWan_sysevent_handler(void *data)
{
    UNREFERENCED_PARAMETER(data);
    GWPROVETHWANLOG( "Entering into %s\n",__FUNCTION__);
    async_id_t ipv4_status_asyncid;
    async_id_t ipv6_status_asyncid;
    async_id_t dhcp_server_status_asyncid;
    async_id_t firewall_restart_asyncid;
    async_id_t lan_restart_asyncid;
    async_id_t lan_stop_asyncid;
    async_id_t lan_status_asyncid;
    async_id_t pnm_status_asyncid;
    async_id_t primary_lan_l3net_asyncid;
#ifdef CONFIG_CISCO_HOME_SECURITY
    async_id_t homesecurity_lan_l3net_asyncid;
#endif
    async_id_t ntp_time_sync_asyncid;
    async_id_t ping_status_asyncid;
    async_id_t conn_status_asyncid;
    async_id_t system_restart_asyncid;
    async_id_t bridge_status_asyncid;
    int l2net_inst_up = FALSE;

    /* RIPD/Zebra event ids */
    async_id_t wan_status_asyncid;

    sysevent_set_options(sysevent_fd, sysevent_token, "ipv4-status", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "ipv4-status",  &ipv4_status_asyncid);

    sysevent_set_options(sysevent_fd, sysevent_token, "ipv6-status", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "ipv6-status",  &ipv6_status_asyncid);

    sysevent_set_options(sysevent_fd, sysevent_token, "dhcp_server-restart", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "dhcp_server-restart",  &dhcp_server_status_asyncid);

    sysevent_set_options(sysevent_fd, sysevent_token, "firewall-restart", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "firewall-restart",  &firewall_restart_asyncid);

    sysevent_set_options(sysevent_fd, sysevent_token, "lan-restart", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "lan-restart",  &lan_restart_asyncid);

    sysevent_set_options(sysevent_fd, sysevent_token, "lan-stop", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "lan-stop",  &lan_stop_asyncid);

#if defined(_PLATFORM_IPQ_)
    sysevent_set_options(sysevent_fd, sysevent_token, "bring-lan", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "bring-lan",  &pnm_status_asyncid);
#else
    sysevent_set_options(sysevent_fd, sysevent_token, "pnm-status", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "pnm-status",&pnm_status_asyncid);
#endif
    sysevent_set_options(sysevent_fd, sysevent_token, "primary_lan_l3net", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "primary_lan_l3net",  &primary_lan_l3net_asyncid);

#ifdef CONFIG_CISCO_HOME_SECURITY
    sysevent_set_options(sysevent_fd, sysevent_token, "homesecurity_lan_l3net", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "homesecurity_lan_l3net",  &homesecurity_lan_l3net_asyncid);
#endif
    /* Route events to start ripd and zebra */
    sysevent_set_options    (sysevent_fd, sysevent_token, "lan-status", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "lan-status",  &lan_status_asyncid);
    sysevent_set_options    (sysevent_fd, sysevent_token, "wan-status", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "wan-status",  &wan_status_asyncid);
    sysevent_set_options    (sysevent_fd, sysevent_token, "ntp_time_sync", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "ntp_time_sync",  &ntp_time_sync_asyncid);

    sysevent_setnotification(sysevent_fd, sysevent_token, "ping-status",  &ping_status_asyncid);
    sysevent_setnotification(sysevent_fd, sysevent_token, "conn-status",  &conn_status_asyncid);

    sysevent_setnotification(sysevent_fd, sysevent_token, "system-restart",  &system_restart_asyncid);
    sysevent_set_options(sysevent_fd, sysevent_token, "system-restart", TUPLE_FLAG_EVENT);

    sysevent_setnotification(sysevent_fd, sysevent_token, "bridge-status",  &bridge_status_asyncid);
    sysevent_set_options(sysevent_fd, sysevent_token, "bridge-status", TUPLE_FLAG_EVENT);

#if !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
    GwProvSetLED(YELLOW, BLINK, 1);
#endif
   sysevent_set(sysevent_fd, sysevent_token, "gw_prov-status", "up", 0);
   for (;;)
   {
        unsigned char name[33], val[42],buf[BUF_SIZE];;
        int namelen = sizeof(name);
        int vallen  = sizeof(val);
        int err;
        async_id_t getnotification_asyncid;
	char brlan0_inst[BRG_INST_SIZE], brlan1_inst[BRG_INST_SIZE];
        char* l3net_inst = NULL;
	FILE *responsefd=NULL;
      	char *networkResponse = "/var/tmp/networkresponse.txt";
        int iresCode = 0 , iRet = 0;
        char responseCode[10]={0}, cp_enable[10]={0}, redirect_flag[10]={0};
        int Led_Color = 0 , Led_State = 0 , Led_Interval = 0 ;
        int PING_STATUS = -1 , CONN_STATUS = -1 ;
        err = sysevent_getnotification(sysevent_fd, sysevent_token, name, &namelen,  val, &vallen, &getnotification_asyncid);

        if (err)
        {
           GWPROVETHWANLOG("sysevent_getnotification failed with error: %d\n", err);
        }
        else
        {
            GWPROVETHWANLOG("received notification event %s\n", name);
	    
            printf("received notification event %s\n", name);fflush(stdout);
	   if (strcmp(name, "ipv4-status")==0)
            {
                if (strcmp(val, "up")==0)
                {
                    //Need to Implement 
                }
                else if (strcmp(val, "down")==0)
                {
                    //Need to Implement 
                }
            }
            else if (strcmp(name, "ipv6-status")==0)
            {
                if (strcmp(val, "up")==0)
                {
                    //Need to Implement
					system("sysevent set sshd-restart"); 
                }
                else
                {
                    //Need to Implement 
                }
            }
	    else if (strcmp(name, "ntp_time_sync")==0)
            {
                GWPROVETHWANLOG("ntp time syncd, need to restart sshd %s\n", name);

                system("sysevent set sshd-restart");
            }
            else if (strcmp(name, "system-restart") == 0)
            {
                GWPROVETHWANLOG("gw_prov_sm: got system restart\n");
                GWPEthWan_ProcessUtopiaRestart();            }

            else if ( (strcmp(name, "dhcp_server-restart")==0) || (strcmp(name, "dhcpv6s_server")==0) )
            {
               //Need to Implement 
            }
            else if (strcmp(name, "firewall-restart")==0)
            {
		char cmd[100+5];
		    GWPROVETHWANLOG("received notification event %s\n", name);
		    snprintf(cmd,sizeof(cmd), "ip6tables -I OUTPUT -o %s -p icmpv6 -j DROP", ethwan_ifname);     //CID:79901
    		    system(cmd);
		    GWPROVETHWANLOG("cmd %s\n", cmd);
            }

  	   else if ( (strcmp(name, "ping-status") == 0) || (strcmp(name, "conn-status") == 0) )
            {
  
                GWPROVETHWANLOG("Received %s event notification, %s value is %s\n",name,name,val);

                if ( (strcmp(name, "ping-status") == 0) )
                {
                    PING_STATUS = 1 ;

                }
                else
                {
                    CONN_STATUS = 1 ;
                }

                if  ( ( PING_STATUS == 1 && (strcmp(val, "missed")==0) ) || ( CONN_STATUS == 1 && (strcmp(val, "failed")==0) ) )
                {
                        #if defined (_CBR2_PRODUCT_REQ_)
                         if ( PING_STATUS ==1 ) {
                            GWPROVETHWANLOG("Ping missed, Setting LED to WHITE FAST BLINK\n");
			 }
                         else
			 {
                            GWPROVETHWANLOG("Connection failed, Setting LED to WHITE FAST BLINK\n");
			 }
			 GwProvSetLED(WHITE, BLINK, 5) ;
			 #elif !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)

                         if ( PING_STATUS ==1 )
			 {
                            GWPROVETHWANLOG("Ping missed, Setting LED to RED\n");
			 }else {
                            GWPROVETHWANLOG("Connection failed, Setting LED to RED\n");
			}
			GwProvSetLED(RED, SOLID, 0) ;
			#endif
		
			// Set LED state to RED
                }
                else if  ( ( PING_STATUS == 1 && (strcmp(val, "received")==0) ) || ( CONN_STATUS == 1 && (strcmp(val, "success")==0) ) )
                {
		    // Set LED state based on whether device is in CP or not
	           Led_Color = WHITE ;
		   Led_State = SOLID ;
		   Led_Interval = 0 ;

 		   iRet = syscfg_get(NULL, "CaptivePortal_Enable", cp_enable, sizeof(cp_enable));
		
		    if ( ( iRet == 0 ) && ( strcmp(cp_enable,"true") == 0 ) )
		    {
			
			iRet=0;
		   	iRet = syscfg_get(NULL, "redirection_flag", redirect_flag, sizeof(redirect_flag));
			if ( ( iRet == 0 ) &&  (strcmp(redirect_flag,"true") == 0 ) )
			{
				
           	    		if((responsefd = fopen(networkResponse, "r")) != NULL)
            	    		{
                			if(fgets(responseCode, sizeof(responseCode), responsefd) != NULL)
                			{
                    				iresCode = atoi(responseCode);
                			}

                        		fclose(responsefd);
                			responsefd = NULL;
					if ( 204 == iresCode )
					{
					        Led_State = BLINK ;
			                        Led_Interval = 1 ;
					}
            	    		}
				
			}
		    }
		   
  		    if ( BLINK == Led_State )
		    {
         		GWPROVETHWANLOG("Device is in Captive Portal, setting WHITE LED to blink\n");
		    }
		    else
		    {
         		GWPROVETHWANLOG("Device is not in Captive Portal, setting LED to SOLID WHITE\n");
		    }

                    #if !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
                      GwProvSetLED(Led_Color, Led_State, Led_Interval) ;
                    #endif
                }
                PING_STATUS = -1;
                CONN_STATUS = -1;
            }
#if defined(_PLATFORM_IPQ_)
            else if ((strcmp(name, "lan-status") == 0 ||
                     strcmp(name, "wan-status") == 0) &&
                     strcmp(val, "started") == 0)
#else
            else if ((strcmp(name, "wan-status") == 0) &&
                     (strcmp(val, "started") == 0))
#endif
            {
                int restartFirewall = 0;
                // When lan-status and wan-status started, only call functions when both are started
                // or bad things will happen
                do
                {
                    unsigned char wan_status[20];
#if defined(_PLATFORM_IPQ_)
                    unsigned char lan_status[20];
                    // Make sure lan-status is started first...
                   lan_status[0] = '\0';
                    if (GWPEthWan_SyseventGetStr("lan-status", lan_status, sizeof(lan_status)) < 0)
                    {
                        break;
                    }
                    
                    if (strcmp(lan_status, "started") != 0)
                    {
                       if (!webui_started) {
                           system("/bin/sh /etc/webgui.sh");
                           webui_started = 1;
                       }
                    }
#endif
                    
                    // Make sure wan-status is started second...
                    wan_status[0] = '\0';
                    if (GWPEthWan_SyseventGetStr("wan-status", wan_status, sizeof(wan_status)) < 0)
                    {
                        break;
                    }
                    
                    if (strcmp(wan_status, "started") != 0)
                    {
                        if (!once) {
                              check_lan_wan_ready();
                        }
                    }
                    char command[100+5];
                    snprintf(command, sizeof(command), "sysctl -w net.ipv6.conf.%s.disable_ipv6=1", ethwan_ifname); // Fix: RDKB-21410, disabling IPv6 for ethwan port
                    printf("****************value of command = %s**********************\n", command);
                    system(command);
		    system("touch /tmp/phylink_wan_state_up");
                    system("sysevent set sshd-restart");
                    restartFirewall = 1;
                } while (0);

                if (strcmp(name, "lan-status") == 0)
                {
                     //Need to Implement 
                }
                
                if (restartFirewall == 1)
                {
                    //Need to Implement 
                }
            }
#if !defined(_CBR2_PRODUCT_REQ_)
            else if (strcmp(name, "lan-status") == 0 || strcmp(name, "bridge-status") == 0 ) 
            {
		if(strcmp(val, "started") == 0)
		{
                if ( ( strcmp(name, "bridge-status") ) == 0 && (!bridgeModeInBootup))
                {		    
                    GWPROVETHWANLOG("bridge-status = %s start webgui.sh \n", val );
                    system("/bin/sh /etc/webgui.sh &");
                }
		}
                bridgeModeInBootup = 0; // reset after lan/bridge status is received.
            }
#endif
            else if (strcmp(name, "lan-restart") == 0)
            {
                if (strcmp(val, "1")==0)
                {
                    //Need to Implement 
                }
            }
            else if (strcmp(name, "lan-stop") == 0)
            {
	        //Need to Implement 
            }
            else if (strcmp(name, "pnm-status") == 0)
            {
             	if (strcmp(val, "up")==0)
                 {
		         LAN_start();

                 }
            }
            else if (strcmp(name, "bring-lan") == 0)
            {
                if (strcmp(val, "up")==0)
                 {
                    pnm_inited = 1;
                    if (netids_inited) {
                        LAN_start();
                    }
                 }
            }
            else if (strcmp(name, "primary_lan_l3net") == 0)
            {
                GWPROVETHWANLOG(" primary_lan_l3net received \n");
                if (pnm_inited)
                 {
                    LAN_start();
                 }
                netids_inited = 1;
            }
#ifdef CONFIG_CISCO_HOME_SECURITY
            else if (strcmp(name, "homesecurity_lan_l3net") == 0)
            {
                GWPROVETHWANLOG(" homesecurity_lan_l3net received \n");
				sysevent_set(sysevent_fd_gs, sysevent_token_gs, "ipv4-up", val, 0);

            }
#endif
            else if (strcmp(name, "tr_" ER_NETDEVNAME "_dhcpv6_client_v6addr") == 0)
	    {
                Uint8 v6addr[ NETUTILS_IPv6_GLOBAL_ADDR_LEN / sizeof(Uint8) ];
                Uint8 soladdr[ NETUTILS_IPv6_GLOBAL_ADDR_LEN / sizeof(Uint8) ];
                inet_pton(AF_INET6, val, v6addr);

                inet_ntop(AF_INET6, soladdr, val, sizeof(val));


                sysevent_set(sysevent_fd_gs, sysevent_token_gs, "ipv6_"ER_NETDEVNAME"_dhcp_solicNodeAddr", val,0);

                unsigned char lan_wan_ready = 0;
                char command[256], result_buf[32];
                command[0] = result_buf[0] = '\0';

                sysevent_get(sysevent_fd_gs, sysevent_token_gs, "start-misc", result_buf, sizeof(result_buf));
                lan_wan_ready = strstr(result_buf, "ready") == NULL ? 0 : 1;

                if(!lan_wan_ready) {
                    snprintf(command, sizeof(command),"ip6tables -t mangle -I PREROUTING 1 -i %s -d %s -p ipv6-icmp -m icmp6 --icmpv6-type 135 -m limit --limit 20/sec -j ACCEPT", ER_NETDEVNAME, val);
                    system(command);
                }
                else
                    sysevent_set(sysevent_fd_gs, sysevent_token_gs, "firewall-restart", "",0);
            }
#if !defined(_PLATFORM_IPQ_) 
#if defined(_CBR2_PRODUCT_REQ_)
            else if (strcmp(name, "lan-status") == 0 || strcmp(name, "bridge-status") == 0 ) 
#else
            else if (strcmp(name, "lan-status") == 0 )
#endif
            {
		 GWPROVETHWANLOG(" lan-status received \n");
				if (strcmp(val, "started") == 0) {
				    if (!webui_started) { 
#if defined(_PLATFORM_RASPBERRYPI_) || defined(_PLATFORM_TURRIS_)
				       system("/bin/sh /etc/webgui.sh");
				       // For other devices CcspWebUI.service launches the GUI processes
#elif defined(_CBR2_PRODUCT_REQ_)
                        bridgeModeInBootup = 0; // reset after lan/bridge status is received.

                        startWebUIProcess();
#endif
				        webui_started = 1 ;

#ifdef CONFIG_CISCO_HOME_SECURITY
				        //Piggy back off the webui start event to signal XHS startup
				        sysevent_get(sysevent_fd_gs, sysevent_token_gs, "homesecurity_lan_l3net", buf, sizeof(buf));
				        if (buf[0] != '\0') sysevent_set(sysevent_fd_gs, sysevent_token_gs, "ipv4-up", buf, 0);
#endif

				    }
			sysevent_get(sysevent_fd_gs, sysevent_token_gs, "primary_lan_l3net", buf, sizeof(buf));
			strncpy(brlan0_inst, buf, BRG_INST_SIZE-1);
			sysevent_get(sysevent_fd_gs, sysevent_token_gs, "homesecurity_lan_l3net", buf, sizeof(buf));
			strncpy(brlan1_inst, buf, BRG_INST_SIZE-1);

			/*Get the active bridge instances and bring up the bridges */
			sysevent_get(sysevent_fd_gs, sysevent_token_gs, "l3net_instances", buf, sizeof(buf));
			
			l3net_inst = strtok(buf, " ");
			while(l3net_inst != NULL)
			{
			    /*brlan0 and brlan1 are already up. We should not call their instances again*/
			    if(!((strcmp(l3net_inst, brlan0_inst)==0) || (strcmp(l3net_inst, brlan1_inst)==0)))
			    {
				sysevent_set(sysevent_fd_gs, sysevent_token_gs, "ipv4-up", l3net_inst, 0);
				l2net_inst_up = TRUE;
			    }
			    l3net_inst = strtok(NULL, " ");
			}
			if(l2net_inst_up == FALSE)
			{
				sysevent_set(sysevent_fd_gs, sysevent_token_gs, "ipv4-up", brlan0_inst, 0);
				sysevent_set(sysevent_fd_gs, sysevent_token_gs, "ipv4-up", brlan1_inst, 0);
			}
				   
				    if (!hotspot_started) {
		#if !defined(INTEL_PUMA7) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
				        printf("Not Calling hotspot-start for XB3 it will be done in cosa_start_rem.sh\n");
		#else
				        /* TCXB6-1922: Adding 5 seconds delay as sysevents are not getting handled properly */
				        sleep(5);
				        sysevent_set(sysevent_fd_gs, sysevent_token_gs, "hotspot-start", "", 0);
				        hotspot_started = 1 ;
		#endif
				    }
	    } 
	    }
#endif
            else
            {
#ifdef UNIT_TEST_DOCKER_SUPPORT
               break;
#endif
               GWPROVETHWANLOG( "undefined event %s \n",name);
            }			
        }
    }

    GWPROVETHWANLOG("Exiting from %s\n",__FUNCTION__);
    return NULL;
}

#if defined(_PLATFORM_IPQ_) || defined(_PLATFORM_RASPBERRYPI_) || defined(_PLATFORM_TURRIS_)
static int GWP_act_ProvEntry_callback()
{
    GWPROVETHWANLOG( "Entering into %s\n",__FUNCTION__);

    system("mkdir -p /nvram");
#if defined(_PLATFORM_TURRIS_)
    system("mount /dev/mmcblk0p6 /nvram");
#endif
    system("rm -f /nvram/dnsmasq.leases");
    system("syslogd -f /etc/syslog.conf");

    //copy files that are needed by CCSP modules
    system("cp /usr/ccsp/ccsp_msg.cfg /tmp");
    system("touch /tmp/cp_subsys_ert");

    /* Below link is created because crond is expecting /crontabs/ dir instead of /var/spool/cron/crontabs */
    system("ln -s /var/spool/cron/crontabs /");
    /* directory /var/run/firewall because crond is expecting this dir to execute time specific blocking of firewall*/
    system("mkdir -p /var/run/firewall");

    system("/etc/utopia/utopia_init.sh");

    if (0 != GWPEthWan_SysCfgGetInt("bridge_mode"))
    {
        bridgeModeInBootup = 1;
    }

    sleep(2);
    /*
     * Invoking board specific configuration script to set the board related
     * syscfg parameters.
     */
#if !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
    system("/usr/bin/apply_board_defaults.sh");
#endif
    char command[100];
    char wanPhyName[20];
    char out_value[20];
    int outbufsz = sizeof(out_value);

    char previousLinkStatus[10] = "down";
    if (!syscfg_get(NULL, "wan_physical_ifname", out_value, outbufsz))
    {
       strcpy(wanPhyName, out_value);
       printf("wanPhyName = %s\n", wanPhyName);
    }
    else
    {
       return -1;
    }

    snprintf(command, sizeof(command), "ifconfig %s down", ETHWAN_DEF_INTF_NAME);
    printf("****************value of command = %s**********************\n", command);
    system(command);

    snprintf(command, sizeof(command), "ip link set %s name %s", ETHWAN_DEF_INTF_NAME, wanPhyName);
    printf("****************value of command = %s**********************\n", command);
    system(command);

    snprintf(command, sizeof(command), "ifconfig %s up", wanPhyName);
    printf("****************value of command = %s**********************\n", command);
    system(command);

    sysevent_fd = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov_ethwan", &sysevent_token);

    if (sysevent_fd >= 0)
    {
        system("sysevent set phylink_wan_state down");
        printf(" Creating Thread  GWPEthWan_sysevent_threadfunc \n");
        GWPROVETHWANLOG(" Creating Thread  GWPEthWan_sysevent_threadfunc \n");
        pthread_create(&sysevent_tid, NULL, GWPEthWan_sysevent_handler, NULL);
    }

    //Make another connection for gets/sets
    sysevent_fd_gs = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov_ethwan-gs", &sysevent_token_gs);

    LAN_start();

    pthread_create(&linkstate_tid, NULL, GWPEthWan_linkstate_threadfunc, NULL);
    return 0;
}
#else
static int GWP_act_ProvEntry_callback()
{
    char command[100+30];
#if !defined(FEATURE_RDKB_WAN_MANAGER)
    char wanPhyName[20];
    char out_value[20];
    int outbufsz = sizeof(out_value);
#endif
    GWPROVETHWANLOG(" Entry %s \n", __FUNCTION__);
    //system("sysevent set lan-start");
   
/* TODO: OEM to implement swctl apis */



    GWPROVETHWANLOG(" Calling /etc/utopia/utopia_init.sh \n"); 
    system("/etc/utopia/utopia_init.sh");

    sleep(2);
    sysevent_fd = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov", &sysevent_token);

    if (sysevent_fd >= 0)
    {
        system("sysevent set phylink_wan_state down");
        GWPROVETHWANLOG(" Creating Thread  GWPEthWan_sysevent_handler \n"); 
       // pthread_create(&sysevent_tid, NULL, GWPEthWan_sysevent_handler, NULL);
    }
    
    //Make another connection for gets/sets
    sysevent_fd_gs = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov-gs", &sysevent_token_gs);
#if !defined(FEATURE_RDKB_WAN_MANAGER)
   if (!syscfg_get(NULL, "wan_physical_ifname", out_value, outbufsz))
    {
       strcpy(wanPhyName, out_value);
       printf("wanPhyName = %s\n", wanPhyName);
    }
    else
    {
        strcpy(wanPhyName, "erouter0");
      
    }

	//Get the ethwan interface name from HAL
	memset( ethwan_ifname , 0, sizeof( ethwan_ifname ) );
	if ( ( 0 != GWP_GetEthWanInterfaceName( ethwan_ifname, sizeof(ethwan_ifname) ) ) )
	{
		//Fallback case needs to set it default
		memset( ethwan_ifname , 0, sizeof( ethwan_ifname ) );
		sprintf( ethwan_ifname , "%s", ETHWAN_DEF_INTF_NAME );
		GWPROVETHWANLOG(" Failed to get EthWanInterfaceName: %s \n", ethwan_ifname );		
	}

		GWPROVETHWANLOG(" EthWanInterfaceName: %s \n", ethwan_ifname ); 

        #if defined (_BRIDGE_UTILS_BIN_)

            if ( syscfg_set_commit( NULL, "eth_wan_iface_name", ethwan_ifname ) != 0 )
            {
                GWPROVETHWANLOG( "syscfg_set failed for eth_wan_iface_name\n" );
            }
        #endif
            
        macaddr_t macAddr;
        getWanMacAddress(&macAddr);
    int i =0;
    printf("eRouter macAddr ");
    for (i=0;i<6;i++)
    {
       printf("%2x ",macAddr.hw[i]);
    }
    printf(" \n");
    char wan_mac[18];// = {0};
    sprintf(wan_mac, "%02x:%02x:%02x:%02x:%02x:%02x",macAddr.hw[0],macAddr.hw[1],macAddr.hw[2],macAddr.hw[3],macAddr.hw[4],macAddr.hw[5]);

    snprintf(command, sizeof(command), "ifconfig %s down", ethwan_ifname);
    system(command);
   

#if !defined(INTEL_PUMA7)
    snprintf(command, sizeof(command), "vlan_util del_interface brlan0 %s", ethwan_ifname);
    system(command);
#endif

#ifdef _COSA_BCM_ARM_
    snprintf(command, sizeof(command), "ifconfig %s down; ip link set %s name cm0", wanPhyName, wanPhyName);
#else
    snprintf(command, sizeof(command), "ifconfig %s down; ip link set %s name dummy-rf", wanPhyName, wanPhyName);
#endif
    system(command);

#if 0
    snprintf(command, sizeof(command), "ip link set eth0 name %s", wanPhyName);
#else
    snprintf(command, sizeof(command), "brctl addbr %s; brctl addif %s %s", wanPhyName,wanPhyName,ethwan_ifname);
#endif
    printf("****************value of command = %s**********************\n", command);
    system(command);

#if defined(INTEL_PUMA7)
    snprintf(command, sizeof(command), "brctl addif %s dpdmta1",wanPhyName);
    printf("****************value of command = %s**********************\n", command);
    system(command);
    snprintf(command, sizeof(command), "echo 1 > /sys/devices/virtual/net/%s/bridge/nf_disable_iptables",wanPhyName);
    printf("****************value of command = %s**********************\n", command);
    system(command);
    snprintf(command, sizeof(command), "echo 1 > /sys/devices/virtual/net/%s/bridge/nf_disable_ip6tables",wanPhyName);
    printf("****************value of command = %s**********************\n", command);
    system(command);
    snprintf(command, sizeof(command), "echo 1 > /sys/devices/virtual/net/%s/bridge/nf_disable_arptables",wanPhyName);
    printf("****************value of command = %s**********************\n", command);
    system(command);
#endif
  
    snprintf(command, sizeof(command), "sysctl -w net.ipv6.conf.%s.autoconf=0", ethwan_ifname); // Fix: RDKB-22835, disabling IPv6 for ethwan port
    printf("****************value of command = %s**********************\n", command);
    system(command);

    snprintf(command, sizeof(command), "sysctl -w net.ipv6.conf.%s.disable_ipv6=1", ethwan_ifname); // Fix: RDKB-22835, disabling IPv6 for ethwan port
    printf("****************value of command = %s**********************\n", command);
    system(command);

    snprintf(command, sizeof(command), "ifconfig %s hw ether %s", ethwan_ifname,wan_mac);
    printf("****************value of command = %s**********************\n", command);
    system(command);

    snprintf(command, sizeof(command), "ip6tables -I OUTPUT -o %s -p icmpv6 -j DROP", ethwan_ifname);
    system(command);

#ifdef _COSA_BCM_ARM_
    system("ifconfig cm0 up");
    snprintf(command, sizeof(command), "brctl addbr %s; brctl addif %s cm0", wanPhyName,wanPhyName);
    printf("****************value of command = %s**********************\n", command);
    system(command);
    snprintf(command, sizeof(command), "sysctl -w net.ipv6.conf.cm0.disable_ipv6=1");
    printf("****************value of command = %s**********************\n", command);
    system(command);
#endif

    snprintf(command, sizeof(command), "ifconfig %s down", wanPhyName);
    system(command);
    snprintf(command, sizeof(command), "ifconfig %s hw ether %s", wanPhyName,wan_mac);
    printf("************************value of command = %s***********************\n", command);
    system(command);
    platform_hal_GetBaseMacAddress(wan_mac);
    printf("************************cmmac = %s***********************\n", wan_mac);
    snprintf(command, sizeof(command), "sysevent set eth_wan_mac %s", wan_mac);
    system(command);
      // setNetworkDeviceMacAddress(ER_NETDEVNAME,&macAddr);
    snprintf(command, sizeof(command), "ifconfig %s up", wanPhyName);
    printf("************************value of command = %s************************\n", command);
    system(command);
              
#if defined(INTEL_PUMA7) || defined(_CBR2_PRODUCT_REQ_)
    snprintf(command, sizeof(command), "ifconfig %s up",ethwan_ifname);
    printf("************************value of command = %s***********************\n", command);
    system(command);
#endif
#endif // ifndef FEATURE_RDKB_WAN_MANAGER

    #if 0
    system("sysevent set bridge_mode 0"); // to boot in router mode
    #endif
    
    int sysevent_bridge_mode = 0;
    bridge_mode = GWPEthWan_SysCfgGetInt("bridge_mode");
    validate_mode(&bridge_mode);
    sysevent_bridge_mode = getSyseventBridgeMode(bridge_mode);
    active_mode = sysevent_bridge_mode;

    GWPROVETHWANLOG(" active_mode %d \n", active_mode);
    snprintf(command, sizeof(command), "sysevent set bridge_mode %d", sysevent_bridge_mode);

    system(command);

    system("syscfg set eth_wan_enabled true"); // to handle Factory reset case
    return 0;
}
#endif

static bool GWPEthWan_Register_sysevent()
{
    bool status = false;
    const int max_retries = 6;
    int retry = 0;
    GWPROVETHWANLOG("Entering into %s\n",__FUNCTION__);
    do
    {
        sysevent_fd = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov_ethwan", &sysevent_token);
        if (sysevent_fd < 0)
        {
            GWPROVETHWANLOG("gw_prov_ethwan failed to register with sysevent daemon\n");
            status = false;
        }
        else
        {
            GWPROVETHWANLOG( "gw_prov_ethwan registered with sysevent daemon successfully\n");
            status = true;
        }
        
        //Make another connection for gets/sets
        sysevent_fd_gs = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov_ethwan-gs", &sysevent_token_gs);
        if (sysevent_fd_gs < 0)
        {
            GWPROVETHWANLOG( "gw_prov_ethwan-gs failed to register with sysevent daemon\n");
            status = false;
        }
        else
        {
            GWPROVETHWANLOG("gw_prov_ethwan-gs registered with sysevent daemon successfully\n");
            status = true;
        }

    }while((status == false) && (retry++ < max_retries));

    GWPROVETHWANLOG("Exiting from %s\n",__FUNCTION__);
    return status;
}

#if !defined(_PLATFORM_IPQ_)
STATIC int GWP_ETHWAN_Init()
{
    int status = 0;
    int thread_status = 0;
    GWPROVETHWANLOG("Entering into %s\n",__FUNCTION__);

    if (GWPEthWan_Register_sysevent() == false)
    {
        GWPROVETHWANLOG("GWP_ETHWAN_Register_sysevent failed\n");
        status = -1;
    }
    else 
    {
        GWPROVETHWANLOG("GWP_ETHWAN_Register_sysevent Successful\n");
#if defined(FEATURE_RDKB_WAN_MANAGER)
        system("sysevent set ethwan-initialized 1");
        system("syscfg set eth_wan_enabled true"); // to handle Factory reset case
        system("syscfg set ntp_enabled 1"); // Enable NTP in case of ETHWAN
        system("syscfg set last_wan_mode 1"); // to handle Factory reset case
        system("syscfg set curr_wan_mode 1"); // to handle Factory reset case
        system("syscfg commit");
#endif
 
        thread_status = pthread_create(&sysevent_tid, NULL, GWPEthWan_sysevent_handler, NULL);
        if (thread_status == 0)
        {
            GWPROVETHWANLOG("GWP_ETHWAN_sysevent_handler thread created successfully\n");              
            sleep(5);
        }
        else
        {
            GWPROVETHWANLOG("%s error occured while creating GWP_ETHWAN_sysevent_handler thread\n", strerror(errno));
            status = -1;
        }
    }
#if !defined(FEATURE_RDKB_WAN_MANAGER)
#ifdef AUTOWAN_ENABLE
    AutoWAN_main();
#endif
#endif
    GWPROVETHWANLOG("Exiting from %s\n",__FUNCTION__);
    return status;
}
#endif

static bool checkIfAlreadyRunning(const char* name)
{
    UNREFERENCED_PARAMETER(name);
    GWPROVETHWANLOG("Entering into %s\n",__FUNCTION__);
    bool status = true;

    FILE *fp = fopen("/tmp/.gwprovethwan.pid", "r");
    if (fp == NULL)
    {
        GWPROVETHWANLOG("File /tmp/.gwprovethwan.pid doesn't exist\n");
        FILE *pfp = fopen("/tmp/.gwprovethwan.pid", "w");
        if (pfp == NULL)
        {
            GWPROVETHWANLOG("Error in creating file /tmp/.gwprovethwan.pid\n");
        }
        else
        {
            pid_t pid = getpid();
            fprintf(pfp, "%d", pid);
            fclose(pfp);
        }
        status = false;
    }
    else
    {
        fclose(fp);
    }
    GWPROVETHWANLOG("Exiting from %s\n",__FUNCTION__);
    return status;
}

#if 0
static void daemonize(void) 
{
    GWPROVETHWANLOG("Entering into %s\n",__FUNCTION__);
    int fd;
    switch (fork()) {
    case 0:
      	GWPROVETHWANLOG("In child pid=%d\n", getpid());
        break;
    case -1:
    	// Error
    	GWPROVETHWANLOG( "Error daemonizing (fork)! %d - %s\n", errno, strerror(errno));
    	exit(0);
    	break;
    default:
     	GWPROVETHWANLOG("In parent exiting\n");
    	_exit(0);
    }

    //create new session and process group
    if (setsid() < 0) {
        GWPROVETHWANLOG( "Error demonizing (setsid)! %d - %s\n", errno, strerror(errno));
    	exit(0);
    }    

#ifndef  _DEBUG
    //redirect fd's 0,1,2 to /dev/null     
    fd = open("/dev/null", O_RDONLY);
    if (fd != 0) {
        dup2(fd, 0);
        close(fd);
    }
    fd = open("/dev/null", O_WRONLY);
    if (fd != 1) {
        dup2(fd, 1);
        close(fd);
    }
    fd = open("/dev/null", O_WRONLY);
    if (fd != 2) {
        dup2(fd, 2);
        close(fd);
    }
#endif
}
#endif

STATIC void LAN_start() {
        GWPROVETHWANLOG("Utopia starting lan...\n");
#if defined(_PLATFORM_RASPBERRYPI_) || defined(_PLATFORM_TURRIS_)
	int bridge_mode = 0;
        bridge_mode = GWPEthWan_SysCfgGetInt("bridge_mode");
        if(bridge_mode == 0)  // start the router mode set-up
        {
                printf("Utopia starting lan...\n");
                sysevent_set(sysevent_fd_gs, sysevent_token_gs, "lan-start", "", 0);
                sysevent_set(sysevent_fd_gs, sysevent_token_gs, "bridge-stop", "", 0);
        }
        else if(bridge_mode == 2)
        {
                printf("Utopia starting bridge...\n");
                sysevent_set(sysevent_fd_gs, sysevent_token_gs, "bridge-start", "", 0);
                sysevent_set(sysevent_fd_gs, sysevent_token_gs, "lan-stop", "", 0);
        }
        else
        {
                printf("starting with different mode ..\n");
        }
#else
    if (bridge_mode == 0) 
    {
        printf("Utopia starting lan...\n");
        GWPROVETHWANLOG(" Setting lan-start event \n");           
        sysevent_set(sysevent_fd_gs, sysevent_token_gs, "lan-start", "", 0);
        
        
    } else {
        // TODO: fix this
        printf("Utopia starting bridge...\n");
        GWPROVETHWANLOG(" Setting bridge-start event \n");         
        sysevent_set(sysevent_fd_gs, sysevent_token_gs, "bridge-start", "", 0);
    }

#endif
        sysevent_set(sysevent_fd_gs, sysevent_token_gs, "dhcp_server-resync", "", 0);

        return;
}

//#define EROUTER_PRIV_NET(X)      "172.31.255." #X /* X=25 is CM and X=40 is eRouter */
#if !defined(_PLATFORM_IPQ_) && (!defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_))
#if !defined(FEATURE_RDKB_WAN_MANAGER)
STATIC void GWP_CreateTFTPServerForCMConsoleLogs()
{
 	char cmd[BUF_SIZE]; 
 	if (!tftpserverStarted)
 	{	
	 	//snprintf(cmd,sizeof(cmd), "/usr/bin/udpsvd -E %s 69 /usr/sbin/tftpd -c /var/tftpboot &", EROUTER_PRIV_NET(40));
          	snprintf(cmd,sizeof(cmd), "/usr/bin/udpsvd -E %s 69 /usr/sbin/tftpd -c /var/tftpboot &","172.31.255.40");
		if (system(cmd) == -1)
			fprintf(stderr,"%s failed\n", cmd);
		else
		 	tftpserverStarted = TRUE;
 	}
}
#endif
#endif
/**************************************************************************/
/*! \fn int main(int argc, char *argv)
 **************************************************************************
 *  \brief Init and run the Provisioning process
 *  \param[in] argc
 *  \param[in] argv
 *  \return Currently, never exits
 **************************************************************************/
#ifdef UNIT_TEST_DOCKER_SUPPORT
int UT_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    UNREFERENCED_PARAMETER(argc);
    int status = 0;
#if !defined(_PLATFORM_IPQ_)
#if !defined(FEATURE_RDKB_WAN_MANAGER)
    appCallBack *obj     =    NULL;
#endif
    char sysevent_cmd[80];
#endif
#if defined(_INTEL_BUG_FIXES_)
    macaddr_t macAddr;
#endif
    sleep(2);
#ifdef FEATURE_SUPPORT_RDKLOG
    setenv("LOG4C_RCPATH","/rdklogger",1);
    rdk_logger_init(DEBUG_INI_NAME);
#endif
    GWPROVETHWANLOG("GWP_ETHWAN Started gw_prov_EthWan\n");

#if !defined(_PLATFORM_IPQ_)

    if (checkIfAlreadyRunning(argv[0]) == true)
    {
        GWPROVETHWANLOG("Process %s already running\n", argv[0]);
        status = 1;
    }
    else
    {    


 			GWP_act_ProvEntry_callback();
#if !defined(FEATURE_RDKB_WAN_MANAGER)
#if !defined(_PLATFORM_IPQ_) && !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
	        CcspHalEthSwInit();
#endif
#endif
            if (GWP_ETHWAN_Init() != 0)
            {
                GWPROVETHWANLOG( "GWP_ETHWAN Initialization failed\n");
                status = 1;
            }
            else
            {
                GWPROVETHWANLOG("GWP_ETHWAN initialization completed\n");
                //wait for sysevent_tid thread to terminate
                //pthread_join(sysevent_tid, NULL);
                
                GWPROVETHWANLOG("sysevent_tid thread terminated\n");
            }
#if !defined(FEATURE_RDKB_WAN_MANAGER)
    GWPROVETHWANLOG("GWP_ETHWAN initialization completed\n");
	obj = ( appCallBack* ) malloc ( sizeof ( appCallBack ) );

	obj->pGWP_act_EthWanLinkDown =  (void *)GWP_EthWanLinkDown_callback;
	obj->pGWP_act_EthWanLinkUP =  (void *)GWP_EthWanLinkUp_callback;
	GWPROVETHWANLOG("GWP_ETHWAN Creating RegisterEthWan Handler\n");
#if !defined (_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
	GWP_RegisterEthWan_Callback ( obj );
       //As per Broadcom Comments which mentioned in TCXB6-8538
        //To get the CM console logs creation of tftpserver is required
        //In case of docsis ccsp-gwprovapp will call eSafeDevice_Initialize which will install Brcm tftp server.
        // In case of ccsp-gwprovapp-ethwan we should create the tftp server 
        GWP_CreateTFTPServerForCMConsoleLogs();

#endif
#endif //ifndef FEATURE_RDKB_WAN_MANAGER
	GWPROVETHWANLOG("GWP_ETHWAN Creating RegisterEthWan Handler over\n");
#if defined(_INTEL_BUG_FIXES_)
    getNetworkDeviceMacAddress(&macAddr);
    snprintf(sysevent_cmd, sizeof(sysevent_cmd), "%02x:%02x:%02x:%02x:%02x:%02x",
        macAddr.hw[0],macAddr.hw[1],
        macAddr.hw[2],macAddr.hw[3],
        macAddr.hw[4],macAddr.hw[5]);
#endif

    if ((syscfg_set(NULL, BASE_MAC_SYSCFG_KEY, sysevent_cmd) != 0))
    {
        fprintf(stderr, "Error in %s: Failed to set %s!\n", __FUNCTION__, BASE_MAC_SYSCFG_KEY);
    }

    /* Update LAN bridge mac address offset */
    if ((syscfg_set_u(NULL, BASE_MAC_BRIDGE_OFFSET_SYSCFG_KEY, BASE_MAC_BRIDGE_OFFSET) != 0))
    {
        fprintf(stderr, "Error in %s: Failed to set %s!\n", __FUNCTION__, BASE_MAC_BRIDGE_OFFSET_SYSCFG_KEY);
    }

    /* Update wired LAN interface mac address offset */
    if ((syscfg_set_u(NULL, BASE_MAC_LAN_OFFSET_SYSCFG_KEY, BASE_MAC_LAN_OFFSET) != 0))
    {
        fprintf(stderr, "Error in %s: Failed to set %s!\n", __FUNCTION__, BASE_MAC_LAN_OFFSET_SYSCFG_KEY);
    }

    /* Update WiFi interface mac address offset */
    if ((syscfg_set_u(NULL, BASE_MAC_WLAN_OFFSET_SYSCFG_KEY, BASE_MAC_WLAN_OFFSET) != 0))
    {
        fprintf(stderr, "Error in %s: Failed to set %s!\n", __FUNCTION__, BASE_MAC_WLAN_OFFSET_SYSCFG_KEY);
    }

// Lan_start needs to be after pnm-status or bring-lan event
	//LAN_start();

#ifdef _COSA_BCM_ARM_ 
        {
            appCallBack *pObjEthwan = NULL;
            pObjEthwan = (appCallBack*)malloc(sizeof(appCallBack));
            memset(pObjEthwan, 0, sizeof(appCallBack));

            GWPROVETHWANLOG(" Creating Event Handler\n");

#if !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
            /*appCallBack doesn't match with the one define in gw_prov_abstraction.h, pass NULL point just to start RPC tunnel. */
            SME_CreateEventHandler(NULL);
#endif

            GWPROVETHWANLOG(" Creating Event Handler over\n");
        }
#endif

	while(1)
	{
#ifndef UNIT_TEST_DOCKER_SUPPORT
            sleep(30);
#else
            break;
#endif
	}
    }
#else
    for (int i = 0; i < CB_REG_CNT_MAX; i++) {
	obj_l[i] = ( appCallBack* ) malloc ( sizeof ( appCallBack ) );
	obj_l[i]->pGWP_act_EthWanLinkDown = NULL;
	obj_l[i]->pGWP_act_EthWanLinkUP = NULL;
    }

    GWP_act_ProvEntry_callback();
    (void) pthread_join(sysevent_tid, NULL);
    (void) pthread_join(linkstate_tid, NULL);
#endif

    return status;
}
#endif
