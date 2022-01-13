# SPDX-License-Identifier: GPL-2.0
ifneq ($(strip $(CONFIG_BUILD_ARM64_DTB_IMAGE_NAMES)),)
dtb-y += $(addsuffix .dtb, $(notdir $(subst $\",,$(CONFIG_BUILD_ARM64_DTB_IMAGE_NAMES))))
endif

ifeq ($(strip $(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE)), y)
dtb-y += $(addsuffix .dtb, $(notdir $(subst $\",,$(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES))))
endif

always          := $(dtb-y)
subdir-y        := $(dts-dirs)
clean-files     := *.dtb
