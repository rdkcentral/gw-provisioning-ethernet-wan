/*
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2015 RDK Management
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
#include "gtest/gtest.h"
#include <gmock/gmock.h>
#include <mocks/mock_ethsw_hal.h>
#include <mocks/mock_syscfg.h>
#include <mocks/mock_sysevent.h>
#include <mocks/mock_platform_hal.h>
#include <mocks/mock_rdklogger.h>
#include <mocks/mock_pthread.h>
#include <mocks/mock_util.h>
#include <mocks/mock_cm_hal.h>

using namespace std;
using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::HasSubstr;
using ::testing::SetArgPointee;
using ::testing::DoAll;

extern PtdHandlerMock * g_PtdHandlerMock;
extern EthSwHalMock *g_ethSwHALMock;
extern SyscfgMock *g_syscfgMock;
extern SyseventMock *g_syseventMock;
extern PlatformHalMock *g_platformHALMock;
extern rdkloggerMock *g_rdkloggerMock;
extern UtilMock * g_utilMock;
extern CmHalMock *g_cmHALMock;