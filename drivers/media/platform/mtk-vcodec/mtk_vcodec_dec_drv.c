// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *         Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/iommu.h>
#include <linux/pm_wakeup.h>
#include <linux/delay.h>
#include <linux/suspend.h>

#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_dec_pm_plat.h"
#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"

module_param(mtk_v4l2_dbg_level, int, 0644);
module_param(mtk_vcodec_dbg, bool, 0644);
module_param(mtk_vcodec_perf, bool, 0644);
module_param(mtk_vcodec_vcp, int, 0644);
char mtk_vdec_property_prev[1024];
char mtk_vdec_vcp_log_prev[1024];
module_param(mtk_vdec_sw_mem_sec, int, 0644);

static struct mtk_vcodec_dev *dev_ptr;

static int mtk_vcodec_vcp_log_write(const char *val, const struct kernel_param *kp)
{
	pr_info("%s, val: %s, len: %d", __func__, val, strlen(val));
	if (!(val == NULL || strlen(val) == 0))
		mtk_vcodec_set_log(dev_ptr, val, MTK_VCODEC_LOG_INDEX_LOG);
	return 0;
}
static struct kernel_param_ops vcodec_vcp_log_param_ops = {
	.set = mtk_vcodec_vcp_log_write,
};
module_param_cb(mtk_vdec_vcp_log, &vcodec_vcp_log_param_ops, &mtk_vdec_vcp_log, 0644);

static int mtk_vcodec_vcp_property_write(const char *val, const struct kernel_param *kp)
{
	pr_info("%s, val: %s, len: %d", __func__, val, strlen(val));
	if (!(val == NULL || strlen(val) == 0))
		mtk_vcodec_set_log(dev_ptr, val, MTK_VCODEC_LOG_INDEX_PROP);
	return 0;
}
static struct kernel_param_ops vcodec_vcp_prop_param_ops = {
	.set = mtk_vcodec_vcp_property_write,
};
module_param_cb(mtk_vdec_property, &vcodec_vcp_prop_param_ops, &mtk_vdec_property, 0644);

static int fops_vcodec_open(struct file *file)
{
	struct mtk_vcodec_dev *dev = video_drvdata(file);
	struct mtk_vcodec_ctx *ctx = NULL;
	struct mtk_video_dec_buf *mtk_buf = NULL;
	int ret = 0;
	int i = 0;
	struct vb2_queue *src_vq;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mtk_buf = kzalloc(sizeof(*mtk_buf), GFP_KERNEL);
	if (!mtk_buf) {
		kfree(ctx);
		return -ENOMEM;
	}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	if (mtk_vcodec_vcp & (1 << MTK_INST_DECODER))
		vcp_register_feature(VDEC_FEATURE_ID);
#endif

	mutex_lock(&dev->dev_mutex);
	ctx->dec_flush_buf = mtk_buf;
	ctx->id = dev->id_counter++;
	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);
	INIT_LIST_HEAD(&ctx->list);
	ctx->dev = dev;
	for (i = 0; i < MTK_VDEC_HW_NUM; i++)
		init_waitqueue_head(&ctx->queue[i]);
	mutex_init(&ctx->buf_lock);
	mutex_init(&ctx->worker_lock);
	mutex_init(&ctx->hw_status);
	mutex_init(&ctx->q_mutex);

	ctx->type = MTK_INST_DECODER;
	ret = mtk_vcodec_dec_ctrls_setup(ctx);
	if (ret) {
		mtk_v4l2_err("Failed to setup mt vcodec controls");
		goto err_ctrls_setup;
	}
	ctx->m2m_ctx = v4l2_m2m_ctx_init(dev->m2m_dev_dec, ctx,
		&mtk_vcodec_dec_queue_init);
	if (IS_ERR((__force void *)ctx->m2m_ctx)) {
		ret = PTR_ERR((__force void *)ctx->m2m_ctx);
		mtk_v4l2_err("Failed to v4l2_m2m_ctx_init() (%d)",
					 ret);
		goto err_m2m_ctx_init;
	}
	src_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
		V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	ctx->dec_flush_buf->vb.vb2_buf.vb2_queue = src_vq;

	mtk_vcodec_dec_set_default_params(ctx);

	if (v4l2_fh_is_singular(&ctx->fh)) {
		ret = mtk_vcodec_dec_pw_on(&dev->pm);
		if (ret < 0)
			goto err_load_fw;
		/*
		 * vcu_load_firmware checks if it was loaded already and
		 * does nothing in that case
		 */
		ret = VCU_FPTR(vcu_load_firmware)(dev->vcu_plat_dev);
		if (ret < 0) {
			/*
			 * Return 0 if downloading firmware successfully,
			 * otherwise it is failed
			 */
			mtk_v4l2_err("vcu_load_firmware failed!");
			goto err_load_fw;
		}

		if (VCU_FPTR(vcu_compare_version)(dev->vcu_plat_dev,
			MTK_VCU_FW_VERSION) != 0) {
			mtk_v4l2_err("Invalid vcu firmware, should be %s!",
						 MTK_VCU_FW_VERSION);
			ret = -EPERM;
			goto err_load_fw;
		}
	}
#endif
	dev->dec_cnt++;

	mutex_unlock(&dev->dev_mutex);
	mtk_v4l2_debug(0, "%s decoder [%d]", dev_name(&dev->plat_dev->dev),
				   ctx->id);
	return ret;

	/* Deinit when failure occurred */

#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
err_load_fw:
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	mtk_vcodec_del_ctx_list(ctx);
#endif
err_m2m_ctx_init:
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
err_ctrls_setup:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx->dec_flush_buf);
	kfree(ctx);
	mutex_unlock(&dev->dev_mutex);

	return ret;
}

static int fops_vcodec_release(struct file *file)
{
	struct mtk_vcodec_dev *dev = video_drvdata(file);
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(file->private_data);

	mtk_v4l2_debug(0, "[%d] decoder", ctx->id);
	mutex_lock(&dev->dev_mutex);

	/*
	 * Call v4l2_m2m_ctx_release before mtk_vcodec_dec_release. First, it
	 * makes sure the worker thread is not running after vdec_if_deinit.
	 * Second, the decoder will be flushed and all the buffers will be
	 * returned in stop_streaming.
	 */
	mtk_vcodec_dec_empty_queues(file, ctx);
	// Need to sync worker status in case ctx is free.
	mutex_lock(&ctx->worker_lock);
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	mutex_unlock(&ctx->worker_lock);
	mtk_vcodec_dec_release(ctx);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);

	kfree(ctx->dec_flush_buf);
	kfree(ctx);
	if (dev->dec_cnt > 0)
		dev->dec_cnt--;
	mutex_unlock(&dev->dev_mutex);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	if (mtk_vcodec_vcp & (1 << MTK_INST_DECODER))
		vcp_deregister_feature(VDEC_FEATURE_ID);
#endif

	return 0;
}

static const struct v4l2_file_operations mtk_vcodec_fops = {
	.owner          = THIS_MODULE,
	.open           = fops_vcodec_open,
	.release        = fops_vcodec_release,
	.poll           = v4l2_m2m_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap           = v4l2_m2m_fop_mmap,
};


/**
 * Suspend callbacks after user space processes are frozen
 * Since user space processes are frozen, there is no need and cannot hold same
 * mutex that protects lock owner while checking status.
 * If video codec hardware is still active now, must not to enter suspend.
 **/
static int mtk_vcodec_dec_suspend(struct device *pDev)
{
	int val, i;
	struct mtk_vcodec_dev *dev = dev_get_drvdata(pDev);

	for (i = 0; i < MTK_VDEC_HW_NUM; i++) {
		val = down_trylock(&dev->dec_sem[i]);
		if (val == 1) {
			mtk_v4l2_debug(0, "fail due to videocodec activity");
			return -EBUSY;
		}
		up(&dev->dec_sem[i]);
	}

	mtk_v4l2_debug(1, "done");
	return 0;
}

static int mtk_vcodec_dec_resume(struct device *pDev)
{
	mtk_v4l2_debug(1, "done");
	return 0;
}

static int mtk_vcodec_dec_suspend_notifier(struct notifier_block *nb,
					unsigned long action, void *data)
{
	int wait_cnt = 0;
	int val = 0;
	int i;
	struct mtk_vcodec_dev *dev =
		container_of(nb, struct mtk_vcodec_dev, pm_notifier);

	mtk_v4l2_debug(1, "action = %ld", action);
	switch (action) {
	case PM_SUSPEND_PREPARE:
		dev->is_codec_suspending = 1;
		for (i = 0; i < MTK_VDEC_HW_NUM; i++) {
			val = down_trylock(&dev->dec_sem[i]);
			while (val == 1) {
				usleep_range(10000, 20000);
				wait_cnt++;
				/* Current task is still not finished, don't
				 * care, will check again in real suspend
				 */
				if (wait_cnt > 5) {
					mtk_v4l2_err("waiting fail");
					return NOTIFY_DONE;
				}
				val = down_trylock(&dev->dec_sem[i]);
			}
			up(&dev->dec_sem[i]);
		}
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		dev->is_codec_suspending = 0;
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_DONE;
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
extern void vdec_vcp_probe(struct mtk_vcodec_dev *dev);
#endif

static int mtk_vcodec_dec_probe(struct platform_device *pdev)
{
	struct mtk_vcodec_dev *dev;
	struct video_device *vfd_dec;
	struct resource *res;
	int i = 0, reg_index = 0, ret;
	const char *name = NULL;
	u32 port_id;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	INIT_LIST_HEAD(&dev->ctx_list);
	dev->plat_dev = pdev;

#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
	if (VCU_FPTR(vcu_get_plat_device)) {
		dev->vcu_plat_dev = VCU_FPTR(vcu_get_plat_device)(dev->plat_dev);
		if (dev->vcu_plat_dev == NULL) {
			mtk_v4l2_err("[VCU] vcu device in not ready");
			return -EPROBE_DEFER;
		}
	}
#endif

	ret = of_property_read_string(pdev->dev.of_node, "mediatek,platform", &dev->platform);
	if (ret != 0) {
		mtk_v4l2_err("failed to find mediatek,platform\n");
		return ret;
	}
	mtk_v4l2_debug(0, "%s", dev->platform);

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,ipm", &dev->vdec_hw_ipm);
	if (ret != 0 || dev->vdec_hw_ipm > VCODEC_IPM_MAX) {
		mtk_v4l2_debug(0, "default use ipm v1");
		dev->vdec_hw_ipm = VCODEC_IPM_V1;
	}
	mtk_v4l2_debug(0, "hw ipm: %d", dev->vdec_hw_ipm);

	ret = mtk_vcodec_init_dec_pm(dev);
	if (ret < 0) {
		dev_info(&pdev->dev, "Failed to get mt vcodec clock source");
		return ret;
	}

	for (i = 0; i < NUM_MAX_VDEC_REG_BASE; i++)
		dev->dec_reg_base[i] = NULL;
	for (i = 0; !of_property_read_string_index(pdev->dev.of_node, "reg-names", i, &name); i++) {
		if (!strcmp(MTK_VDEC_REG_NAME_VDEC_BASE, name)) {
			reg_index = VDEC_BASE;
		} else if (!strcmp(MTK_VDEC_REG_NAME_VDEC_SYS, name)) {
			reg_index = VDEC_SYS;
		} else if (!strcmp(MTK_VDEC_REG_NAME_VDEC_VLD, name)) {
			reg_index = VDEC_VLD;
		} else if (!strcmp(MTK_VDEC_REG_NAME_VDEC_MC, name)) {
			reg_index = VDEC_MC;
		} else if (!strcmp(MTK_VDEC_REG_NAME_VDEC_MV, name)) {
			reg_index = VDEC_MV;
		} else if (!strcmp(MTK_VDEC_REG_NAME_VDEC_MISC, name)) {
			reg_index = VDEC_MISC;
		} else if (!strcmp(MTK_VDEC_REG_NAME_VDEC_LAT_MISC, name)) {
			reg_index = VDEC_LAT_MISC;
		} else if (!strcmp(MTK_VDEC_REG_NAME_VDEC_LAT_VLD, name)) {
			reg_index = VDEC_LAT_VLD;
		} else if (!strcmp(MTK_VDEC_REG_NAME_VDEC_SOC_GCON, name)) {
			reg_index = VDEC_SOC_GCON;
		} else if (!strcmp(MTK_VDEC_REG_NAME_VDEC_RACING_CTRL, name)) {
			reg_index = VDEC_RACING_CTRL;
		} else {
			dev_info(&pdev->dev, "invalid reg name: %s, index: %d", name, i);
			return -EINVAL;
		}
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res == NULL) {
			dev_info(&pdev->dev, "get memory resource failed.");
			ret = -ENXIO;
			goto err_res;
		}
		dev->dec_reg_base[reg_index] = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR((__force void *)dev->dec_reg_base[reg_index])) {
			ret = PTR_ERR((__force void *)dev->dec_reg_base[reg_index]);
			goto err_res;
		}
		mtk_v4l2_debug(2, "reg[%d] base=0x%px",
			reg_index, dev->dec_reg_base[reg_index]);
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_info(&pdev->dev, "failed to get irq resource");
		ret = -ENOENT;
		goto err_res;
	}

	ret = mtk_vcodec_dec_irq_setup(pdev, dev);
	if (ret)
		goto err_res;

	for (i = 0; i < MTK_VDEC_HW_NUM; i++) {
		sema_init(&dev->dec_sem[i], 1);
		spin_lock_init(&dev->dec_power_lock[i]);
		dev->dec_is_power_on[i] = false;
	}
	mutex_init(&dev->ctx_mutex);
	mutex_init(&dev->dev_mutex);
	mutex_init(&dev->ipi_mutex);
	mutex_init(&dev->ipi_mutex_res);
	mutex_init(&dev->dec_dvfs_mutex);
	spin_lock_init(&dev->irqlock);

	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "%s",
			 "[/MTK_V4L2_VDEC]");

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret) {
		mtk_v4l2_err("v4l2_device_register err=%d", ret);
		goto err_res;
	}

	vfd_dec = video_device_alloc();
	if (!vfd_dec) {
		mtk_v4l2_err("Failed to allocate video device");
		ret = -ENOMEM;
		goto err_dec_alloc;
	}
	vfd_dec->fops           = &mtk_vcodec_fops;
	vfd_dec->ioctl_ops      = &mtk_vdec_ioctl_ops;
	vfd_dec->release        = video_device_release;
	vfd_dec->lock           = &dev->dev_mutex;
	vfd_dec->v4l2_dev       = &dev->v4l2_dev;
	vfd_dec->vfl_dir        = VFL_DIR_M2M;
	vfd_dec->device_caps    = V4L2_CAP_VIDEO_M2M_MPLANE |
							  V4L2_CAP_STREAMING;

	snprintf(vfd_dec->name, sizeof(vfd_dec->name), "%s",
			 MTK_VCODEC_DEC_NAME);
	video_set_drvdata(vfd_dec, dev);
	dev->vfd_dec = vfd_dec;
	platform_set_drvdata(pdev, dev);

	dev->m2m_dev_dec = v4l2_m2m_init(&mtk_vdec_m2m_ops);
	if (IS_ERR((__force void *)dev->m2m_dev_dec)) {
		mtk_v4l2_err("Failed to init mem2mem dec device");
		ret = PTR_ERR((__force void *)dev->m2m_dev_dec);
		goto err_dec_mem_init;
	}

	dev->decode_workqueue =
		alloc_ordered_workqueue(MTK_VCODEC_DEC_NAME,
			WQ_MEM_RECLAIM | WQ_FREEZABLE);
	if (!dev->decode_workqueue) {
		mtk_v4l2_err("Failed to create decode workqueue");
		ret = -EINVAL;
		goto err_event_workq;
	}

	ret = video_register_device(vfd_dec, VFL_TYPE_VIDEO, -1);
	if (ret) {
		mtk_v4l2_err("Failed to register video device");
		goto err_dec_reg;
	}

	for (i = 0; i < NUM_MAX_VDEC_M4U_PORT; i++)
		dev->dec_m4u_ports[i] = 0;
	for (i = 0; !of_property_read_string_index(
			pdev->dev.of_node, "m4u-port-names", i, &name); i++) {
		reg_index = mtk_vdec_m4u_port_name_to_index(name);
		if (reg_index < 0) {
			dev_info(&pdev->dev, "invalid m4u port name: %s, index: %d", name, i);
			return -EINVAL;
		}
		ret = of_property_read_u32_index(pdev->dev.of_node,
			"m4u-ports", i, &port_id);
		if (ret) {
			dev_info(&pdev->dev, "get m4u port name: %s (%d), index: %d fail %d",
				name, reg_index, i, ret);
			return -EINVAL;
		}
		dev->dec_m4u_ports[reg_index] = (int)port_id;
		mtk_v4l2_debug(2, "dec_m4u_ports[%d]=0x%x",
			reg_index, dev->dec_m4u_ports[reg_index]);
	}

#if IS_ENABLED(CONFIG_MTK_IOMMU)
	dev->io_domain = iommu_get_domain_for_dev(&pdev->dev);
	if (dev->io_domain == NULL) {
		mtk_v4l2_err("Failed to get io_domain\n");
		return -EPROBE_DEFER;
	}

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
	if (ret) {
		ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
		if (ret) {
			dev_info(&pdev->dev, "64-bit DMA enable failed\n");
			return ret;
		}
	}
	if (!pdev->dev.dma_parms) {
		pdev->dev.dma_parms =
			devm_kzalloc(&pdev->dev, sizeof(*pdev->dev.dma_parms), GFP_KERNEL);
	}
	if (pdev->dev.dma_parms) {
		ret = dma_set_max_seg_size(&pdev->dev, (unsigned int)DMA_BIT_MASK(34));
		if (ret)
			dev_info(&pdev->dev, "Failed to set DMA segment size\n");
	}

	mtk_vdec_translation_fault_callback_setting(dev);
#endif
	mtk_v4l2_debug(0, "decoder registered as /dev/video%d",
				   vfd_dec->num);

	mtk_prepare_vdec_dvfs(dev);
	mtk_prepare_vdec_emi_bw(dev);
	dev->pm_notifier.notifier_call = mtk_vcodec_dec_suspend_notifier;
	register_pm_notifier(&dev->pm_notifier);
	dev->is_codec_suspending = 0;
	dev->dec_cnt = 0;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	vdec_vcp_probe(dev);
#endif

	INIT_LIST_HEAD(&dev->log_param_list);
	INIT_LIST_HEAD(&dev->prop_param_list);
	dev_ptr = dev;

	return 0;

err_dec_reg:
	destroy_workqueue(dev->decode_workqueue);
err_event_workq:
	v4l2_m2m_release(dev->m2m_dev_dec);
err_dec_mem_init:
	video_unregister_device(vfd_dec);
err_dec_alloc:
	v4l2_device_unregister(&dev->v4l2_dev);
err_res:
	mtk_vcodec_release_dec_pm(dev);
	return ret;
}

static const struct of_device_id mtk_vcodec_match[] = {
	{.compatible = "mediatek,mt8173-vcodec-dec",},
	{.compatible = "mediatek,mt2712-vcodec-dec",},
	{.compatible = "mediatek,mt8167-vcodec-dec",},
	{.compatible = "mediatek,mt6771-vcodec-dec",},
	{.compatible = "mediatek,mt6885-vcodec-dec",},
	{.compatible = "mediatek,mt6873-vcodec-dec",},
	{.compatible = "mediatek,mt6853-vcodec-dec",},
	{.compatible = "mediatek,mt6983-vcodec-dec",},
	{.compatible = "mediatek,mt6879-vcodec-dec",},
	{.compatible = "mediatek,mt6895-vcodec-dec",},
	{.compatible = "mediatek,mt6855-vcodec-dec",},
	{.compatible = "mediatek,vdec_gcon",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_vcodec_match);

static int mtk_vcodec_dec_remove(struct platform_device *pdev)
{
	struct mtk_vcodec_dev *dev = platform_get_drvdata(pdev);

	mtk_unprepare_vdec_emi_bw(dev);
	mtk_unprepare_vdec_dvfs(dev);

	flush_workqueue(dev->decode_workqueue);
	destroy_workqueue(dev->decode_workqueue);
	if (dev->m2m_dev_dec)
		v4l2_m2m_release(dev->m2m_dev_dec);

	if (dev->vfd_dec)
		video_unregister_device(dev->vfd_dec);

	v4l2_device_unregister(&dev->v4l2_dev);
	mtk_vcodec_release_dec_pm(dev);
	return 0;
}

static const struct dev_pm_ops mtk_vcodec_dec_pm_ops = {
	.suspend = mtk_vcodec_dec_suspend,
	.resume = mtk_vcodec_dec_resume,
};

static struct platform_driver mtk_vcodec_dec_driver = {
	.probe  = mtk_vcodec_dec_probe,
	.remove = mtk_vcodec_dec_remove,
	.driver = {
		.name   = MTK_VCODEC_DEC_NAME,
		.pm = &mtk_vcodec_dec_pm_ops,
		.of_match_table = mtk_vcodec_match,
	},
};

module_platform_driver(mtk_vcodec_dec_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek video codec V4L2 decoder driver");
