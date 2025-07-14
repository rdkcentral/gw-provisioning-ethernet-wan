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
#include "../../include/autowan.h"
#include <mocks/mock_file_io.h>

#define WAN_MODE_AUTO		0
#define WAN_MODE_ETH		1
#define WAN_MODE_DOCSIS		2
#define WAN_MODE_UNKNOWN	3

extern FileIOMock * g_fileIOMock;
FileIOMock * g_fileIOMock = NULL;

extern "C" {
    extern int g_OvsEnable;
    extern int g_SelectedWanMode;
    extern int g_LastKnowWanMode;
    int GetCurrentWanMode();
    int SetCurrentWanMode(int mode);
     void SelectedWanMode(int mode);
     void SetLastKnownWanMode(int mode);
     void WanMngrThread(void * arg);
    void HandleAutoWanMode();
    void ManageWanModes(int mode);
    int CheckWanStatus(int mode);
    void TryAltWan(int *mode);
    void RevertTriedConfig(int mode);
    void AutoWan_BkupAndReboot();
}

class AutowanTest : public ::testing::Test {
    protected:
    void SetUp() override {
         g_syscfgMock = new SyscfgMock();
         g_syseventMock = new SyseventMock();
        g_fileIOMock = new FileIOMock();
        g_ethSwHALMock = new EthSwHalMock();
        g_cmHALMock = new CmHalMock();
        g_utilMock = new UtilMock();
    }
    void TearDown() override {
        delete g_syscfgMock;
        delete g_syseventMock;
        delete g_fileIOMock;
        delete g_ethSwHALMock;
        delete g_cmHALMock;
        delete g_utilMock;
        g_syscfgMock = nullptr;
        g_syseventMock = nullptr;
        g_fileIOMock = nullptr;
        g_ethSwHALMock = nullptr;
        g_cmHALMock = nullptr;
        g_utilMock = nullptr;
    }
};

TEST_F(AutowanTest, GetCurrentWanMode) {
    EXPECT_NE(-1, GetCurrentWanMode());
}

TEST_F(AutowanTest, SetCurrentWanMode) {
   EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(1).WillOnce(Return(0));
   SetCurrentWanMode(1);
}

TEST_F(AutowanTest, SetCurrentWanMode_syscfg_Failure) {
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(1).WillOnce(Return(-1));
    SetCurrentWanMode(1);
}

TEST_F(AutowanTest, SelectedWanMode) {
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(1).WillOnce(Return(0));
    SelectedWanMode(1);
}

TEST_F(AutowanTest, SelectedWanMode_syscfg_Failure) {
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(1).WillOnce(Return(-1));
    SelectedWanMode(1);
}

TEST_F(AutowanTest, SetLastKnownWanMode) {
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(1).WillOnce(Return(0));
    SetLastKnownWanMode(1);
}

TEST_F(AutowanTest, SetLastKnownWanMode_syscfg_Failure) {
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(1).WillOnce(Return(-1));
    SetLastKnownWanMode(1);
}

TEST_F(AutowanTest, WanMngrThread_WAN_MODE_AUTO) {
    g_SelectedWanMode = WAN_MODE_AUTO;

    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));

    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(testing::AtLeast(1)).WillRepeatedly(Return(0));
    EXPECT_CALL(*g_fileIOMock, popen(_, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](const char* command, const char* mode) -> FILE* {
        static FILE mockFile;
        return &mockFile;
    }));

    EXPECT_CALL(*g_fileIOMock, fgets(_, _, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](char* buf, int size, FILE* stream) -> char* {
        if (buf && size > 0) {
            strcpy(buf, "mocked_value");
            return buf;
        }
        return nullptr;
    }));

    EXPECT_CALL(*g_fileIOMock, pclose(_))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(Return(0));

    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
            if (strcmp(default_value, "wan_physical_ifname") == 0) {
                strncpy(value, "erouter0", value_len - 1);
                value[value_len - 1] = '\0';
                return 0;
            }
            else
            {
                strncpy(value, "eth0", value_len - 1);
                value[value_len - 1] = '\0';
            }
            return 0;
        }));
    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* val, int val_len) {
            if (strcmp(name, "current_wan_state") == 0) {
                strcpy(val, "up");
            } else {
                strcpy(val, "up");
            }
            return 0;
        }));

    WanMngrThread(NULL);
}

TEST_F(AutowanTest, WanMngrThread_WAN_MODE_ETH) {
    g_SelectedWanMode = WAN_MODE_ETH;
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(testing::AtLeast(1)).WillRepeatedly(Return(0));
     WanMngrThread(NULL);
}

TEST_F(AutowanTest, WanMngrThread_WAN_MODE_DOCSIS) {
    g_SelectedWanMode = WAN_MODE_DOCSIS;
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(testing::AtLeast(1)).WillRepeatedly(Return(0));
    WanMngrThread(NULL);
}

TEST_F(AutowanTest, WanMngrThread_WAN_MODE_DEFAULT) {
    g_SelectedWanMode = WAN_MODE_UNKNOWN;

    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));

    EXPECT_CALL(*g_fileIOMock, popen(_, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](const char* command, const char* mode) -> FILE* {
        static FILE mockFile;
        return &mockFile;
    }));

    EXPECT_CALL(*g_fileIOMock, fgets(_, _, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](char* buf, int size, FILE* stream) -> char* {
        if (buf && size > 0) {
            strcpy(buf, "mocked_value");
            return buf;
        }
        return nullptr;
    }));

    EXPECT_CALL(*g_fileIOMock, pclose(_))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(Return(0));

    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* val, int val_len) {
            if (strcmp(name, "current_wan_state") == 0) {
                strcpy(val, "up");
            } else {
                strcpy(val, "up");
            }
            return 0;
        }));
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(testing::AtLeast(1)).WillRepeatedly(Return(0));
    WanMngrThread(NULL);
}

TEST_F(AutowanTest, HandleAutoWanMode_WAN_MODE_UNKNOWN) {
    g_LastKnowWanMode = WAN_MODE_UNKNOWN;

    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));

    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(testing::AtLeast(1)).WillRepeatedly(Return(0));
    EXPECT_CALL(*g_fileIOMock, popen(_, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](const char* command, const char* mode) -> FILE* {
        static FILE mockFile;
        return &mockFile;
    }));

    EXPECT_CALL(*g_fileIOMock, fgets(_, _, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](char* buf, int size, FILE* stream) -> char* {
        if (buf && size > 0) {
            strcpy(buf, "mocked_value");
            return buf;
        }
        return nullptr;
    }));

    EXPECT_CALL(*g_fileIOMock, pclose(_))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(Return(0));

    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* val, int val_len) {
            if (strcmp(name, "current_wan_state") == 0) {
                strcpy(val, "up");
            } else {
                strcpy(val, "up");
            }
            return 0;
        }));
    HandleAutoWanMode();
}

TEST_F(AutowanTest, HandleAutoWanMode_WAN_MODE_DEFAULT) {
    g_LastKnowWanMode = WAN_MODE_AUTO;

    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));

    EXPECT_CALL(*g_fileIOMock, popen(_, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](const char* command, const char* mode) -> FILE* {
        static FILE mockFile;
        return &mockFile;
    }));

    EXPECT_CALL(*g_fileIOMock, fgets(_, _, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](char* buf, int size, FILE* stream) -> char* {
        if (buf && size > 0) {
            strcpy(buf, "mocked_value");
            return buf;
        }
        return nullptr;
    }));

    EXPECT_CALL(*g_fileIOMock, pclose(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(Return(0));


    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* val, int val_len) {
            if (strcmp(name, "current_wan_state") == 0) {
                strcpy(val, "up");
            } else {
                strcpy(val, "up");
            }
            return 0;
        }));
    
    EXPECT_CALL(*g_cmHALMock, docsis_IsEnergyDetected(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](unsigned char* pRfSignalStatus) {
            if (pRfSignalStatus) {
                *pRfSignalStatus = 1;
            }
            return 0;
        }));
    
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(testing::AtLeast(1)).WillRepeatedly(Return(0));
    HandleAutoWanMode();
}

TEST_F(AutowanTest, ManageWanModes_WAN_MODE_ETH) {
    int mode = WAN_MODE_ETH;

    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));

    EXPECT_CALL(*g_ethSwHALMock, GWP_GetEthWanInterfaceName(_, _))
    .Times(testing::AtLeast(1))
    .WillOnce(testing::Invoke([](unsigned char* ifname, unsigned long len) {
        strncpy(reinterpret_cast<char*>(ifname), "disable", len - 1);
        ifname[len - 1] = '\0';
        return 0;
    }));

    EXPECT_CALL(*g_cmHALMock, docsis_IsEnergyDetected(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](unsigned char* pRfSignalStatus) {
            if (pRfSignalStatus) {
                *pRfSignalStatus = 0;
            }
            return -1;
        }));

    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _)).Times(testing::AtLeast(1)).WillRepeatedly(Return(0));
    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _)).Times(testing::AtLeast(1))
     .WillRepeatedly(testing::Return(0));
    ManageWanModes(mode);
}

TEST_F(AutowanTest, ManageWanModes_WAN_MODE_DOCSIS) {
    int mode = WAN_MODE_DOCSIS;

    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));

    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_u_commit(_,_)).Times(testing::AtLeast(1)).WillRepeatedly(Return(0));
    EXPECT_CALL(*g_fileIOMock, popen(_, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](const char* command, const char* mode) -> FILE* {
        static FILE mockFile;
        return &mockFile;
    }));

    EXPECT_CALL(*g_fileIOMock, fgets(_, _, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](char* buf, int size, FILE* stream) -> char* {
        if (buf && size > 0) {
            strcpy(buf, "mocked_value");
            return buf;
        }
        return nullptr;
    }));

    EXPECT_CALL(*g_fileIOMock, pclose(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _)).Times(testing::AtLeast(1))
     .WillRepeatedly(testing::Return(0));
    ManageWanModes(mode);
}

TEST_F(AutowanTest, ManageWanModes_WAN_MODE_UNKNOWN) {
    int mode = WAN_MODE_UNKNOWN;

    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));

    EXPECT_CALL(*g_ethSwHALMock, GWP_GetEthWanInterfaceName(_, _))
    .Times(testing::AtLeast(1))
    .WillOnce(testing::Invoke([](unsigned char* ifname, unsigned long len) {
        strncpy(reinterpret_cast<char*>(ifname), "disable", len - 1);
        ifname[len - 1] = '\0';
        return 0;
    }));

    EXPECT_CALL(*g_cmHALMock, docsis_IsEnergyDetected(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](unsigned char* pRfSignalStatus) {
            if (pRfSignalStatus) {
                *pRfSignalStatus = 0;
            }
            return -1;
        }));

    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _)).Times(testing::AtLeast(1)).WillRepeatedly(Return(0));
    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _)).Times(testing::AtLeast(1))
     .WillRepeatedly(testing::Return(0));
    ManageWanModes(mode);
}

TEST_F(AutowanTest, ManageWanModes_WAN_MODE_DEFAULT) {
    int mode = WAN_MODE_AUTO;

    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));

    EXPECT_CALL(*g_ethSwHALMock, GWP_GetEthWanInterfaceName(_, _))
    .Times(testing::AtLeast(1))
    .WillOnce(testing::Invoke([](unsigned char* ifname, unsigned long len) {
        strncpy(reinterpret_cast<char*>(ifname), "disable", len - 1);
        ifname[len - 1] = '\0';
        return 0;
    }));

    EXPECT_CALL(*g_cmHALMock, docsis_IsEnergyDetected(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](unsigned char* pRfSignalStatus) {
            if (pRfSignalStatus) {
                *pRfSignalStatus = 0;
            }
            return -1;
        }));

    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _)).Times(testing::AtLeast(1)).WillRepeatedly(Return(0));
    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _)).Times(testing::AtLeast(1))
     .WillRepeatedly(testing::Return(0));
    ManageWanModes(mode);
}

TEST_F(AutowanTest, CheckWanStatus) {
    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](const char* key, const char* default_value, char* value, int value_len) {
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
    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* buff, int s_buff) {
            if (strcmp(name, "current_wan_state") == 0) {
                strcpy(buff, "up");
            } else {
                strcpy(buff, "up");
            }
            return 0;
        }));
    EXPECT_CALL(*g_fileIOMock, popen(_, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](const char* command, const char* mode) -> FILE* {
        static FILE mockFile;
        return &mockFile;
    }));

    EXPECT_CALL(*g_fileIOMock, fgets(_, _, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](char* buf, int size, FILE* stream) -> char* {
        if (buf && size > 0) {
            strcpy(buf, "mocked_value");
            return buf;
        }
        return nullptr;
    }));

    EXPECT_CALL(*g_fileIOMock, pclose(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(Return(0));


    EXPECT_EQ(2, CheckWanStatus(1));
}

TEST_F(AutowanTest, CheckWanStatus_out_value_NULL) {
    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* buff, int s_buff) {
            if (strcmp(name, "current_wan_state") == 0) {
                strcpy(buff, "up");
            } else {
                strcpy(buff, "up");
            }
            return 0;
        }));
    EXPECT_CALL(*g_fileIOMock, popen(_, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return(nullptr));

    EXPECT_EQ(1, CheckWanStatus(1));
}

TEST_F(AutowanTest, CheckWanStatus_out_value_NULL_popen_Failure_at_first_call) {
    EXPECT_CALL(*g_syscfgMock, syscfg_get(_, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* buff, int s_buff) {
            if (strcmp(name, "current_wan_state") == 0) {
                strcpy(buff, "up");
            } else {
                strcpy(buff, "up");
            }
            return 0;
        }));
    EXPECT_CALL(*g_fileIOMock, popen(_, _))
    .Times(testing::AtLeast(1))
    .WillOnce(testing::Return(nullptr))
    .WillOnce(testing::Invoke([](const char* command, const char* mode) -> FILE* {
        static FILE mockFile;
        return &mockFile;
    }));

    EXPECT_CALL(*g_fileIOMock, fgets(_, _, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](char* buf, int size, FILE* stream) -> char* {
        if (buf && size > 0) {
            strcpy(buf, "mocked_value");
            return buf;
        }
        return nullptr;
    }));

    EXPECT_CALL(*g_fileIOMock, pclose(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(Return(0));

    EXPECT_EQ(2, CheckWanStatus(1));
}

TEST_F(AutowanTest, CheckWanStatus_WAN_MODE_DOCSIS) {

    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* buff, int s_buff) {
            if (strcmp(name, "current_wan_state") == 0) {
                strcpy(buff, "up");
            } else {
                strcpy(buff, "up");
            }
            return 0;
        }));
    EXPECT_CALL(*g_fileIOMock, popen(_, _))
    .Times(testing::AtLeast(1))
    .WillOnce(testing::Return(nullptr))
    .WillOnce(testing::Invoke([](const char* command, const char* mode) -> FILE* {
        static FILE mockFile;
        return &mockFile;
    }));

    EXPECT_CALL(*g_fileIOMock, fgets(_, _, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Invoke([](char* buf, int size, FILE* stream) -> char* {
        if (buf && size > 0) {
            strcpy(buf, "mocked_value");
            return buf;
        }
        return nullptr;
    }));

    EXPECT_CALL(*g_fileIOMock, pclose(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(Return(0));

    EXPECT_EQ(0, CheckWanStatus(2));
}

TEST_F(AutowanTest, CheckWanStatus_WAN_MODE_DOCSIS_popen_Failure_both_calls) {

    EXPECT_CALL(*g_syseventMock, sysevent_get(_, _, _, _, _))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](int sysevent_fd, unsigned int sysevent_token, const char* name, char* buff, int s_buff) {
            if (strcmp(name, "current_wan_state") == 0) {
                strcpy(buff, "up");
            } else {
                strcpy(buff, "up");
            }
            return 0;
        }));
    EXPECT_CALL(*g_fileIOMock, popen(_, _))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::Return(nullptr));

    EXPECT_EQ(1, CheckWanStatus(2));
}

TEST_F(AutowanTest, TryAltWan_WAN_MODE_DOCSIS_gOvsEnable_all_scenarios) {
    int mode = WAN_MODE_DOCSIS;
    g_OvsEnable = 0;

    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));
    // Scenario 1: gOvsEnable = 0 syscfg_set_nns_commit success
    EXPECT_CALL(*g_ethSwHALMock, GWP_GetEthWanInterfaceName(_, _))
    .Times(testing::AtLeast(1))
    .WillOnce(testing::Invoke([](unsigned char* ifname, unsigned long len) {
        strncpy(reinterpret_cast<char*>(ifname), "disable", len - 1);
        ifname[len - 1] = '\0';
        return 0;
    }));
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
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_commit(_,_)).Times(testing::AtLeast(1)).WillRepeatedly(Return(-1));
    TryAltWan(&mode);

    // Scenario 2: gOvsEnable = 0 syscfg_set_nns_commit failure
    g_OvsEnable = 1;

    EXPECT_CALL(*g_ethSwHALMock, GWP_GetEthWanInterfaceName(_, _))
    .Times(testing::AtLeast(1))
    .WillOnce(testing::Invoke([](unsigned char* ifname, unsigned long len) {
        strncpy(reinterpret_cast<char*>(ifname), "disable", len - 1);
        ifname[len - 1] = '\0';
        return 1;
    }));

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
    mode = WAN_MODE_DOCSIS;
    TryAltWan(&mode);
}

TEST_F(AutowanTest, TryAltWan_WAN_MODE_AUTO) {
    int mode = WAN_MODE_AUTO;
    
    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));

    //scenario 1 : pRfSignalStatus = 0
    EXPECT_CALL(*g_cmHALMock, docsis_IsEnergyDetected(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](unsigned char* pRfSignalStatus) {
            if (pRfSignalStatus) {
                *pRfSignalStatus = 0;
            }
            return -1;
        }));

    EXPECT_CALL(*g_ethSwHALMock, GWP_GetEthWanInterfaceName(_, _))
    .Times(testing::AtLeast(1))
    .WillOnce(testing::Invoke([](unsigned char* ifname, unsigned long len) {
        strncpy(reinterpret_cast<char*>(ifname), "disable", len - 1);
        ifname[len - 1] = '\0';
        return 0;
    }));
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
    TryAltWan(&mode);

    //scenario 2 : pRfSignalStatus = 1

    EXPECT_CALL(*g_cmHALMock, docsis_IsEnergyDetected(_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke([](unsigned char* pRfSignalStatus) {
            if (pRfSignalStatus) {
                *pRfSignalStatus = 1;
            }
            return 0;
        }));

    EXPECT_CALL(*g_ethSwHALMock, GWP_GetEthWanInterfaceName(_, _))
    .Times(testing::AtLeast(1))
    .WillOnce(testing::Invoke([](unsigned char* ifname, unsigned long len) {
        strncpy(reinterpret_cast<char*>(ifname), "disable", len - 1);
        ifname[len - 1] = '\0';
        return 0;
    }));
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
    TryAltWan(&mode);
}

TEST_F(AutowanTest, RevertTriedConfig_WAN_MODE_DOCSIS) {

    //scenario 1 : g_OvsEnable = 0
    int mode = WAN_MODE_DOCSIS;
    char command[64+5];
    char ethwan_ifname[ETHWAN_INTERFACE_NAME_MAX_LENGTH] = {0};
    g_OvsEnable = 0;

    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_commit(_,_)).Times(testing::AtLeast(1)).WillOnce(Return(-1));

    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));

    EXPECT_CALL(*g_ethSwHALMock, GWP_GetEthWanInterfaceName(_, _))
    .Times(testing::AtLeast(1))
    .WillOnce(testing::Invoke([](unsigned char* ifname, unsigned long len) {
        strncpy(reinterpret_cast<char*>(ifname), "disable", len - 1);
        ifname[len - 1] = '\0';
        return 0;
    }))
    .WillOnce(testing::Invoke([](unsigned char* ifname, unsigned long len) {
        strncpy(reinterpret_cast<char*>(ifname), "enable", len - 1);
        ifname[len - 1] = '\0';
        return 1;
    }));

    RevertTriedConfig(mode);

    //scenario 2 : g_OvsEnable = 1 syscfg_set_nns_commit success

    g_OvsEnable = 1;

    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_commit(_,_)).Times(testing::AtLeast(1)).WillOnce(Return(0));

    RevertTriedConfig(mode);  
}

TEST_F(AutowanTest, AutoWan_BkupAndReboot) {

    //scenario 1 : syscfg_set_commit failure for X_RDKCENTRAL-COM_LastRebootReason and X_RDKCENTRAL-COM_LastRebootCounter
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_commit(_,_)).Times(testing::AtLeast(1))
    .WillOnce(Return(-1))
    .WillOnce(Return(-1));
    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));
    AutoWan_BkupAndReboot();

    //scenario 2 : syscfg_set_commit success for X_RDKCENTRAL-COM_LastRebootReason and X_RDKCENTRAL-COM_LastRebootCounter
    EXPECT_CALL(*g_syscfgMock, syscfg_set_nns_commit(_,_)).Times(testing::AtLeast(1))
    .WillOnce(Return(0))
    .WillOnce(Return(0));
    EXPECT_CALL(*g_utilMock, system(_)).WillRepeatedly(Return(0));
    AutoWan_BkupAndReboot();
}