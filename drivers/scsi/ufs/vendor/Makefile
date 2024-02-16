# SPDX-License-Identifier: GPL-2.0
# Mediatek UFS extended features makefile
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/scsi/ufs/vendor/
ccflags-y += -I$(srctree)/drivers/scsi/

obj-$(CONFIG_SCSI_UFS_MEDIATEK) += ufs-mediatek-mod.o
ufs-mediatek-mod-y := ufs-mediatek.o
ufs-mediatek-mod-y += ufshcd-pltfrm.o
ufs-mediatek-mod-y += ufshcd.o
ufs-mediatek-mod-y += ufshcd-crypto.o
ufs-mediatek-mod-y += ufs_bsg.o
ufs-mediatek-mod-y += ufs-debugfs.o
ufs-mediatek-mod-y += ufs-sysfs.o
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_XIAOMI_FFU) += feature/ffu/ufsffu.o 
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_XIAOMI_CLD) += feature/cld/ufscld.o 
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_XIAOMI_CLD) += feature/cld/ufscld_sysfs.o 
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_XIAOMI_CLD) += feature/cld/skhynix.o 
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_XIAOMI_CLD) += feature/cld/samsung.o 
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_XIAOMI_CLD) += feature/cld/kioxia.o
ufs-mediatek-mod-$(CONFIG_SCSI_UFS_XIAOMI_CLD) += feature/cld/micron.o
ufs-mediatek-mod-$(CONFIG_UFSFEATURE) += feature/ufsfeature.o feature/ufsfeature-mimic.o
ufs-mediatek-mod-$(CONFIG_UFSSHPB) += feature/hpb/ufsshpb.o
ufs-mediatek-mod-$(CONFIG_UFSTW) += feature/tw/ufstw.o
ufs-mediatek-mod-$(CONFIG_UFSHID) += feature/hid/ufshid.o
ufs-mediatek-mod-$(CONFIG_UFSRINGBUF) += feature/ringbuf/ufsringbuf.o
