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

import sys,os
import re
import string
import ConfigParser
import xml.dom.minidom

import ChipObj
from data.PowerData import PowerData
from utility.util import log
from utility.util import LogLevel
from utility.util import sorted_key
from ModuleObj import ModuleObj

class PowerObj(ModuleObj):
    def __init__(self):
        ModuleObj.__init__(self, 'cust_power.h', 'cust_power.dtsi')
        self.__list = {}

    def getCfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        cp.read(ModuleObj.get_figPath())

        self.__list = cp.options('POWER')
        self.__list = self.__list[1:]
        self.__list = sorted(self.__list)

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType == xml.dom.Node.ELEMENT_NODE:
                if node.nodeName == 'count':
                    continue

                varNode = node.getElementsByTagName('varName')

                data = PowerData()
                data.set_varName(varNode[0].childNodes[0].nodeValue)

                ModuleObj.set_data(self, node.nodeName, data)

        return True

    def parse(self, node):
        self.getCfgInfo()
        self.read(node)

    def gen_files(self):
        ModuleObj.gen_files(self)

    def gen_spec(self, para):
        if re.match(r'.*_h$', para):
            self.gen_hFile()

    # power module has no DTSI file
    def gen_dtsiFile(self):
        pass

    def fill_hFile(self):
        gen_str = ''
        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if value.get_varName() == '':
                continue
            idx = string.atoi(key[5:])
            name = self.__list[idx]
            gen_str += '''#define GPIO_%s\t\tGPIO_%s\n''' %(name.upper(), value.get_varName())

        return gen_str

    def fill_dtsiFile(self):
        return ''
