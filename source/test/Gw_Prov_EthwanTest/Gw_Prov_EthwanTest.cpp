/*
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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

#include "Gw_Prov_Ethwan_Mock.h"
#include "../../include/gw_prov_ethwan.h"
#include  "test/mocks/mock_gwprovabs.h"

#define WHITE 0
#define YELLOW 1
#define SOLID   0
#define BLINK   1
#define RED     3
#define BRMODE_ROUTER 0
#define BRMODE_PRIMARY_BRIDGE   3
#define BRMODE_GLOBAL_BRIDGE 2

GWPROVABS_Mock *g_gwprovabs = nullptr;

extern "C"
{
    extern unsigned char tftpserverStarted;
    void LAN_start();
    int GwProvSetLED(int color, int state, int interval);
    void getNetworkDeviceMacAddress(macaddr_t* macAddr);
    int UT_main(int argc, char *argv[]);
    void validate_mode(int* bridge_mode);
    int getSyseventBridgeMode(int bridgeMode);
    int GWP_EthWanLinkDown_callback();
    int ethGetPHYRate(CCSP_HAL_ETHSW_PORT PortId);
    int GWP_EthWanLinkUp_callback();
    void GWPEthWan_EnterBridgeMode(void);
    void GWPEthWan_EnterRouterMode(void);
    void GWPEthWan_ProcessUtopiaRestart(void);
    void GWP_CreateTFTPServerForCMConsoleLogs();
    void check_lan_wan_ready();
    int GWPEthWan_SysCfgGetInt(const char *name);
    int GWP_ETHWAN_Init();
}

class Gw_Prov_EthwanTest : public ::testing::Test {
    protected:
    void SetUp() override {
        g_ethSwHALMock = new EthSwHalMock();
        g_syscfgMock = new SyscfgMock();
        g_syseventMock = new SyseventMock();
        g_platformHALMock = new PlatformHalMock();
        g_rdkloggerMock = new rdkloggerMock();
        g_PtdHandlerMock = new PtdHandlerMock();
        g_utilMock = new UtilMock();
        g_cmHALMock = new CmHalMock();
        g_gwprovabs = new GWPROVABS_Mock();
    }
    void TearDown() override {
        delete g_ethSwHALMock;
        delete g_syscfgMock;
        delete g_syseventMock;
        delete g_platformHALMock;
        delete g_rdkloggerMock;
        delete g_PtdHandlerMock;
        delete g_utilMock;
        delete g_cmHALMock;
        delete g_gwprovabs;

        g_syscfgMock = nullptr;
        g_ethSwHALMock = nullptr;
        g_syseventMock = nullptr;
        g_platformHALMock = nullptr;
        g_rdkloggerMock = nullptr;
        g_PtdHandlerMock = nullptr;
        g_utilMock = nullptr;
        g_cmHALMock = nullptr;
        g_gwprovabs = nullptr;
    }
};

TEST_F(Gw_Prov_EthwanTest, MainFunction_checkIfAlreadyRunning_false_syscfgset_fails) {

    EXPECT_CALL(*g_rdkloggerMock, rdk_logger_init(_)).Times(testing::AtLeast(1));
    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));

    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));

    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_, _))
        .WillRepeatedly(::testing::Return(-1));

    EXPECT_CALL(*g_syseventMock, sysevent_open(_, _, _, _, _)).Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return(0));
    
    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
        if (strcmp(default_value, "CaptivePortal_Enable") == 0 || strcmp(default_value, "redirection_flag") == 0 || strcmp(default_value,"mesh_ovs_enable") == 0) {
            strncpy(value, "true", value_len - 1);
            value[value_len - 1] = '\0'; 
            return 0;
        }
        strncpy(value, "true", value_len - 1);
            value[value_len - 1] = '\0'; 
        return 0; 
    }));

    EXPECT_CALL(*g_ethSwHALMock, CcspHalEthSwInit()).Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return(0));

    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_commit(_,_)).Times(1).WillOnce(Return(-1));
    
    EXPECT_CALL(*g_gwprovabs, getWanMacAddress(_)).Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return()); 

    EXPECT_CALL(*g_syseventMock, sysevent_set(_, _, _, _, _)).Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*g_syseventMock, sysevent_set_options(_, _, _, _)).Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*g_syseventMock, sysevent_setnotification(_, _, _, _)).Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return(0));

    EXPECT_CALL(*g_ethSwHALMock, GWP_GetEthWanInterfaceName(_, _)).Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return(-1));

    EXPECT_CALL(*g_platformHALMock, platform_hal_GetBaseMacAddress(_)).Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return(0));

    EXPECT_CALL(*g_ethSwHALMock, GWP_RegisterEthWan_Callback(_)).Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return());

    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns(_, _)).Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return(-1));

    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u(_, _)).Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return(-1));

    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* val, int val_len) {
            if (strcmp(name, "lan-status") == 0) {
                strcpy(val, "started");
            } else if (strcmp(name, "wan-status") == 0) {
                strcpy(val, "started");
            }
            else
            {
                strcpy(val, "up");
            }
            return 0;
        }));

    EXPECT_CALL(*g_syseventMock, sysevent_getnotification(_, _, _, _, _, _, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](const int sysevent_fd, const token_t sysevent_token, char *name, int *namelen, char *val, int *vallen, async_id_t *getnotification_asyncid) {
        static int call_count = 0;
        if (call_count == 0) {
            strcpy(name, "ipv4-status");
            strcpy(val, "up");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        } 
        else if(call_count == 1) {
            strcpy(name, "ipv4-status");
            strcpy(val, "down");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 2)
        {
            strcpy(name, "ipv6-status");
            strcpy(val, "up");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 3)
        {
            strcpy(name, "ipv6-status");
            strcpy(val, "down");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 4)
        {
            strcpy(name, "ntp_time_sync");
            strcpy(val, "up");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 5)
        {
            strcpy(name, "system-restart");
            strcpy(val, "up");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 6)
        {
            strcpy(name, "dhcp_server-restart");
            strcpy(val, "up");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 7)
        {
            strcpy(name, "dhcpv6s_server");
            strcpy(val, "up");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 8)
        {
            strcpy(name, "firewall-restart");
            strcpy(val, "up");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 9)
        {
            strcpy(name, "ping-status");
            strcpy(val, "missed");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 10)
        {
            strcpy(name, "conn-status");
            strcpy(val, "failed");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 11)
        {
            strcpy(name, "ping-status");
            strcpy(val, "received");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 12)
        {
            strcpy(name, "conn-status");
            strcpy(val, "success");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 13)
        {
            strcpy(name, "wan-status");
            strcpy(val, "started");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 14)
        {
            strcpy(name, "lan-status");
            strcpy(val, "started");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 15)
        {
            strcpy(name, "bridge-status");
            strcpy(val, "started");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 16)
        {
            strcpy(name, "lan-restart");
            strcpy(val, "1");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 17)
        {
            strcpy(name, "lan-stop");
            strcpy(val, "0");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 18)
        {
            strcpy(name, "pnm-status");
            strcpy(val, "up");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 19)
        {
            strcpy(name, "bring-lan");
            strcpy(val, "up");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 20)
        {
            strcpy(name, "primary_lan_l3net");
            strcpy(val, "up");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if(call_count == 21)
        {
            strcpy(name, "homesecurity_lan_l3net");
            strcpy(val, "up");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if (call_count == 22)
        {
            strcpy(name, "tr_erouter0_dhcpv6_client_v6addr\0");
            name[33 - 1] = '\0';
            strcpy(val, "up");
            val[42 - 1] = '\0';
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else if (call_count == 23)
        {
            strcpy(name, "exit");
            strcpy(val, "1");
            *namelen = strlen(name);
            *vallen = strlen(val);
            call_count++;
            return 0;
        }
        else {
            return -1;
        }
    }));

    char* argv[] = { "test_program", nullptr };
    int argc = 1;
    int result;

    remove("/tmp/.gwprovethwan.pid");
    result = UT_main(argc, argv);
    EXPECT_EQ(result, 0);
}

TEST_F(Gw_Prov_EthwanTest, MainFunction_checkIfAlreadyRunning_true) {

    EXPECT_CALL(*g_rdkloggerMock, rdk_logger_init(_)).Times(testing::AtLeast(1));
    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));

    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillOnce(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
            if (strcmp(default_value, "wan_physical_ifname") == 0) {
                strncpy(value, "erouter0", value_len - 1);
                value[value_len - 1] = '\0';
                return 0;
            }
            else
            {
                strncpy(value, "erouter0", value_len - 1);
                value[value_len - 1] = '\0';
            }
            return 0;
        }));

 /*   EXPECT_CALL(*g_cmHALMock, docsis_IsEnergyDetected(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](unsigned char* pRfSignalStatus) {
            if (pRfSignalStatus) {
                *pRfSignalStatus = 1;
            }
            return 0;
        }));*/

    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* val, int val_len) {
            if (strcmp(name, "lan-status") == 0) {
                strcpy(val, "started");
            } else if (strcmp(name, "wan-status") == 0) {
                strcpy(val, "started");
            }
            return 0;
        }));

    EXPECT_CALL(*g_ethSwHALMock, GWP_GetEthWanInterfaceName(_, _))
    .Times(testing::AtLeast(1))
    .WillOnce(testing::Invoke([](unsigned char* ifname, unsigned long len) {
        strncpy(reinterpret_cast<char*>(ifname), "disable", len - 1);
        ifname[len - 1] = '\0';
        return -1;
    }));

    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));

    char* argv[] = { "test_program", nullptr };
    int argc = 1;
   FILE *file = fopen("/tmp/.gwprovethwan.pid", "w");
    if (file == NULL) {
        printf("Error opening file");
    }
    int result = UT_main(argc, argv);
    EXPECT_EQ(result, 1);
    fclose(file);
}

TEST_F(Gw_Prov_EthwanTest, GwProvSetLED_platform_hal_setLed_pass) {
    EXPECT_EQ(0, GwProvSetLED(WHITE, BLINK, 0));
}

TEST_F(Gw_Prov_EthwanTest, validate_mode_bridge_mode_1) {

    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(1).WillOnce(Return(0));
    int bridge_mode = 1;
    validate_mode(&bridge_mode);

    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(1).WillOnce(Return(-1));
    bridge_mode = 1;
    validate_mode(&bridge_mode);

    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    bridge_mode = 0;
    validate_mode(&bridge_mode);
}

TEST_F(Gw_Prov_EthwanTest, getSyseventBridgeMode) {
    int bridge_mode = 1;
    EXPECT_EQ(0, getSyseventBridgeMode(0));
    EXPECT_EQ(0, getSyseventBridgeMode(1));
    EXPECT_EQ(2, getSyseventBridgeMode(2));
    EXPECT_EQ(3, getSyseventBridgeMode(3));
}

TEST_F(Gw_Prov_EthwanTest, GWP_EthWanLinkDown_callback) {
    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    EXPECT_EQ(0, GWP_EthWanLinkDown_callback());
}

TEST_F(Gw_Prov_EthwanTest, ethGetPHYRate_AllCases) {

    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    struct TestCase {
        _CCSP_HAL_ETHSW_LINK_RATE linkRate;
        _CCSP_HAL_ETHSW_DUPLEX_MODE duplexMode;
        int expectedPHYRate;
    };

    std::vector<TestCase> testCases = {
        {CCSP_HAL_ETHSW_LINK_10Mbps, CCSP_HAL_ETHSW_DUPLEX_Auto, 10},
        {CCSP_HAL_ETHSW_LINK_100Mbps, CCSP_HAL_ETHSW_DUPLEX_Auto, 100},
        {CCSP_HAL_ETHSW_LINK_1Gbps, CCSP_HAL_ETHSW_DUPLEX_Auto, 1000},
#ifdef _2_5G_ETHERNET_SUPPORT_
        {CCSP_HAL_ETHSW_LINK_2_5Gbps, CCSP_HAL_ETHSW_DUPLEX_Auto, 2500},
        {CCSP_HAL_ETHSW_LINK_5Gbps, CCSP_HAL_ETHSW_DUPLEX_Auto, 5000},
#endif
        {CCSP_HAL_ETHSW_LINK_10Gbps, CCSP_HAL_ETHSW_DUPLEX_Auto, 10000},
        {CCSP_HAL_ETHSW_LINK_Auto, CCSP_HAL_ETHSW_DUPLEX_Auto, 1000},
        {CCSP_HAL_ETHSW_LINK_NULL, CCSP_HAL_ETHSW_DUPLEX_Auto, 0}
    };

    for (const auto& testCase : testCases) {
        EXPECT_CALL(*g_ethSwHALMock, CcspHalEthSwGetPortCfg(_, _, _))
            .Times(1)
            .WillOnce(testing::Invoke([=](_CCSP_HAL_ETHSW_PORT portId,
                                          _CCSP_HAL_ETHSW_LINK_RATE* linkRate,
                                          _CCSP_HAL_ETHSW_DUPLEX_MODE* duplexMode) {
                *linkRate = testCase.linkRate;
                *duplexMode = testCase.duplexMode;
                return 0;
            }));

        EXPECT_EQ(testCase.expectedPHYRate, ethGetPHYRate((CCSP_HAL_ETHSW_PORT)CCSP_HAL_ETHSW_EthPort1));
    }
}

TEST_F(Gw_Prov_EthwanTest, GWP_EthWanLinkUp_callback) {
    
    EXPECT_CALL(*g_ethSwHALMock, CcspHalExtSw_getEthWanPort(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));

    EXPECT_CALL(*g_ethSwHALMock, CcspHalEthSwGetPortCfg(_, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](_CCSP_HAL_ETHSW_PORT portId, _CCSP_HAL_ETHSW_LINK_RATE* linkRate, _CCSP_HAL_ETHSW_DUPLEX_MODE* duplexMode) {
            *linkRate = CCSP_HAL_ETHSW_LINK_1Gbps;
            *duplexMode = CCSP_HAL_ETHSW_DUPLEX_Auto;
            return 0;
        }));
    // Test case 1: for success case
    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillOnce(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
            if (strcmp(default_value, "wan_physical_ifname") == 0) {
                strncpy(value, "erouter0", value_len - 1);
                value[value_len - 1] = '\0';
                return 0;
            }
            return 0;
        }));

    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    EXPECT_EQ(0, GWP_EthWanLinkUp_callback());

    // Test case 2: for CcspHalExtSw_getEthWanPort failure case

    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillOnce(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
            if (strcmp(default_value, "wan_physical_ifname") == 0) {
                strncpy(value, "erouter0", value_len - 1);
                value[value_len - 1] = '\0';
                return 0;
            }
            return 0;
        }));
    EXPECT_CALL(*g_ethSwHALMock, CcspHalExtSw_getEthWanPort(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(-1));
    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    EXPECT_EQ(0, GWP_EthWanLinkUp_callback());

    // Test case 3: for syscfg_get failure case
    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillOnce(testing::Return(-1));
    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    EXPECT_EQ(-1, GWP_EthWanLinkUp_callback());
}

TEST_F(Gw_Prov_EthwanTest, GWPEthWan_EnterBridgeMode) {

    // Test case 1: for success case

    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_commit(_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillOnce(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
            if (strcmp(default_value, "MoCA_previous_status") == 0) {
                strncpy(value, "Enabled", value_len - 1);
                value[value_len - 1] = '\0';
                return 0;
            }
            return 0;
        }));

    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    GWPEthWan_EnterBridgeMode();

    // Test case 2: for syscfg_set_nns_commit failure case
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_commit(_,_)).Times(1).WillOnce(Return(-1));
    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillOnce(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
            if (strcmp(default_value, "MoCA_previous_status") == 0) {
                strncpy(value, "Enabled", value_len - 1);
                value[value_len - 1] = '\0';
                return 0;
            }
            return 0;
        }));

    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    GWPEthWan_EnterBridgeMode();
}

TEST_F(Gw_Prov_EthwanTest, GWPEthWan_EnterRouterMode) {

    // Test case 1: for success case prev == 1

    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillOnce(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
            if (strcmp(default_value, "MoCA_previous_status") == 0) {
                strncpy(value, "1", value_len - 1);
                value[value_len - 1] = '\0';
                return 0;
            }
            return 0;
        }));

    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    GWPEthWan_EnterRouterMode();

    // Test case 2: for prev == 0

    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillOnce(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
            if (strcmp(default_value, "MoCA_previous_status") == 0) {
                strncpy(value, "0", value_len - 1);
                value[value_len - 1] = '\0';
                return 0;
            }
            return 0;
        }));

    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    GWPEthWan_EnterRouterMode();
}

TEST_F(Gw_Prov_EthwanTest, GWPEthWan_ProcessUtopiaRestart) {

    // Test case 1: for active_mode == 1
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_commit(_,_)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
            if (strcmp(default_value, "bridge_mode") == 0) {
                strncpy(value, "3", value_len - 1);
                value[value_len - 1] = '\0';
                return 0;
            }
            return 0;
        }));

    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    GWPEthWan_ProcessUtopiaRestart();

    // Test case 2: for active_mode == 0
    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));
    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
            if (strcmp(default_value, "bridge_mode") == 0) {
                strncpy(value, "1", value_len - 1);
                value[value_len - 1] = '\0';
                return 0;
            }
            return 0;
        }));

    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));

    GWPEthWan_ProcessUtopiaRestart();
}

TEST_F(Gw_Prov_EthwanTest, GWP_CreateTFTPServerForCMConsoleLogs) {
    // Scenario 1: system() returns success
    tftpserverStarted = 0; // Reset flag to FALSE
    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(0));
    GWP_CreateTFTPServerForCMConsoleLogs();

    // Scenario 2: system() returns failure
    tftpserverStarted = 0; // Reset flag to FALSE for the next test case
    EXPECT_CALL(*g_utilMock, system(_)).Times(testing::AtLeast(1)).WillRepeatedly(testing::Return(-1));
    GWP_CreateTFTPServerForCMConsoleLogs();
}

TEST_F(Gw_Prov_EthwanTest, check_lan_wan_ready) {

    // Test case 1: for lan-status and wan-status started
    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* val, int val_len) {
            if (strcmp(name, "lan-status") == 0) {
                strcpy(val, "started");
            } else if (strcmp(name, "wan-status") == 0) {
                strcpy(val, "started");
            }
            return 0;
        }));

    EXPECT_CALL(*g_syseventMock, sysevent_set(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillOnce(testing::Return(0));
    check_lan_wan_ready();

    // Test case 2: for lan-status and wan-status not started
    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* val, int val_len) {
            if (strcmp(name, "lan-status") == 0) {
                strcpy(val, "stopped");
            } else if (strcmp(name, "wan-status") == 0) {
                strcpy(val, "stopped");
            }
            return 0;
        }));
    check_lan_wan_ready();
}

TEST_F(Gw_Prov_EthwanTest, GWPEthWan_SysCfgGetInt) {
    const char * mode = "bridge_mode";

    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillOnce(Return(-1));
    EXPECT_EQ(-1, GWPEthWan_SysCfgGetInt(mode));
}

TEST_F(Gw_Prov_EthwanTest, GWP_ETHWAN_Init) {

    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_, _))
        .WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
        if (strcmp(default_value, "CaptivePortal_Enable") == 0 || strcmp(default_value, "redirection_flag") == 0 || strcmp(default_value,"mesh_ovs_enable") == 0) {
            strncpy(value, "true", value_len - 1);
            value[value_len - 1] = '\0'; 
            return 0;
        }
        strncpy(value, "true", value_len - 1);
            value[value_len - 1] = '\0'; 
        return 0; 
    }));
    EXPECT_CALL(*g_syseventMock, sysevent_open(_, _, _, _, _)).Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return(-1));
    EXPECT_EQ(-1, GWP_ETHWAN_Init());

}

TEST_F(Gw_Prov_EthwanTest, LAN_start) {

    EXPECT_CALL(*g_rdkloggerMock, rdk_dbg_MsgRaw(_, _, _, _)).Times(testing::AtLeast(1));
    // Test case 1: for bridge_mode == 0

    EXPECT_CALL(*g_syseventMock, sysevent_set(_, _, _, _, _))


        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(0));
    LAN_start();

    // Test case 2: for bridge_mode == 2

    EXPECT_CALL(*g_syseventMock, sysevent_set(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(0));
    LAN_start();
}