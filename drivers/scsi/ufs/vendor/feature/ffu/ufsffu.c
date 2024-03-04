/*
 * Copyright (C) 2019 Xiaomi Ltd.
 *
 * Author:
 *    Shane Gao  <gaoshan3@xiaomi.com>
 *    Venco Du   <duwenchao@xiaomi.com>
 */
#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/reboot.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/efi.h>
#include <linux/genhd.h>
#include <linux/rcupdate.h>
#include <linux/of.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>

#include "ufshcd.h"
#include "ufsffu.h"
#include "sd.h"
#include "SAMSUNG_Solvit_UFS31_128GB_0300.h"
#include "SAMSUNG_Solvit_UFS31_256GB_0300.h"
#include "SAMSUNG_Solvit_UFS31_128GB_0500.h"
#include "SAMSUNG_Solvit_UFS31_256GB_0500.h"
#include "SAMSUNG_Solvit_UFS31_128GB_0700.h"
#include "SAMSUNG_Solvit_UFS31_256GB_0700.h"
#include "MICRON_Aquila_UFS31_128GB_256GB_Bridge_B103.h"
#include "MICRON_Aquila_UFS31_128GB_256GB_Bridge_B202.h"
#include "MICRON_Aquila_UFS31_128GB_256GB_FBS_0201.h"
#include "MICRON_Aquila_UFS31_128GB_256GB_FBS_0202.h"
#include "MICRON_Aquila_UFS31_128GB_256GB_512GB_0203.h"
#include "MICRON_Aquila_UFS31_128GB_256GB_512GB_B112.h"
#include "Selen_V6_TLC_256GB_P02_20201123.h"
#include "LD_VG_UFS31_A043.h"
#include "SKHYNIX_Voyager_UFS31_512GB_A003.h"


static struct fw_update_checklist need_update_k[] = {
	/* vendor_id[8], product_id[16], product_rev_from[4],
	product_rev_to[4], fw_addr, fw_size; */
	{"SAMSUNG", "KLUEG8UHDC-B0E1", "0100","0200",
	(uint64_t)Selen_V6_TLC_256GB_P02_20201123, sizeof(Selen_V6_TLC_256GB_P02_20201123)},
	{"SAMSUNG", "KLUDG4UHGC-B0E1", "0100","0700",
	(uint64_t)SAMSUNG_Solvit_UFS31_128GB_0700, sizeof(SAMSUNG_Solvit_UFS31_128GB_0700)},
	{"SAMSUNG", "KLUEG8UHGC-B0E1", "0100","0700",
	(uint64_t)SAMSUNG_Solvit_UFS31_256GB_0700, sizeof(SAMSUNG_Solvit_UFS31_256GB_0700)},
	{"SAMSUNG", "KLUDG4UHGC-B0E1", "0200","0700",
	(uint64_t)SAMSUNG_Solvit_UFS31_128GB_0700, sizeof(SAMSUNG_Solvit_UFS31_128GB_0700)},
	{"SAMSUNG", "KLUEG8UHGC-B0E1", "0200","0700",
	(uint64_t)SAMSUNG_Solvit_UFS31_256GB_0700, sizeof(SAMSUNG_Solvit_UFS31_256GB_0700)},
	{"SAMSUNG", "KLUDG4UHGC-B0E1", "0300","0700",
	(uint64_t)SAMSUNG_Solvit_UFS31_128GB_0700, sizeof(SAMSUNG_Solvit_UFS31_128GB_0700)},
	{"SAMSUNG", "KLUEG8UHGC-B0E1", "0300","0700",
	(uint64_t)SAMSUNG_Solvit_UFS31_256GB_0700, sizeof(SAMSUNG_Solvit_UFS31_256GB_0700)},
	{"MICRON", "MT128GAXAT2U3101", "0102","B112",
	(uint64_t)MICRON_Aquila_UFS31_128GB_256GB_512GB_B112, sizeof(MICRON_Aquila_UFS31_128GB_256GB_512GB_B112)},
	{"MICRON", "MT256GAXAT4U3101", "0102","B112",
	(uint64_t)MICRON_Aquila_UFS31_128GB_256GB_512GB_B112, sizeof(MICRON_Aquila_UFS31_128GB_256GB_512GB_B112)},
	{"MICRON", "MT512GAXAT8U31", "0102","B112",
	(uint64_t)MICRON_Aquila_UFS31_128GB_256GB_512GB_B112, sizeof(MICRON_Aquila_UFS31_128GB_256GB_512GB_B112)},
	{"MICRON", "MT128GAXAT2U3101", "B112","0203",
	(uint64_t)MICRON_Aquila_UFS31_128GB_256GB_512GB_0203, sizeof(MICRON_Aquila_UFS31_128GB_256GB_512GB_0203)},
	{"MICRON", "MT256GAXAT4U3101", "B112","0203",
	(uint64_t)MICRON_Aquila_UFS31_128GB_256GB_512GB_0203, sizeof(MICRON_Aquila_UFS31_128GB_256GB_512GB_0203)},
	{"MICRON", "MT512GAXAT8U31", "B112","0203",
	(uint64_t)MICRON_Aquila_UFS31_128GB_256GB_512GB_0203, sizeof(MICRON_Aquila_UFS31_128GB_256GB_512GB_0203)},
	{"MICRON", "MT128GAXAT2U3101", "0202","0203",
	(uint64_t)MICRON_Aquila_UFS31_128GB_256GB_512GB_0203, sizeof(MICRON_Aquila_UFS31_128GB_256GB_512GB_0203)},
	{"MICRON", "MT256GAXAT4U3101", "0202","0203",
	(uint64_t)MICRON_Aquila_UFS31_128GB_256GB_512GB_0203, sizeof(MICRON_Aquila_UFS31_128GB_256GB_512GB_0203)},
	{"MICRON", "MT512GAXAT8U31", "0202","0203",
	(uint64_t)MICRON_Aquila_UFS31_128GB_256GB_512GB_0203, sizeof(MICRON_Aquila_UFS31_128GB_256GB_512GB_0203)},
	{"SKHYNIX", "HN8T15BZGKX016", "A002","A043",
	(uint64_t)LD_VG_UFS31_A043, sizeof(LD_VG_UFS31_A043)},
	{"SKHYNIX", "HN8T25DEHKX077", "A002","A003",
	(uint64_t)SKHYNIX_Voyager_UFS31_512GB_A003, sizeof(SKHYNIX_Voyager_UFS31_512GB_A003)},
};
static void ufs_ffu_reboot(void *ptr)
{
	//ufs_ffu_reboot_reason_reboot(ptr);
	emergency_restart();
}

/*
* get info of partition from ufs for ffu bin
*
*/
static bool get_ffu_partition_info(struct ffu_data *ffudata)
{
	struct scsi_disk *sdkp = NULL;
	struct hd_struct *part_hd = NULL;
	struct disk_part_tbl *ptbl = NULL;
	struct scsi_device *sdev = ffudata->sdev;
	int part_number = ffudata->part_info_part_number;

	if(!sdev->sdev_gendev.driver_data){
		pr_err("[ufs_ffu] scsi disk is null\n");
		return false;
	}

	sdkp = (struct scsi_disk *)sdev->sdev_gendev.driver_data;
	if(!sdkp->disk) {
		pr_err("[ufs_ffu] gendisk is null\n");
		return false;
	}

	ffudata->part_info_part_name = sdkp->disk->disk_name;

	ptbl = rcu_dereference(sdkp->disk->part_tbl);
	if(ptbl->len < part_number) {
		pr_err("[ufs_ffu] This lun does not contain ffu parttion!\n");
		return false;
	}

	part_hd = rcu_dereference(ptbl->part[part_number]);
	if(!part_hd) {
		pr_err("[ufs_ffu] Get ffu parttion part_hd fail!\n");
		return false;
	}

	part_info.part_start = part_hd->start_sect * PART_SECTOR_SIZE / PART_BLOCK_SIZE;
	part_info.part_size = part_hd->nr_sects * PART_SECTOR_SIZE / PART_BLOCK_SIZE;

	pr_warn("[ufs_ffu] partion: %s start %d(block) size %d(block)\n", 
			ffudata->part_info_part_name, part_info.part_start, part_info.part_size);

	return true;
}

/*
*crc funtion
*/
static unsigned int crc32(unsigned char const *p, unsigned int len)
{
	int i;
	unsigned int crc = 0;
	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
	}
 	return crc;
}

/*
* write data to partition
*
*/
static int ufs_scsi_write_partition(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t blocks)
{
	uint8_t cdb[16];
	int ret = 0;
	struct ufs_hba *hba = NULL;
	struct scsi_sense_hdr sshdr = {};
	unsigned long flags = 0;

	hba = shost_priv(sdev->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_err("get device fail\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	hba->host->eh_noresume = 1;

	// Maximum size of transfer supported by CMD_READ 10 is 16k blocks
	if (blocks > UFS_MAX_BLOCK_TRANSFERS) {
		return -EINVAL;
	}

	// Fill in the CDB with SCSI command structure
	memset (cdb, 0, sizeof(cdb));
	cdb[0] = WRITE_10;				// Command
	cdb[1] = 0;
	cdb[2] = (lba >> 24) & 0xff;	// LBA
	cdb[3] = (lba >> 16) & 0xff;
	cdb[4] = (lba >> 8) & 0xff;
	cdb[5] = (lba) & 0xff;
	cdb[6] = 0;						// Group Number
	cdb[7] = (blocks >> 8) & 0xff;	// Transfer Len
	cdb[8] = (blocks) & 0xff;
	cdb[9] = 0;						// Control

	ret = scsi_execute_req(sdev, cdb, DMA_TO_DEVICE, buf, (blocks * PART_BLOCK_SIZE), &sshdr, msecs_to_jiffies(15000), 3, NULL);
	if (ret) {
		pr_err("[ufs_ffu] write error %d\n", ret);
	}

	sdev_printk(KERN_ERR, sdev, "sense key:0x%x; asc:0x%x; ascq:0x%x\n", (int)sshdr.sense_key, (int)sshdr.asc, (int)sshdr.ascq);
	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;
	return ret;
}

static int ufs_write(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t buf_size)
{
	uint32_t transfer_size = 0, block_count = 0;
	void *temp_buffer = buf;
	int rc;

	block_count = (buf_size / PART_BLOCK_SIZE);

	/* Check if LBA plus the total sectors trying to access would exceed the */
	/* total size of the partition */
	if ((lba + block_count) > (part_info.part_start + part_info.part_size)) {
		return -EINVAL;
	}

	while (block_count > 0) {
		transfer_size = (block_count > UFS_MAX_BLOCK_TRANSFERS) ? UFS_MAX_BLOCK_TRANSFERS : block_count;

		rc = ufs_scsi_write_partition(sdev, temp_buffer, lba, transfer_size);

		lba = lba + transfer_size;
		block_count = block_count - transfer_size;
		temp_buffer = temp_buffer + (transfer_size * PART_BLOCK_SIZE) / sizeof(void);

		if (rc != 0) {
			pr_err("[ufs_ffu] write partition error %d\n", rc);
			return 1;
		}
	}
	return 0;
}

/*
* get data from partition from ufs for ffu bin
*
*/
static int ufs_scsi_read_partition(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t blocks)
{
	uint8_t cdb[16];
	int ret = 0;
	struct ufs_hba *hba = NULL;
	struct scsi_sense_hdr sshdr = {};
	unsigned long flags = 0;

	hba = shost_priv(sdev->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_err("get device fail\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	hba->host->eh_noresume = 1;

	// Maximum size of transfer supported by CMD_READ 10 is 16k blocks
	if (blocks > UFS_MAX_BLOCK_TRANSFERS) {
		return -EINVAL;
	}

	// Fill in the CDB with SCSI command structure
	memset (cdb, 0, sizeof(cdb));
	cdb[0] = READ_10;				// Command
	cdb[1] = 0;
	cdb[2] = (lba >> 24) & 0xff;	// LBA
	cdb[3] = (lba >> 16) & 0xff;
	cdb[4] = (lba >> 8) & 0xff;
	cdb[5] = (lba) & 0xff;
	cdb[6] = 0;						// Group Number
	cdb[7] = (blocks >> 8) & 0xff;	// Transfer Len
	cdb[8] = (blocks) & 0xff;
	cdb[9] = 0;						// Control

	ret = scsi_execute_req(sdev, cdb, DMA_FROM_DEVICE, buf, (blocks * PART_BLOCK_SIZE), &sshdr, msecs_to_jiffies(15000), 3, NULL);
	if (ret) {
		pr_err("[ufs_ffu] read error %d\n", ret);
	}

	sdev_printk(KERN_ERR, sdev, "sense key:0x%x; asc:0x%x; ascq:0x%x\n", (int)sshdr.sense_key, (int)sshdr.asc, (int)sshdr.ascq);
	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;
	return ret;
}

static int ufs_read(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t buf_size)
{
	uint32_t transfer_size = 0, block_count = 0;
	void *temp_buffer = buf;
	int rc;

	block_count = (buf_size / PART_BLOCK_SIZE);

	/* Check if LBA plus the total sectors trying to access would exceed the */
	/* total size of the partition */
	if ((lba + block_count) > (part_info.part_start + part_info.part_size)) {
		return -EINVAL;
	}

	while (block_count > 0) {
		transfer_size = (block_count > UFS_MAX_BLOCK_TRANSFERS) ? UFS_MAX_BLOCK_TRANSFERS : block_count;

		rc = ufs_scsi_read_partition(sdev, temp_buffer, lba, transfer_size);

		lba = lba + transfer_size;
		block_count = block_count - transfer_size;
		temp_buffer = temp_buffer + (transfer_size * PART_BLOCK_SIZE) / sizeof(void);

		if (rc != 0) {
			pr_err("[ufs_ffu] read partition error %d\n", rc);
			return 1;
		}
	}
	return 0;
}


static int check_fw_version(struct fw_update_checklist *fw_info, struct ffu_inquiry *stdinq)
{
	if (strncmp((char *)stdinq->vendor_id, (char *)fw_info->vendor_id, strlen(fw_info->vendor_id))) {
		pr_notice("[ufs_ffu] check fw vendor_id not matching %s\n", (char *)fw_info->vendor_id);
		return 1;
	}
	if (strncmp((char *)stdinq->product_id, (char *)fw_info->product_id, strlen(fw_info->product_id))) {
		pr_notice("[ufs_ffu] check fw product_id not matching %s\n", (char *)fw_info->product_id);
		return 2;
	}
	if (!strncmp((char *)stdinq->product_rev, (char *)fw_info->product_rev_to, strlen(fw_info->product_rev_to))) {
		pr_notice("[ufs_ffu] check fw product_rev_to not matching %s\n", (char *)fw_info->product_rev_to);
		return 3;
	}
	return 0;
}

static int get_fw_update_checklist(struct scsi_device *sdev, struct fw_update_checklist **need_update, struct ffu_inquiry *stdinq)
{
	void* buf = NULL;
	char* fw = NULL;
	int ret = sizeof(need_update_k) / sizeof(need_update_k[0]);
	int rc = 0, i = 0, list_count = 0;
	device_struct part_head;
	struct fw_update_checklist fw_info;

	buf = kmalloc(PART_BLOCK_SIZE, GFP_KERNEL);
	memset(buf, 0, PART_BLOCK_SIZE);

	/*get ffu partition for head part info*/
	rc = ufs_read(sdev, buf, part_info.part_start, PART_BLOCK_SIZE);
	if (rc) {
		pr_err("[ufs_ffu] get partion info error %d, use internal fw update list\n", rc);
		*need_update = need_update_k;
		goto out;
	}

	memcpy(&part_head, buf, sizeof(part_head));
	if (strncmp(part_head.ffu_flag, FFU_FLAG, sizeof(FFU_FLAG))) {
		pr_err("[ufs_ffu] ufs_flag mismatches ffu, use internal fw update list, ufs_flag=%s\n", part_head.ffu_flag);
		*need_update = need_update_k;
		goto out;
	}

	pr_notice("[ufs_ffu] ffu_count is %d\n", part_head.ffu_count);
	if (0 == part_head.ffu_count) {
		pr_err("[ufs_ffu] ffu_count is %d, use internal fw update list\n", part_head.ffu_count);
		*need_update = need_update_k;
		goto out;
	}

	ret = sizeof(need_update_k) / sizeof(need_update_k[0]);
	list_count = part_head.ffu_count + ret;

	/*malloc a list need_update for ffu*/
	*need_update = kmalloc(list_count*sizeof(struct fw_update_checklist), GFP_KERNEL);
	memset(*need_update, 0, list_count*sizeof(struct fw_update_checklist));

	/*copy need_update_k list to need_update list*/
	memcpy(*need_update, need_update_k, sizeof(need_update_k));

	/*put info from ffu partition for bin*/
	for (i = 0; i < part_head.ffu_count; i++) {
		int32_t size = 0;
		uint64_t offset = 0;
		unsigned int *crc = NULL;

		memcpy(&fw_info, (buf + FFU_BIN_HEAD_INFO + i *  FFU_BIN_HEAD_INFO), sizeof(struct fw_update_checklist));
		crc = (unsigned int*)(buf + FFU_BIN_HEAD_INFO + i *  FFU_BIN_HEAD_INFO + sizeof(struct fw_update_checklist));
		pr_notice("[ufs_ffu] get info from partition crc 0x%lx addr 0x%llx size 0x%x\n", *crc, fw_info.fw_addr, fw_info.fw_size);

		if (!fw_info.fw_addr || fw_info.fw_addr % PART_BLOCK_SIZE) {
			pr_notice("[ufs_ffu] ffu bin is not 4K align 0x%llx\n", fw_info.fw_addr);
			continue;
		}

		rc = check_fw_version(&fw_info, stdinq);
		if (rc) {
			continue;
		}

		offset = fw_info.fw_addr / PART_BLOCK_SIZE;
		size = fw_info.fw_size % PART_BLOCK_SIZE ?((fw_info.fw_size / PART_BLOCK_SIZE) + 1) * PART_BLOCK_SIZE : fw_info.fw_size;

		fw = kmalloc(size, GFP_KERNEL);
		memset(fw, 0, size);

		rc = ufs_read(sdev, fw, (part_info.part_start + offset), size);
		if (rc) {
			pr_err("[ufs_ffu] get fw bin info error %d\n", rc);
			continue;
		}

		if (*crc != crc32(fw, fw_info.fw_size)) {
			pr_notice("[ufs_ffu] crc error 0x%x diff 0x%x\n", *crc ,crc32(fw, fw_info.fw_size));
			kfree(fw);
			continue;
		}

		fw_info.fw_addr = (uint64_t)fw;
		memcpy((*need_update + ret), &fw_info, sizeof(struct fw_update_checklist));
		ret++;
	}

	pr_info("[ufs_ffu] partion start %d size %d\n", part_info.part_start, part_info.part_size);
out:
	kfree(buf);
	return ret;
}

static int ufs_scsi_write_buf(struct scsi_device *sdev, uint8_t *buf, uint8_t mode, uint8_t buf_id, int32 offset, uint32 len)
{
	struct ufs_hba *hba = NULL;
	unsigned char cdb[10] = {0};
	struct scsi_sense_hdr sshdr = {};
	unsigned long flags = 0;
	int ret = 0;

	hba = shost_priv(sdev->host);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		ret = -ENODEV;
		scsi_device_put(sdev);
		pr_err("get device fail\n");
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	hba->host->eh_noresume = 1;

	cdb[0] = WRITE_BUFFER;
	cdb[1] = mode;
	cdb[2] = buf_id;
	cdb[3] = (offset >> 16) & 0xff;
	cdb[4] = (offset >> 8) & 0xff;
	cdb[5] = (offset) & 0xff;
	cdb[6] = (len >> 16) & 0xff;
	cdb[7] = (len >> 8) & 0xff;
	cdb[8] = (len) & 0xff;
	cdb[9] = 0;

	ret = scsi_execute(sdev, cdb, DMA_TO_DEVICE, buf, len, NULL, &sshdr, msecs_to_jiffies(15000), 0, 0, RQF_PM, NULL);

	if (ret) {
		sdev_printk(KERN_ERR, sdev, "WRITE BUFFER failed for firmware upgrade %d\n", ret);
	}

	/*check sense key*/
	sdev_printk(KERN_ERR, sdev, "sense key:0x%x; asc:0x%x; ascq:0x%x\n", (int)sshdr.sense_key, (int)sshdr.asc, (int)sshdr.ascq);

	/* for some devices, device can't response to host after write buffer.
	ret = ufshcd_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_DEVICE_FFU_STATUS, 0, 0, &attr);

		if (ret) {
			dev_err(hba->dev, "%s: query bDeviceFFUStatus failed, err %d\n",__func__, ret);
			return -2;
		}

		if (attr > UFS_FFU_STATUS_OK) {
			dev_err(hba->dev, "%s: check bDeviceFFUStatus fail:%d\n",	__func__, attr);
			ret = -1;
			return ret;
		} else {
			dev_err(hba->dev, "%s: check bDeviceFFUStatus pass:%d\n",	__func__, attr);
		}
	*/

	scsi_device_put(sdev);
	hba->host->eh_noresume = 0;
	return ret;
}

#define WB_SIZE (512*1024)
static int32 ufs_fw_update_write(struct scsi_device *sdev, int32_t size, uint8_t *buf)
{
	int32 rc = 0;
	int offset = 0;
	char *buf_ptr = kzalloc(size, GFP_KERNEL);

	if (!buf_ptr) {
		pr_err("ffu kzalloc fail.\n");
		rc = -1;
		return rc;
	}

	memcpy(buf_ptr, buf, size);
	pr_info("fw addr: %p, %p. total fw size: %d\n", buf_ptr, buf, size);

	for (offset = 0; offset + WB_SIZE <= size; offset += WB_SIZE) {
		pr_info("ffu offset:%d, size%d\n", offset, WB_SIZE);
		rc = ufs_scsi_write_buf(sdev, buf_ptr+offset, 0x0e, 0x00, offset, WB_SIZE);
		if (rc)
			goto out;
	}

	if (size % WB_SIZE) {
		pr_info("ffu offset:%d, size%d\n", offset, size%WB_SIZE);
		rc = ufs_scsi_write_buf(sdev, buf_ptr+offset, 0x0e, 0x00, offset, size % WB_SIZE);
	}

out:
	kfree(buf_ptr);
	return rc;
}

static int check_ffu_parition_fw_error(struct scsi_device *sdev, int index)
{
	void *buf = NULL;
	int rc = 0;

	int update_k_size = sizeof(need_update_k) / sizeof(need_update_k[0]);

	buf = kzalloc(PART_BLOCK_SIZE, GFP_KERNEL);

	if (index >= update_k_size) {
		/*clear ffu parition HEAD*/
		rc = ufs_write(sdev, buf, part_info.part_start, PART_BLOCK_SIZE);
		if (rc) {
			pr_err("[ufs_ffu] write partion info error %d\n", rc);
		}
	}

	kfree(buf);
	return 0;

}

static int start_ffu(struct ffu_data *ffudata)
{
	int ret = 0;
	unsigned int i = 0;
	int fw_update_checklist_size = 0;
	struct ffu_inquiry stdinq = {};
	struct ufs_hba *hba = NULL;
	char *parse_inquiry = NULL;
	struct fw_update_checklist *need_update = NULL;

	hba = shost_priv(ffudata->sdev->host);

	parse_inquiry = hba->sdev_ufs_device->inquiry + 8;  /*be careful about the inquiry formate*/

	memcpy(stdinq.vendor_id, parse_inquiry, INQURIY_VENDOR_ID_SIZE);
	parse_inquiry += INQURIY_VENDOR_ID_SIZE;

	memcpy(stdinq.product_id, parse_inquiry, INQURIY_PRODUCT_ID_SIZE);
	parse_inquiry += INQURIY_PRODUCT_ID_SIZE;

	memcpy(stdinq.product_rev, parse_inquiry, INQURIY_FW_VERSION_SIZE);


	pr_notice("[ufs_ffu] check ffu_arry, vendor:%s, product:%s, fw_rev:%s\n", stdinq.vendor_id, stdinq.product_id, stdinq.product_rev);

	fw_update_checklist_size = get_fw_update_checklist(ffudata->sdev, &need_update, &stdinq);

	for (i = 0; i < fw_update_checklist_size; i++) {
		/* step 1. match vendor id && product id */
		if (strncmp((char *)stdinq.vendor_id, (char *)need_update[i].vendor_id, strlen(need_update[i].vendor_id)) == 0
				&& strncmp((char *)stdinq.product_id, (char *)need_update[i].product_id, strlen(need_update[i].product_id)) == 0) {
			pr_info("match vendor:%s, product id:%s hw_sectors %d\n", stdinq.vendor_id, stdinq.product_id,
				queue_max_hw_sectors(ffudata->sdev->request_queue));

			/*step 2.  match product revision from */
			if (strncmp((char *)stdinq.product_rev, (char *)need_update[i].product_rev_from, strlen(need_update[i].product_rev_from)) == 0) {
				pr_info("match product revision from:%s\n", stdinq.product_rev);
				pr_info("start write buffer\n");
				pr_info("fw_size = %d, address = %p\n", need_update[i].fw_size, need_update[i].fw_addr);
				ret = ufs_fw_update_write(ffudata->sdev, need_update[i].fw_size, (uint8_t *)need_update[i].fw_addr);
				if (ret == 0) {
					pr_notice("write buffer success\n");
					/*kernel_restart don't  work sometimes. use kernel_restart and sleep 10ms to print above logs*/
					msleep(20);
					ufs_ffu_reboot(NULL);
				} else {
					/*set a flag at here and do not enter ffu next time.*/
					pr_notice("ufs ffu failed, don't clear fail flag, reboot directly\n");
					check_ffu_parition_fw_error(ffudata->sdev, i);
					msleep(20);
					ufs_ffu_reboot("ffu");
				}
				break;

			} else if (strncmp((char *)stdinq.product_rev, (char *)need_update[i].product_rev_to, strlen(need_update[i].product_rev_to)) == 0) {
				/*step 3. match the product version to after reboot.do not use loop just for reminder.*/
				pr_notice("ufs_ffu firmware is already the newest now\n");
				continue;
			} else {
				pr_notice("ufs_ffu do not match the rev\n");
			}
		} else {
			pr_info("ufs_ffu do not match %d:%s %s\n", i, need_update[i].vendor_id, need_update[i].product_id);
		}
	}

	if (need_update != need_update_k) {
		kfree(need_update);
	}
	return ret;
}

static bool check_off_charge_flag(struct ffu_data *ffudata)
{
	if (ffudata->off_charge_flag) {
		pr_notice("ufs_ffu enter off charger\n");
	}
	return ffudata->off_charge_flag;
}

static bool check_device_is_correct(struct ffu_data *ffudata)
{

	if (ffudata->one_time_entry) {
		pr_notice("[ufs_ffu] one time entry is true\n");
		return false;
	}

	if(ffudata->bootreason == UFS_FFU_BOOTREASON) {
		pr_err("bootreason was set ffu in last boot. skip FFU.\n");
		return false;
	}

	if (ffudata->sdev->host->hostt->proc_name == NULL) {
		pr_notice("[ufs_ffu] proc name is null\n");
		return false;
	}

	if (strncmp(ffudata->sdev->host->hostt->proc_name, UFSHCD, strlen(UFSHCD))) {
		/*check if the device is ufs. If not, just return directly.*/
		pr_notice("[ufs_ffu] proc name is not ufs\n");
		return false;
	}

	return true;
}

static bool of_obtain_memory_info(struct ffu_data *ffudata)
{
	u32 ufs_ffu_bootreason = 0;
	u32 ufs_ffu_batterystatus = 0;
	struct device_node *mem_node;

	mem_node = of_find_node_by_path("/memory");
	
	if (!mem_node)
		return false;

	of_property_read_u32(mem_node, "ufs_ffu_batterystatus", &ufs_ffu_batterystatus);
	of_property_read_u32(mem_node, "ufs_ffu_bootreason", &ufs_ffu_bootreason);
	of_node_put(mem_node);

	ffudata->bootreason = ufs_ffu_bootreason;
	ffudata->batterstatus = ufs_ffu_batterystatus;

	if(ffudata->batterstatus == 1)
		ffudata->off_charge_flag = true;

	return true;
}

static bool look_up_scsi_device(struct ffu_data *ffudata, int lun)
{
	struct Scsi_Host *shost;

	shost = scsi_host_lookup(0);
	if (!shost)
		return false;

	ffudata->sdev = scsi_device_lookup(shost, 0, 0, lun);
	if (!ffudata->sdev)
		return false;

	scsi_host_put(shost);

	return true;
}


static void ffu_ffu_delayed_work(struct work_struct *work)
{
	int lun = 2;
	struct ffu_data *ffudata = container_of(to_delayed_work(work),
					   struct ffu_data,
					   ffu_delayed_work);

	pr_notice("ufs_ffu Start\n");

	ffudata->off_charge_flag = false;
	ffudata->one_time_entry = false;
	ffudata->part_info_part_number = FFU_PART_NUMBER;

/*
	Useless in this realization
*/
	if(!of_obtain_memory_info(ffudata))
		return;

	/*check enter off charge*/
	if (check_off_charge_flag(ffudata))
		return;

	for(lun = 2; lun >= 0; lun--) {
		pr_notice("ufs_ffu lun = %d\n",lun);
		if (!look_up_scsi_device(ffudata, lun))
			continue;

		/*check device is correct*/
		if (!check_device_is_correct(ffudata))
			continue;

		if(get_ffu_partition_info(ffudata)){
			start_ffu(ffudata);
			break;
		}
	}



	ffudata->one_time_entry = true;
	scsi_device_put(ffudata->sdev);

	cancel_delayed_work(&ffudata->ffu_delayed_work);
	kfree(ffudata);

	return;
}

void ufs_ffu(void)
{
	struct ffu_data *ffudata = NULL;

	ffudata = (struct ffu_data *)kzalloc(sizeof(struct ffu_data), GFP_KERNEL);

	INIT_DELAYED_WORK(&ffudata->ffu_delayed_work, ffu_ffu_delayed_work);

	schedule_delayed_work(&ffudata->ffu_delayed_work, msecs_to_jiffies(FFU_WORK_DELAY_MS));
}

