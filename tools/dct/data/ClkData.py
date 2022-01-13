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

class ClkData:
    _varList = ['DISABLE', 'SW_CONTROL', 'HW_CONTROL']
    _count = 0

    def __init__(self):
        self.__varName = ''

    def set_defVarName(self, idx):
        self.__varName = self._varList[idx]

    def set_varName(self, name):
        self.__varName = name

    def get_varName(self):
        return self.__varName

    def set_varNameList(self, varNameList):
        self._varList = varNameList


class OldClkData(ClkData):
    def __init__(self):
        ClkData.__init__(self)
        self.__current = ''
        self.__curList = []

    def set_defCurrent(self, idx):
        self.__current = self.__curList[idx]

    def set_current(self, current):
        self.__current = current

    def get_current(self):
        return self.__current

    def set_curList(self, cur_list):
        self.__curList = cur_list

    def get_curList(self):
        return self.__curList


class NewClkData(ClkData):
    def __init__(self):
        ClkData.__init__(self)
        self.__cur_buf_output = ""
        self.__cur_buf_output_list = []
        self.__cur_driving_control = ""
        self.__cur_driving_control_list = []

    def set_def_buf_output(self, index):
        self.__cur_buf_output = self.cur_buf_output_list[index]

    @property
    def cur_buf_output(self):
        return self.__cur_buf_output

    @cur_buf_output.setter
    def cur_buf_output(self, value):
        self.__cur_buf_output = value

    @property
    def cur_buf_output_list(self):
        return self.__cur_buf_output_list

    @cur_buf_output_list.setter
    def cur_buf_output_list(self, value):
        self.__cur_buf_output_list = value

    def set_def_driving_control(self, index):
        self.__cur_driving_control = self.cur_driving_control_list[index]

    @property
    def cur_driving_control(self):
        return self.__cur_driving_control

    @cur_driving_control.setter
    def cur_driving_control(self, value):
        self.__cur_driving_control = value

    @property
    def cur_driving_control_list(self):
        return self.__cur_driving_control_list

    @cur_driving_control_list.setter
    def cur_driving_control_list(self, value):
        self.__cur_driving_control_list = value
