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

import os
import re
import string
import ConfigParser

import xml.dom.minidom

from ModuleObj import ModuleObj
from data.ClkData import ClkData
from data.ClkData import OldClkData
from data.ClkData import NewClkData
from utility.util import log
from utility.util import LogLevel
from utility.util import sorted_key

DEFAULT_AUTOK = 'AutoK'
class ClkObj(ModuleObj):
    def __init__(self):
        ModuleObj.__init__(self, 'cust_clk_buf.h', 'cust_clk_buf.dtsi')
        #self.__prefix_cfg = 'driving_current_pmic_clk_buf'
        self._suffix = '_BUF'
        self.__count = -1

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType == xml.dom.Node.ELEMENT_NODE:
                if node.nodeName == 'count':
                    continue

                varNode = node.getElementsByTagName('varName')
                curNode = node.getElementsByTagName('current')

                key = re.findall(r'\D+', node.nodeName)[0].upper() + self._suffix + '%s' %(re.findall(r'\d+', node.nodeName)[0])

                if key not in ModuleObj.get_data(self):
	                continue;

                data = ModuleObj.get_data(self)[key]

                if len(varNode):
                    data.set_varName(varNode[0].childNodes[0].nodeValue)

                if len(curNode):
                    data.set_current(curNode[0].childNodes[0].nodeValue)

                ModuleObj.set_data(self, key, data)

        return True

    def get_cfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        cp.read(ModuleObj.get_figPath())

        count = string.atoi(cp.get('CLK_BUF', 'CLK_BUF_COUNT'))
        self.__count = count

        ops = cp.options('CLK_BUF')
        for op in ops:
            if op == 'clk_buf_count':
                self.__count = string.atoi(cp.get('CLK_BUF', op))
                ClkData._count = string.atoi(cp.get('CLK_BUF', op))
                continue

            value = cp.get('CLK_BUF', op)
            var_list = value.split(':')

            data = OldClkData()
            data.set_curList(var_list[2:])
            data.set_defVarName(string.atoi(var_list[0]))
            data.set_defCurrent(string.atoi(var_list[1]))

            key = op[16:].upper()
            ModuleObj.set_data(self, key, data)

    def parse(self, node):
        self.get_cfgInfo()
        self.read(node)

    def gen_files(self):
        ModuleObj.gen_files(self)

    def fill_hFile(self):
        gen_str = '''typedef enum {\n'''
        gen_str += '''\tCLOCK_BUFFER_DISABLE,\n'''
        gen_str += '''\tCLOCK_BUFFER_SW_CONTROL,\n'''
        gen_str += '''\tCLOCK_BUFFER_HW_CONTROL\n'''
        gen_str += '''} MTK_CLK_BUF_STATUS;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_AUTO_K = -1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_0,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_2,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_3\n'''
        gen_str += '''} MTK_CLK_BUF_DRIVING_CURR;\n'''
        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            gen_str += '''#define %s_STATUS_PMIC\t\tCLOCK_BUFFER_%s\n''' %(key[5:], value.get_varName().upper())

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            idx = value.get_curList().index(value.get_current())
            if cmp(value.get_curList()[0], DEFAULT_AUTOK) == 0:
                idx -= 1

            if idx >= 0:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_%d\n''' %(key, idx)
            else:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_AUTO_K\n''' %(key)

        gen_str += '''\n'''

        return gen_str

    def fill_dtsiFile(self):
        gen_str = '''&pmic_clock_buffer_ctrl {\n'''
        gen_str += '''\tmediatek,clkbuf-quantity = <%d>;\n''' %(self.__count)
        gen_str += '''\tmediatek,clkbuf-config = <'''

        #sorted_list = sorted(ModuleObj.get_data(self).keys())
        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('PMIC') == -1:
                continue
            value = ModuleObj.get_data(self)[key]
            gen_str += '''%d ''' %(ClkData._varList.index(value.get_varName()))

        gen_str = gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tmediatek,clkbuf-driving-current = <'''

        #sorted_list = sorted(ModuleObj.get_data(self).keys())
        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('PMIC') == -1:
                continue
            value = ModuleObj.get_data(self)[key]
            idx = value.get_curList().index(value.get_current())
            if cmp(value.get_curList()[0], DEFAULT_AUTOK) == 0:
                idx -= 1
            if idx < 0:
                gen_str += '''(%d) ''' %(-1)
            else:
                gen_str += '''%d ''' %(idx)

        gen_str = gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n'''

        return gen_str


class ClkObj_MT6797(ClkObj):
    def __init__(self):
        ClkObj.__init__(self)
        self.__rf = 'RF'
        self.__pmic = 'PMIC'

    def parse(self, node):
        ClkObj.parse(self, node)

    def gen_files(self):
        ClkObj.gen_files(self)

    def fill_hFile(self):
        gen_str = '''typedef enum {\n'''
        gen_str += '''\tCLOCK_BUFFER_DISABLE,\n'''
        gen_str += '''\tCLOCK_BUFFER_SW_CONTROL,\n'''
        gen_str += '''\tCLOCK_BUFFER_HW_CONTROL\n'''
        gen_str += '''} MTK_CLK_BUF_STATUS;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_0_4MA,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_0_9MA,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_1_4MA,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_1_9MA\n'''
        gen_str += '''} MTK_CLK_BUF_DRIVING_CURR;\n'''
        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find(self.__pmic) != -1:
                gen_str += '''#define %s_STATUS_PMIC\t\t\t\tCLOCK_BUFFER_%s\n''' %(key[5:], value.get_varName())

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find(self.__pmic) != -1:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_%sMA\n''' %(key, value.get_current().replace('.', '_'))

        gen_str += '''\n'''


        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find(self.__rf) != -1:
                gen_str += '''#define %s_STATUS\t\tCLOCK_BUFFER_%s\n''' %(key[3:], value.get_varName())

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find(self.__rf) != -1:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_%sMA\n''' %(key, value.get_current().replace('.', '_'))

        gen_str += '''\n'''



        return gen_str

    def fill_dtsiFile(self):
        gen_str = ClkObj.fill_dtsiFile(self)

        gen_str += '''\n'''

        gen_str += '''&rf_clock_buffer_ctrl {\n'''
        gen_str += '''\tmediatek,clkbuf-quantity = <%d>;\n''' %(len(ModuleObj.get_data(self))-ClkData._count)
        msg = 'rf clk buff count : %d' %(len(ModuleObj.get_data(self))-ClkData._count)
        log(LogLevel.info, msg)
        gen_str += '''\tmediatek,clkbuf-config = <'''

        #sorted_list = sorted(ModuleObj.get_data(self).keys())

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]

            if key.find(self.__rf) != -1:
                gen_str += '''%d ''' %(ClkData._varList.index(value.get_varName()))
        gen_str = gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tmediatek,clkbuf-driving-current = <'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find(self.__rf) != -1:
                idx = value.get_curList().index(value.get_current())
                if cmp(value.get_curList()[0], DEFAULT_AUTOK) == 0:
                    idx -= 1
                gen_str += '''%d ''' %(idx)

        gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n'''

        return gen_str

class ClkObj_MT6757(ClkObj_MT6797):

    def __init__(self):
        ClkObj_Everest.__init__(self)

    def get_cfgInfo(self):
        ClkObj_Everest.get_cfgInfo(self)

    def parse(self, node):
        ClkObj_Everest.parse(self, node)

    def gen_files(self):
        ClkObj_Everest.gen_files(self)

    def fill_hFile(self):
        gen_str = '''typedef enum {\n'''
        gen_str += '''\tCLOCK_BUFFER_DISABLE,\n'''
        gen_str += '''\tCLOCK_BUFFER_SW_CONTROL,\n'''
        gen_str += '''\tCLOCK_BUFFER_HW_CONTROL\n'''
        gen_str += '''} MTK_CLK_BUF_STATUS;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_AUTO_K = -1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_0,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_2,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_3\n'''
        gen_str += '''} MTK_CLK_BUF_DRIVING_CURR;\n'''
        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find('PMIC') != -1:
                gen_str += '''#define %s_STATUS_PMIC\t\tCLOCK_BUFFER_%s\n''' %(key[5:], value.get_varName())

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find('RF') != -1:
                gen_str += '''#define %s_STATUS\t\t\t\tCLOCK_BUFFER_%s\n''' %(key[3:], value.get_varName())

        gen_str += '''\n'''


        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('PMIC') != -1:
                continue
            value = ModuleObj.get_data(self)[key]
            idx = value.get_curList().index(value.get_current())
            if cmp(value.get_curList()[0], DEFAULT_AUTOK) == 0:
                idx -= 1

            if idx >= 0:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_%d\n''' %(key, idx)
            else:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_AUTO_K\n''' %(key)

        gen_str += '''\n'''


        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('RF') != -1:
                continue
            value = ModuleObj.get_data(self)[key]
            idx = value.get_curList().index(value.get_current())
            if cmp(value.get_curList()[0], DEFAULT_AUTOK) == 0:
                idx -= 1

            if idx >= 0:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_%d\n''' %(key, idx)
            else:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_AUTO_K\n''' %(key)

        gen_str += '''\n'''

        return gen_str

class ClkObj_MT6570(ClkObj):

    def __init__(self):
        ClkObj.__init__(self)

    def parse(self, node):
        ClkObj.parse(self, node)

    def get_cfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        cp.read(ModuleObj.get_figPath())

        count = string.atoi(cp.get('CLK_BUF', 'CLK_BUF_COUNT'))
        self.__count = count

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType == xml.dom.Node.ELEMENT_NODE:
                if node.nodeName == 'count':
                    continue

                varNode = node.getElementsByTagName('varName')
                curNode = node.getElementsByTagName('current')

                key = re.findall(r'\D+', node.nodeName)[0].upper() + self._suffix + '%s' %(re.findall(r'\d+', node.nodeName)[0])
                data = OldClkData()
                if len(varNode):
                    data.set_varName(varNode[0].childNodes[0].nodeValue)

                #if len(curNode):
                    #data.set_current(curNode[0].childNodes[0].nodeValue)

                ModuleObj.set_data(self, key, data)

        return True

    def fill_hFile(self):
        gen_str = '''typedef enum {\n'''
        gen_str += '''\tCLOCK_BUFFER_DISABLE,\n'''
        gen_str += '''\tCLOCK_BUFFER_SW_CONTROL,\n'''
        gen_str += '''\tCLOCK_BUFFER_HW_CONTROL\n'''
        gen_str += '''} MTK_CLK_BUF_STATUS;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_AUTO_K = -1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_0,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_2,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURR_3\n'''
        gen_str += '''} MTK_CLK_BUF_DRIVING_CURR;\n'''
        gen_str += '''\n'''


        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if key.find('RF') != -1:
                gen_str += '''#define %s_STATUS\t\t\t\tCLOCK_BUFFER_%s\n''' %(key[3:], value.get_varName())

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('RF') != -1:
                continue
            value = ModuleObj.get_data(self)[key]
            idx = value.get_curList().index(value.get_current())
            if cmp(value.get_curList()[0], DEFAULT_AUTOK) == 0:
                idx -= 1

            if idx >= 0:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_%d\n''' %(key, idx)
            else:
                gen_str += '''#define %s_DRIVING_CURR\t\tCLK_BUF_DRIVING_CURR_AUTO_K\n''' %(key)

        gen_str += '''\n'''

        return gen_str

    def fill_dtsiFile(self):
        gen_str = '''&rf_clock_buffer_ctrl {\n'''
        gen_str += '''\tmediatek,clkbuf-quantity = <%d>;\n''' %(self.__count)
        gen_str += '''\tmediatek,clkbuf-config = <'''

        #sorted_list = sorted(ModuleObj.get_data(self).keys())
        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('RF') == -1:
                continue
            value = ModuleObj.get_data(self)[key]
            gen_str += '''%d ''' %(ClkData._varList.index(value.get_varName()))

        gen_str = gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n'''

        return gen_str


class ClkObj_MT6779(ClkObj):
    def __init__(self):
        ClkObj.__init__(self)
        self.__bbck_buf_cout = 0

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType == xml.dom.Node.ELEMENT_NODE:
                if node.nodeName == 'count' or node.nodeName == 'bbck_buf_count':
                    continue

                key = re.findall(r'\D+', node.nodeName)[0].upper() + self._suffix + '%s' % (re.findall(r'\d+', node.nodeName)[0])

                if key not in ModuleObj.get_data(self):
                    continue

                data = ModuleObj.get_data(self)[key]

                var_name_node = node.getElementsByTagName('varName')
                cur_buf_node = node.getElementsByTagName('cur_buf_output')
                cur_driving_node = node.getElementsByTagName('cur_driving_control')

                if len(var_name_node):
                    data.set_varName(var_name_node[0].childNodes[0].nodeValue)
                if len(cur_buf_node):
                    data.cur_buf_output = cur_buf_node[0].childNodes[0].nodeValue
                if len(cur_driving_node):
                    data.cur_driving_control = cur_driving_node[0].childNodes[0].nodeValue

                ModuleObj.set_data(self, key, data)

        return True

    def get_cfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        cp.read(ModuleObj.get_figPath())

        max_count = self.get_max_count(cp)
        self.__count = max_count
        ClkData._count = max_count

        ops = cp.options('CLK_BUF')
        for op in ops:
            if op == 'clk_buf_count' or op == 'bbck_buf_count':
                continue

            value = cp.get('CLK_BUF', op)
            var_list = value.split(r'/')

            data = NewClkData()
            data.set_defVarName(string.atoi(var_list[0]))

            buf_output_list = var_list[1].split(r":")
            # only -1 means no data
            if len(buf_output_list) > 1:
                data.cur_buf_output_list = buf_output_list[1:]
                data.set_def_buf_output(string.atoi(buf_output_list[0]))

            driving_control_list = var_list[2].split(r":")
            # only -1 means no data
            if len(driving_control_list) > 1:
                data.cur_driving_control_list = driving_control_list[1:]
                data.set_def_driving_control(string.atoi(driving_control_list[0]))

            key = op[16:].upper()
            ModuleObj.set_data(self, key, data)

        # generate some dummy data, used for generating dtsi file
        for i in range(max_count):
            key = "PMIC_CLK_BUF" + "%s" % (i + 1)
            if key not in ModuleObj.get_data(self).keys():
                data = NewClkData()
                ModuleObj.set_data(self, key, data)

    def fill_hFile(self):
        gen_str = '''typedef enum {\n'''
        gen_str += '''\tCLOCK_BUFFER_DISABLE,\n'''
        gen_str += '''\tCLOCK_BUFFER_SW_CONTROL,\n'''
        gen_str += '''\tCLOCK_BUFFER_HW_CONTROL\n'''
        gen_str += '''} MTK_CLK_BUF_STATUS;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_0,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_1,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_2,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_3,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_4,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_5,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_6,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_7\n'''
        gen_str += '''} MTK_CLK_BUF_OUTPUT_IMPEDANCE;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_CONTROLS_FOR_DESENSE_0,\n'''
        gen_str += '''\tCLK_BUF_CONTROLS_FOR_DESENSE_1,\n'''
        gen_str += '''\tCLK_BUF_CONTROLS_FOR_DESENSE_2,\n'''
        gen_str += '''\tCLK_BUF_CONTROLS_FOR_DESENSE_3,\n'''
        gen_str += '''\tCLK_BUF_CONTROLS_FOR_DESENSE_4,\n'''
        gen_str += '''\tCLK_BUF_CONTROLS_FOR_DESENSE_5,\n'''
        gen_str += '''\tCLK_BUF_CONTROLS_FOR_DESENSE_6,\n'''
        gen_str += '''\tCLK_BUF_CONTROLS_FOR_DESENSE_7\n'''
        gen_str += '''} MTK_CLK_BUF_CONTROLS_FOR_DESENSE;\n'''
        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if len(value.get_varName()):
                gen_str += '''#define %s_STATUS_PMIC\t\tCLOCK_BUFFER_%s\n''' % (key[5:], value.get_varName().upper())

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if len(value.cur_buf_output_list) and len(value.cur_buf_output):
                idx = value.cur_buf_output_list.index(value.cur_buf_output)
                gen_str += '''#define %s_OUTPUT_IMPEDANCE\t\tCLK_BUF_OUTPUT_IMPEDANCE_%d\n''' % (key, idx)

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if len(value.cur_driving_control_list) and len(value.cur_driving_control):
                idx = value.cur_driving_control_list.index(value.cur_driving_control)
                gen_str += '''#define %s_CONTROLS_FOR_DESENSE\t\tCLK_BUF_CONTROLS_FOR_DESENSE_%d\n''' % (key, idx)

        gen_str += '''\n'''

        return gen_str

    def fill_dtsiFile(self):
        gen_str = '''&pmic_clock_buffer_ctrl {\n'''
        gen_str += '''\tmediatek,clkbuf-quantity = <%d>;\n''' % self.__count
        gen_str += '''\tmediatek,clkbuf-config = <'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('PMIC') == -1:
                continue
            value = ModuleObj.get_data(self)[key]
            if len(value.get_varName()):
                gen_str += '''%d ''' % (ClkData._varList.index(value.get_varName()))
            else:
                gen_str += '''%d ''' % 0

        gen_str = gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tmediatek,clkbuf-output-impedance = <'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('PMIC') == -1:
                continue
            value = ModuleObj.get_data(self)[key]
            if len(value.cur_buf_output_list) and len(value.cur_buf_output):
                idx = value.cur_buf_output_list.index(value.cur_buf_output)
                gen_str += '''%d ''' % idx
            else:
                gen_str += '''%d ''' % 0

        gen_str = gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tmediatek,clkbuf-controls-for-desense = <'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            if key.find('PMIC') == -1:
                continue
            value = ModuleObj.get_data(self)[key]
            if len(value.cur_driving_control_list) and len(value.cur_driving_control):
                idx = value.cur_driving_control_list.index(value.cur_driving_control)
                gen_str += '''%d ''' % idx
            else:
                gen_str += '''%d ''' % 0

        gen_str = gen_str.rstrip()
        gen_str += '''>;\n'''

        gen_str += '''\tstatus = \"okay\";\n'''
        gen_str += '''};\n'''

        return gen_str

    @staticmethod
    def get_max_count(fig):
        if fig.has_section("CLK_BUF_EX_PIN"):
            max_count = -1
            options = fig.options("CLK_BUF_EX_PIN")
            for option in options:
                cur_count = fig.getint("CLK_BUF_EX_PIN", option)
                max_count = max(cur_count, max_count)
            return max_count
        else:
            return fig.getint('CLK_BUF', 'CLK_BUF_COUNT')

class ClkObj_MT6879(ClkObj_MT6779):
    def __init__(self):
        ClkObj_MT6779.__init__(self)
        self.__bbck_count = 5

    def fill_hFile(self):
        gen_str = '''typedef enum {\n'''
        gen_str += '''\tCLOCK_BUFFER_DISABLE,\n'''
        gen_str += '''\tCLOCK_BUFFER_SW_CONTROL,\n'''
        gen_str += '''\tCLOCK_BUFFER_HW_CONTROL\n'''
        gen_str += '''} MTK_CLK_BUF_STATUS;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_0,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_1,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_2,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_3,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_4,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_5,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_6,\n'''
        gen_str += '''\tCLK_BUF_OUTPUT_IMPEDANCE_7\n'''
        gen_str += '''} MTK_CLK_BUF_OUTPUT_IMPEDANCE;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_DRIVING_STRENGTH_0,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_STRENGTH_1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_STRENGTH_2,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_STRENGTH_3,\n'''
        gen_str += '''} MTK_CLK_BUF_DRVING_STRENGTH;\n'''
        gen_str += '''\n'''

        index = 1

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if index <= 5:
                if len(value.get_varName()):
                    gen_str += '''#define BBCK_BUF%d_STATUS_PMIC\t\tCLOCK_BUFFER_%s\n''' % (index, value.get_varName().upper())
                if len(value.cur_buf_output_list) and len(value.cur_buf_output):
                    idx = value.cur_buf_output_list.index(value.cur_buf_output)
                    gen_str += '''#define BBCK_BUF%d_OUTPUT_IMPEDANCE\t\tCLK_BUF_OUTPUT_IMPEDANCE_%d\n''' % (index, idx+4)
                if len(value.cur_driving_control_list) and len(value.cur_driving_control):
                    idx = value.cur_driving_control_list.index(value.cur_driving_control)
                    gen_str += '''#define BBCK_BUF%d_DRIVING_STRENGTH\t\tCLK_BUF_DRIVING_STRENGTH_%d\n''' % (index, idx)

                gen_str += '''\n'''
            else:
                if index == 6:
                    if len(value.get_varName()):
                        gen_str += '''#define RFCK_BUF1A_STATUS_PMIC\t\tCLOCK_BUFFER_%s\n''' % (value.get_varName().upper())
                    if len(value.cur_buf_output_list) and len(value.cur_buf_output):
                        idx = value.cur_buf_output_list.index(value.cur_buf_output)
                        gen_str += '''#define RFCK_BUF1A_OUTPUT_IMPEDANCE\t\tCLK_BUF_OUTPUT_IMPEDANCE_%d\n''' % (idx)
                    gen_str += '''\n'''
                elif index == 7:
                    if len(value.get_varName()):
                        gen_str += '''#define RFCK_BUF1B_STATUS_PMIC\t\tCLOCK_BUFFER_%s\n''' % (value.get_varName().upper())
                    if len(value.cur_buf_output_list) and len(value.cur_buf_output):
                        idx = value.cur_buf_output_list.index(value.cur_buf_output)
                        gen_str += '''#define RFCK_BUF1B_OUTPUT_IMPEDANCE\t\tCLK_BUF_OUTPUT_IMPEDANCE_%d\n''' % (idx)
                    gen_str += '''\n'''
                elif index == 8:
                    if len(value.get_varName()):
                        gen_str += '''#define RFCK_BUF2A_STATUS_PMIC\t\tCLOCK_BUFFER_%s\n''' % (value.get_varName().upper())
                    if len(value.cur_buf_output_list) and len(value.cur_buf_output):
                        idx = value.cur_buf_output_list.index(value.cur_buf_output)
                        gen_str += '''#define RFCK_BUF2A_OUTPUT_IMPEDANCE\t\tCLK_BUF_OUTPUT_IMPEDANCE_%d\n''' % (idx)
                    gen_str += '''\n'''
                elif index == 9:
                    if len(value.get_varName()):
                        gen_str += '''#define RFCK_BUF2B_STATUS_PMIC\t\tCLOCK_BUFFER_%s\n''' % (value.get_varName().upper())
                    if len(value.cur_buf_output_list) and len(value.cur_buf_output):
                        idx = value.cur_buf_output_list.index(value.cur_buf_output)
                        gen_str += '''#define RFCK_BUF2B_OUTPUT_IMPEDANCE\t\tCLK_BUF_OUTPUT_IMPEDANCE_%d\n''' % (idx)
                    gen_str += '''\n'''
            index = index + 1

        gen_str += '''\n'''

        return gen_str

    def fill_dtsiFile(self):
        return '';


class ClkObj_MT6789(ClkObj):
    def __init__(self):
        ClkObj.__init__(self)

    def get_cfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        cp.read(ModuleObj.get_figPath())

        hw_control_split = 0
        if cp.has_option(r"Chip Type", r"CLK_BUF_HW_CONTROL_SPLIT"):
            hw_control_split = cp.getint(r"Chip Type", r"CLK_BUF_HW_CONTROL_SPLIT")

        ops = cp.options('CLK_BUF')
        for op in ops:
            if op == 'clk_buf_count':
                self.__count = string.atoi(cp.get('CLK_BUF', op))
                ClkData._count = string.atoi(cp.get('CLK_BUF', op))
                continue

            value = cp.get('CLK_BUF', op)
            var_list = value.split(':')

            data = OldClkData()
            if hw_control_split != 0:
                data.set_varNameList(['DISABLE', 'SW_CONTROL', 'HW_CONTROL1', 'HW_CONTROL2', 'HW_CONTROL3'])
            data.set_curList(var_list[2:])
            data.set_defVarName(string.atoi(var_list[0]))
            data.set_defCurrent(string.atoi(var_list[1]))

            key = op[16:].upper()
            ModuleObj.set_data(self, key, data)

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType != xml.dom.Node.ELEMENT_NODE:
                continue
            if node.nodeName == 'count':
                continue

            varNode = node.getElementsByTagName('varName')
            strengthNode = node.getElementsByTagName('buf_output_strenght')

            key = re.findall(r'\D+', node.nodeName)[0].upper() + self._suffix + '%s' % (re.findall(r'\d+', node.nodeName)[0])

            if key not in ModuleObj.get_data(self):
                continue

            data = ModuleObj.get_data(self)[key]

            if len(varNode):
                data.set_varName(varNode[0].childNodes[0].nodeValue)

            if len(strengthNode):
                data.set_current(strengthNode[0].childNodes[0].nodeValue)

            ModuleObj.set_data(self, key, data)

        return True

    def fill_hFile(self):
        gen_str = '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_DISABLE,\n'''
        gen_str += '''\tCLK_BUF_SW_CONTROL,\n'''
        gen_str += '''\tCLK_BUF_HW_CONTROL1,\n'''
        gen_str += '''\tCLK_BUF_HW_CONTROL2,\n'''
        gen_str += '''\tCLK_BUF_HW_CONTROL3\n'''
        gen_str += '''} MTK_CLK_BUF_STATUS;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_DRIVING_STRENGTH_0,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_STRENGTH_1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_STRENGTH_2,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_STRENGTH_3,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_STRENGTH_MAX\n'''
        gen_str += '''} MTK_CLK_BUF_DRVING_STRENGTH;\n'''
        gen_str += '''\n'''

        gen_str += '''typedef enum {\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURRENT_0,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURRENT_1,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURRENT_2,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURRENT_3,\n'''
        gen_str += '''\tCLK_BUF_DRIVING_CURRENT_MAX\n'''
        gen_str += '''} MTK_CLK_BUF_DRIVING_CURRENT;\n'''
        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if len(value.get_varName()):
                gen_str += '''#define %s_STATUS_PMIC\t\tCLK_BUF_%s\n''' % (key.replace(r"CLK_", r"EXT"),
                                                                                value.get_varName().upper())

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if len(value.get_curList()) and len(value.get_current()):
                idx = value.get_curList().index(value.get_current())
                gen_str += '''#define %s_DRIVING_STRENGTH\t\tCLK_BUF_DRIVING_STRENGTH_%d\n''' % (key.replace(r"CLK_", r"EXT"), idx)

        gen_str += '''\n'''

        for key in sorted_key(ModuleObj.get_data(self).keys()):
            gen_str += '''#define %s_DRIVING_CURRENT\t\tCLK_BUF_DRIVING_CURRENT_1\n''' % key.replace(r"CLK_", r"EXT")

        gen_str += '''\n'''

        return gen_str

    def fill_dtsiFile(self):
        return ''
