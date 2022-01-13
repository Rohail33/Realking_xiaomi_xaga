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

class KpdData:
    _row = -1
    _col = -1
    _row_ext = -1
    _col_ext = -1
    _gpioNum = -1
    _util = ''
    _homeKey = ''
    _keyType = ''
    _pressTime = -1
    _dinHigh = False
    _matrix = []
    _matrix_ext = []
    _useEint = False
    _downloadKeys = []
    _keyValueMap = {}
    _usedKeys = []
    _modeKeys = {'META':None, 'RECOVERY':None, 'FACTORY':None}

    def __init__(self):
        self.__varNames = []

    @staticmethod
    def set_row(row):
        KpdData._row = row

    @staticmethod
    def get_row():
        return KpdData._row

    @staticmethod
    def set_col(col):
        KpdData._col = col

    @staticmethod
    def get_col():
        return KpdData._col

    @staticmethod
    def set_row_ext(row):
        KpdData._row_ext = row

    @staticmethod
    def get_row_ext():
        return KpdData._row_ext

    @staticmethod
    def set_col_ext(col):
        KpdData._col_ext = col

    @staticmethod
    def get_col_ext():
        return KpdData._col_ext

    @staticmethod
    def set_matrix(matrix):
        KpdData._matrix = matrix

    @staticmethod
    def set_matrix_ext(matrix):
        KpdData._matrix_ext = matrix

    @staticmethod
    def get_matrix_ext():
        return KpdData._matrix_ext

    @staticmethod
    def get_matrix():
        return KpdData._matrix

    @staticmethod
    def set_downloadKeys(keys):
        KpdData._downloadKeys = keys

    @staticmethod
    def get_downloadKeys():
        return KpdData._downloadKeys

    @staticmethod
    def get_modeKeys():
        return KpdData._modeKeys

    @staticmethod
    def set_gpioNum(num):
        KpdData._gpioNum = num

    @staticmethod
    def get_gpioNum():
        return KpdData._gpioNum

    @staticmethod
    def set_utility(util):
        KpdData._util = util

    @staticmethod
    def get_utility():
        return KpdData._util

    @staticmethod
    def set_homeKey(home):
        KpdData._homeKey = home

    @staticmethod
    def get_homeKey():
        return KpdData._homeKey

    @staticmethod
    def set_useEint(flag):
        KpdData._useEint = flag

    @staticmethod
    def getUseEint():
        return KpdData._useEint

    @staticmethod
    def set_gpioDinHigh(flag):
        KpdData._dinHigh = flag

    @staticmethod
    def get_gpioDinHigh():
        return KpdData._dinHigh

    @staticmethod
    def set_pressTime(time):
        KpdData._pressTime = time

    @staticmethod
    def get_pressTime():
        return KpdData._pressTime

    @staticmethod
    def set_keyType(keyType):
        KpdData._keyType = keyType

    @staticmethod
    def get_keyType():
        return KpdData._keyType

    @staticmethod
    def get_keyVal(key):
        if key in KpdData._keyValueMap.keys():
            return KpdData._keyValueMap[key]

        return 0

