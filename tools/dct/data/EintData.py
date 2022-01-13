#! /usr/bin/python
# -*- coding: utf-8 -*-

# Copyright Statement:
#
# This software/firmware and related documentation ("MediaTek Software") are
# protected under relevant copyright laws. The information contained herein is
# confidential and proprietary to MediaTek Inc. and/or its licensors. Without
# the prior written permission of MediaTek inc. and/or its licensors, any
# reproduction, modification, use or disclosure of MediaTek Software, and
# information contained herein, in whole or in part, shall be strictly
# prohibited.
#
# MediaTek Inc. (C) 2019. All rights reserved.
#
# BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
# THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
# RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
# ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
# WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
# NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
# RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
# INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
# TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
# RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
# OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
# SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
# RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
# STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
# ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
# RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
# MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
# CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
#
# The following software/firmware and/or related documentation ("MediaTek
# Software") have been modified by MediaTek Inc. All revisions are subject to
# any receiver's applicable license agreements with MediaTek Inc.

class EintData:
    _count = 0
    _debounce_enable_list = ['CUST_EINT_DEBOUNCE_DISABLE', 'CUST_EINT_DEBOUNCE_ENABLE']
    _map_table = {}
    _mode_map = {}
    _int_eint = {}
    _builtin_map = {}
    _builtin_eint_count = 0
    def __init__(self):
        self.__varName = ''
        self.__debounce_time = ''
        self.__polarity = ''
        self.__sensitive_level = ''
        self.__debounce_enable = ''

    def set_varName(self, varName):
        self.__varName = varName

    def get_varName(self):
        return self.__varName

    def set_debounceTime(self, time):
        self.__debounce_time = time

    def get_debounceTime(self):
        return self.__debounce_time

    def set_polarity(self, polarity):
        self.__polarity = polarity

    def get_polarity(self):
        return self.__polarity

    def set_sensitiveLevel(self, level):
        self.__sensitive_level = level

    def get_sensitiveLevel(self):
        return self.__sensitive_level

    def set_debounceEnable(self, enable):
        self.__debounce_enable = enable

    def get_debounceEnable(self):
        return self.__debounce_enable

    @staticmethod
    def set_mapTable(map):
        EintData._map_table = map

    @staticmethod
    def get_mapTable():
        return EintData._map_table

    @staticmethod
    def get_internalEint():
        return EintData._int_eint

    @staticmethod
    def get_modeName(gpio_num, mode_idx):
        key = 'gpio%s' %(gpio_num)

        if key in EintData._mode_map.keys():
            list =  EintData._mode_map[key]
            if mode_idx < len(list) and mode_idx >= 0:
                return list[mode_idx]

        return None

    @staticmethod
    def set_modeMap(map):
        for (key, value) in map.items():
            list = []
            for item in value:
                list.append(item[6:len(item)-1])
            map[key] = list

        EintData._mode_map = map

    @staticmethod
    def get_modeMap():
        return EintData._mode_map

    @staticmethod
    def get_gpioNum(num):
        if len(EintData._map_table):
            for (key,value) in EintData._map_table.items():
                if num == value:
                    return key

        return -1

