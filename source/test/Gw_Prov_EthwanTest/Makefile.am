##########################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2015 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
AM_CFLAGS = -fPIC -fno-exceptions -Os -Wall -Wextra

AM_CXXFLAGS = -std=c++11

ACLOCAL_AMFLAGS = -I m4

AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = Gw_Prov_Ethwan_gtest.bin
Gw_Prov_Ethwan_gtest_bin_CPPFLAGS = -I$(PKG_CONFIG_SYSROOT_DIR)$(includedir)/gtest \
                               -I${top_srcdir}/gtest/include \
                               -I${top_srcdir}/source \
                               -I${top_srcdir}/source/include \
                               -I${PKG_CONFIG_SYSROOT_DIR}$(includedir) \
                               -DUNIT_TEST_DOCKER_SUPPORT \
                               -DFEATURE_SUPPORT_RDKLOG \
                               -DCONFIG_CISCO_HOME_SECURITY \
                               -D_2_5G_ETHERNET_SUPPORT_ \
                               -DAUTOWAN_ENABLE \
                               -D_BRIDGE_UTILS_BIN_
Gw_Prov_Ethwan_gtest_bin_SOURCES = Gw_Prov_EthwanTest.cpp \
                                   Autowan_Gtest.cpp \
                                   ../mocks/mock_gwprovabs.cpp \
                                   Gw_Prov_Ethwan_Mock.cpp \
                                  gtest_main.cpp \
                                  $(top_builddir)/source/gw_prov_ethwan_sm.c \
                                  $(top_builddir)/source/autowan.c
Gw_Prov_Ethwan_gtest_bin_LDADD = $(HOME)/usr/local/lib/libmock_ethsw_hal.la \
                                                                  $(HOME)/usr/local/lib/libmock_syscfg.la \
                                                                  $(HOME)/usr/local/lib/libmock_sysevent.la \
                                                                 $(HOME)/usr/local/lib/libmock_platform_hal.la \
                                                                 $(HOME)/usr/local/lib/libmock_rdklogger.la \
                                                                 $(HOME)/usr/local/lib/libmock_util.la \
                                                                 $(HOME)/usr/local/lib/libmock_cm_hal.la \
                                                                 $(HOME)/usr/local/lib/libmock_file_io.la
Gw_Prov_Ethwan_gtest_bin_LDFLAGS = -lgtest -lgmock -lgcov -lpthread
AM_CFLAGS += -fprofile-arcs -ftest-coverage
AM_CXXFLAGS += -fprofile-arcs -ftest-coverage
Gw_Prov_Ethwan_gtest_bin_LDFLAGS += --coverage