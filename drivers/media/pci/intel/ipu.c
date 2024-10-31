// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2013 - 2024 Intel Corporation

#include <linux/acpi.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/pci-ats.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/version.h>

#include "ipu.h"
#include "ipu-buttress.h"
#include "ipu-platform.h"
#include "ipu-platform-buttress-regs.h"
#include "ipu-cpd.h"
#include "ipu-pdata.h"
#include "ipu-bus.h"
#include "ipu-mmu.h"
#include "ipu-platform-regs.h"
#include "ipu-platform-isys-csi2-reg.h"
#include "ipu-trace.h"
#if IS_ENABLED(CONFIG_IPU_BRIDGE) && \
LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#include <media/ipu-bridge.h>
#endif

#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA)
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
#include <media/ipu-isys.h>
#endif
#endif

#if IS_ENABLED(CONFIG_INTEL_IPU6_ACPI)
#include <media/ipu-acpi.h>
#endif

#define IPU_PCI_BAR		0
enum ipu_version ipu_ver;
EXPORT_SYMBOL(ipu_ver);

static int isys_freq_override = -1;
module_param(isys_freq_override, int, 0660);
MODULE_PARM_DESC(isys_freq_override, "override isys freq default value");

static int psys_freq_override = -1;
module_param(psys_freq_override, int, 0660);
MODULE_PARM_DESC(psys_freq_override, "override psys freq default value");

#if IS_ENABLED(CONFIG_INTEL_IPU6_ACPI)
static int isys_init_acpi_add_device(struct device *dev, void *priv,
				struct ipu_isys_csi2_config *csi2,
				bool reprobe)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_IPU_BRIDGE) && LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
static int ipu_isys_check_fwnode_graph(struct fwnode_handle *fwnode)
{
	struct fwnode_handle *endpoint;

	if (IS_ERR_OR_NULL(fwnode))
		return -EINVAL;

	endpoint = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (endpoint) {
		fwnode_handle_put(endpoint);
		return 0;
	}

	return ipu_isys_check_fwnode_graph(fwnode->secondary);
}
#endif

static struct ipu_bus_device *ipu_isys_init(struct pci_dev *pdev,
					    struct device *parent,
					    struct ipu_buttress_ctrl *ctrl,
					    void __iomem *base,
					    const struct ipu_isys_internal_pdata
					    *ipdata,
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA)
					    struct ipu_isys_subdev_pdata
					    *spdata,
#endif
					    unsigned int nr)
{
	struct ipu_bus_device *isys;
	struct ipu_isys_pdata *pdata;
#if IS_ENABLED(CONFIG_INTEL_IPU6_ACPI)
	struct ipu_isys_subdev_pdata *acpi_pdata;
#endif
	int ret;
#if IS_ENABLED(CONFIG_IPU_BRIDGE) && LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
struct fwnode_handle *fwnode = dev_fwnode(&pdev->dev);

	ret = ipu_isys_check_fwnode_graph(fwnode);
	if (ret) {
		if (fwnode && !IS_ERR_OR_NULL(fwnode->secondary)) {
			dev_err(&pdev->dev,
				"fwnode graph has no endpoints connection\n");
			return ERR_PTR(-EINVAL);
		}

		ret = ipu_bridge_init(&pdev->dev, ipu_bridge_parse_ssdb);
		if (ret) {
			dev_err_probe(&pdev->dev, ret,
				      "IPU bridge init failed\n");
			return ERR_PTR(ret);
		}
	}
#endif

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->base = base;
	pdata->ipdata = ipdata;
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA)
	pdata->spdata = spdata;
#endif

	/* Use 250MHz for ipu6 se */
	if (ipu_ver == IPU_VER_6SE)
		ctrl->ratio = IPU6SE_IS_FREQ_CTL_DEFAULT_RATIO;

	isys = ipu_bus_initialize_device(pdev, parent, pdata, ctrl,
					 IPU_ISYS_NAME, nr);
	if (IS_ERR(isys)) {
		dev_err_probe(&pdev->dev, PTR_ERR(isys),
			      "ipu_bus_initialize_device(isys) failed\n");
		kfree(pdata);
		return isys;
	}
#if IS_ENABLED(CONFIG_INTEL_IPU6_ACPI)
	if (!spdata) {
		dev_dbg(&pdev->dev, "No subdevice info provided");
		ipu_get_acpi_devices(isys, &isys->dev, &acpi_pdata, NULL,
				     isys_init_acpi_add_device);
		pdata->spdata = acpi_pdata;
	} else {
		dev_dbg(&pdev->dev, "Subdevice info found");
		ipu_get_acpi_devices(isys, &isys->dev, &acpi_pdata, &spdata,
				     isys_init_acpi_add_device);
	}
#endif
	isys->mmu = ipu_mmu_init(&pdev->dev, base, ISYS_MMID,
				 &ipdata->hw_variant);
	if (IS_ERR(isys->mmu)) {
		dev_err_probe(&pdev->dev, PTR_ERR(isys->mmu),
			      "ipu_mmu_init(isys->mmu) failed\n");
		put_device(&isys->dev);
		return ERR_CAST(isys->mmu);
	}

	isys->mmu->dev = &isys->dev;

	ret = ipu_bus_add_device(isys);
	if (ret)
		return ERR_PTR(ret);

	return isys;
}

static struct ipu_bus_device *ipu_psys_init(struct pci_dev *pdev,
					    struct device *parent,
					    struct ipu_buttress_ctrl *ctrl,
					    void __iomem *base,
					    const struct ipu_psys_internal_pdata
					    *ipdata, unsigned int nr)
{
	struct ipu_bus_device *psys;
	struct ipu_psys_pdata *pdata;
	int ret;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->base = base;
	pdata->ipdata = ipdata;

	psys = ipu_bus_initialize_device(pdev, parent, pdata, ctrl,
					 IPU_PSYS_NAME, nr);
	if (IS_ERR(psys)) {
		dev_err_probe(&pdev->dev, PTR_ERR(psys),
			      "ipu_bus_initialize_device(psys) failed\n");
		kfree(pdata);
		return psys;
	}

	psys->mmu = ipu_mmu_init(&pdev->dev, base, PSYS_MMID,
				 &ipdata->hw_variant);
	if (IS_ERR(psys->mmu)) {
		dev_err_probe(&pdev->dev, PTR_ERR(psys->mmu),
			      "ipu_mmu_init(psys->mmu) failed\n");
		put_device(&psys->dev);
		return ERR_CAST(psys->mmu);
	}

	psys->mmu->dev = &psys->dev;

	ret = ipu_bus_add_device(psys);
	if (ret)
		return ERR_PTR(ret);

	return psys;
}

int ipu_fw_authenticate(void *data, u64 val)
{
	struct ipu_device *isp = data;
	int ret;

	if (!isp->secure_mode)
		return -EINVAL;

	ret = ipu_buttress_reset_authentication(isp);
	if (ret) {
		dev_err(&isp->pdev->dev, "Failed to reset authentication!\n");
		return ret;
	}

	ret = pm_runtime_get_sync(&isp->psys->dev);
	if (ret < 0) {
		dev_err(&isp->pdev->dev, "Runtime PM failed (%d)\n", ret);
		return ret;
	}

	ret = ipu_buttress_authenticate(isp);
	if (ret) {
		dev_err(&isp->pdev->dev, "FW authentication failed\n");
		return ret;
	}

	pm_runtime_put(&isp->psys->dev);

	return 0;
}
EXPORT_SYMBOL(ipu_fw_authenticate);
DEFINE_SIMPLE_ATTRIBUTE(authenticate_fops, NULL, ipu_fw_authenticate, "%llu\n");

#ifdef CONFIG_DEBUG_FS
static int resume_ipu_bus_device(struct ipu_bus_device *adev)
{
	struct device *dev = &adev->dev;
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (!pm || !pm->resume)
		return -EIO;

	return pm->resume(dev);
}

static int suspend_ipu_bus_device(struct ipu_bus_device *adev)
{
	struct device *dev = &adev->dev;
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (!pm || !pm->suspend)
		return -EIO;

	return pm->suspend(dev);
}

static int force_suspend_get(void *data, u64 *val)
{
	struct ipu_device *isp = data;
	struct ipu_buttress *b = &isp->buttress;

	*val = b->force_suspend;
	return 0;
}

static int force_suspend_set(void *data, u64 val)
{
	struct ipu_device *isp = data;
	struct ipu_buttress *b = &isp->buttress;
	int ret = 0;

	if (val == b->force_suspend)
		return 0;

	if (val) {
		b->force_suspend = 1;
		ret = suspend_ipu_bus_device(isp->psys);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to suspend psys\n");
			return ret;
		}
		ret = suspend_ipu_bus_device(isp->isys);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to suspend isys\n");
			return ret;
		}
		ret = pci_set_power_state(isp->pdev, PCI_D3hot);
		if (ret) {
			dev_err(&isp->pdev->dev,
				"Failed to suspend IUnit PCI device\n");
			return ret;
		}
	} else {
		ret = pci_set_power_state(isp->pdev, PCI_D0);
		if (ret) {
			dev_err(&isp->pdev->dev,
				"Failed to suspend IUnit PCI device\n");
			return ret;
		}
		ret = resume_ipu_bus_device(isp->isys);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to resume isys\n");
			return ret;
		}
		ret = resume_ipu_bus_device(isp->psys);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to resume psys\n");
			return ret;
		}
		b->force_suspend = 0;
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(force_suspend_fops, force_suspend_get,
			force_suspend_set, "%llu\n");
/*
 * The sysfs interface for reloading cpd fw is there only for debug purpose,
 * and it must not be used when either isys or psys is in use.
 */
static int cpd_fw_reload(void *data, u64 val)
{
	struct ipu_device *isp = data;
	int rval = -EINVAL;

	if (isp->cpd_fw_reload)
		rval = isp->cpd_fw_reload(isp);

	return rval;
}

DEFINE_SIMPLE_ATTRIBUTE(cpd_fw_fops, NULL, cpd_fw_reload, "%llu\n");

static int ipu_init_debugfs(struct ipu_device *isp)
{
	struct dentry *file;
	struct dentry *dir;

	dir = debugfs_create_dir(IPU_NAME, NULL);
	if (!dir)
		return -ENOMEM;

	file = debugfs_create_file("force_suspend", 0700, dir, isp,
				   &force_suspend_fops);
	if (!file)
		goto err;
	file = debugfs_create_file("authenticate", 0700, dir, isp,
				   &authenticate_fops);
	if (!file)
		goto err;

	file = debugfs_create_file("cpd_fw_reload", 0700, dir, isp,
				   &cpd_fw_fops);
	if (!file)
		goto err;

	if (ipu_trace_debugfs_add(isp, dir))
		goto err;

	isp->ipu_dir = dir;

	if (ipu_buttress_debugfs_init(isp))
		goto err;

	return 0;
err:
	debugfs_remove_recursive(dir);
	return -ENOMEM;
}

static void ipu_remove_debugfs(struct ipu_device *isp)
{
	/*
	 * Since isys and psys debugfs dir will be created under ipu root dir,
	 * mark its dentry to NULL to avoid duplicate removal.
	 */
	debugfs_remove_recursive(isp->ipu_dir);
	isp->ipu_dir = NULL;
}
#endif /* CONFIG_DEBUG_FS */

static int ipu_pci_config_setup(struct pci_dev *dev)
{
	u16 pci_command;
	int rval;

	pci_read_config_word(dev, PCI_COMMAND, &pci_command);
	pci_command |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
	pci_write_config_word(dev, PCI_COMMAND, pci_command);

	/* disable IPU6 PCI ATS on mtl ES2 */
	if ((boot_cpu_data.x86_model == 0xac ||
	     boot_cpu_data.x86_model == 0xaa) &&
	    boot_cpu_data.x86_stepping == 0x2 && pci_ats_supported(dev))
		pci_disable_ats(dev);

	/* no msi pci capability for IPU6EP */
	if (ipu_ver == IPU_VER_6EP || ipu_ver == IPU_VER_6EP_MTL) {
		/* likely do nothing as msi not enabled by default */
		pci_disable_msi(dev);
		return 0;
	}

	rval = pci_enable_msi(dev);
	if (rval)
		dev_err(&dev->dev, "Failed to enable msi (%d)\n", rval);

	return rval;
}

static void ipu_configure_vc_mechanism(struct ipu_device *isp)
{
	u32 val = readl(isp->base + BUTTRESS_REG_BTRS_CTRL);

	if (IPU_BTRS_ARB_STALL_MODE_VC0 == IPU_BTRS_ARB_MODE_TYPE_STALL)
		val |= BUTTRESS_REG_BTRS_CTRL_STALL_MODE_VC0;
	else
		val &= ~BUTTRESS_REG_BTRS_CTRL_STALL_MODE_VC0;

	if (IPU_BTRS_ARB_STALL_MODE_VC1 == IPU_BTRS_ARB_MODE_TYPE_STALL)
		val |= BUTTRESS_REG_BTRS_CTRL_STALL_MODE_VC1;
	else
		val &= ~BUTTRESS_REG_BTRS_CTRL_STALL_MODE_VC1;

	writel(val, isp->base + BUTTRESS_REG_BTRS_CTRL);
}

int request_cpd_fw(const struct firmware **firmware_p, const char *name,
		   struct device *device)
{
	const struct firmware *fw;
	struct firmware *tmp;
	int ret;

	ret = request_firmware(&fw, name, device);
	if (ret)
		return ret;

	if (is_vmalloc_addr(fw->data)) {
		*firmware_p = fw;
	} else {
		tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
		if (!tmp) {
			release_firmware(fw);
			return -ENOMEM;
		}
		tmp->size = fw->size;
		tmp->data = vmalloc(fw->size);
		if (!tmp->data) {
			kfree(tmp);
			release_firmware(fw);
			return -ENOMEM;
		}
		memcpy((void *)tmp->data, fw->data, fw->size);
		*firmware_p = tmp;
		release_firmware(fw);
	}

	return 0;
}
EXPORT_SYMBOL(request_cpd_fw);

#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA)
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
static inline int match_spdata(struct ipu_isys_subdev_info *sd,
			const struct ipu_spdata_rep *rep)
{
	if (strcmp(sd->i2c.board_info.type, rep->name))
		return 0;

	if (strcmp(sd->i2c.i2c_adapter_bdf, rep->i2c_adapter_bdf_o))
		return 0;

	if (sd->i2c.board_info.addr != rep->slave_addr_o)
		return 0;

	if (sd->csi2->port != rep->port_o)
		return 0;

	return 1;
}

static void fixup_spdata(const void *spdata_rep,
			 struct ipu_isys_subdev_pdata *spdata)
{
	const struct ipu_spdata_rep *rep = spdata_rep;
	struct ipu_isys_subdev_info **subdevs, *sd_info;

	if (!spdata)
		return;

	for (; rep->name[0]; rep++) {
		for (subdevs = spdata->subdevs; *subdevs; subdevs++) {
			sd_info = *subdevs;

			if (!sd_info->csi2)
				continue;

			if (match_spdata(sd_info, rep)) {
				strcpy(sd_info->i2c.i2c_adapter_bdf,
						rep->i2c_adapter_bdf_n);
				sd_info->i2c.board_info.addr =
					rep->slave_addr_n;
				sd_info->csi2->port = rep->port_n;

				if (sd_info->fixup_spdata)
					sd_info->fixup_spdata(rep,
					sd_info->i2c.board_info.platform_data);
			}
		}
	}
}
#endif
#endif

static int ipu_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct ipu_device *isp;
	phys_addr_t phys;
	void __iomem *const *iomap;
	void __iomem *isys_base = NULL;
	void __iomem *psys_base = NULL;
	struct ipu_buttress_ctrl *isys_ctrl = NULL, *psys_ctrl = NULL;
	unsigned int dma_mask = IPU_DMA_MASK;
	struct fwnode_handle *fwnode = dev_fwnode(&pdev->dev);
	u32 is_es;
	int rval;
	u32 val;

	if (!fwnode || fwnode_property_read_u32(fwnode, "is_es", &is_es))
		is_es = 0;

	isp = devm_kzalloc(&pdev->dev, sizeof(*isp), GFP_KERNEL);
	if (!isp)
		return -ENOMEM;

	isp->pdev = pdev;
	INIT_LIST_HEAD(&isp->devices);

	rval = pcim_enable_device(pdev);
	if (rval) {
		dev_err(&pdev->dev, "Failed to enable CI ISP device (%d)\n",
			rval);
		return rval;
	}

	dev_info(&pdev->dev, "Device 0x%x (rev: 0x%x)\n",
		 pdev->device, pdev->revision);

	phys = pci_resource_start(pdev, IPU_PCI_BAR);

	rval = pcim_iomap_regions(pdev,
				  1 << IPU_PCI_BAR,
				  pci_name(pdev));
	if (rval) {
		dev_err(&pdev->dev, "Failed to I/O memory remapping (%d)\n",
			rval);
		return rval;
	}
	dev_info(&pdev->dev, "physical base address 0x%llx\n", phys);

	iomap = pcim_iomap_table(pdev);
	if (!iomap) {
		dev_err(&pdev->dev, "Failed to iomap table (%d)\n", rval);
		return -ENODEV;
	}

	isp->base = iomap[IPU_PCI_BAR];
	dev_info(&pdev->dev, "mapped as: 0x%p\n", isp->base);

	pci_set_drvdata(pdev, isp);
	pci_set_master(pdev);

	switch (id->device) {
	case IPU6_PCI_ID:
		ipu_ver = IPU_VER_6;
		isp->cpd_fw_name = IPU6_FIRMWARE_NAME;
		isp->cpd_fw_name_new = IPU6_FIRMWARE_NAME_NEW;
		break;
	case IPU6SE_PCI_ID:
		ipu_ver = IPU_VER_6SE;
		isp->cpd_fw_name = IPU6SE_FIRMWARE_NAME;
		isp->cpd_fw_name_new = IPU6SE_FIRMWARE_NAME_NEW;
		break;
	case IPU6EP_ADL_P_PCI_ID:
	case IPU6EP_RPL_P_PCI_ID:
		ipu_ver = IPU_VER_6EP;
		isp->cpd_fw_name = is_es ? IPU6EPES_FIRMWARE_NAME : IPU6EP_FIRMWARE_NAME;
		isp->cpd_fw_name_new = is_es ? IPU6EPES_FIRMWARE_NAME_NEW
					     : IPU6EP_FIRMWARE_NAME_NEW;
		break;
	case IPU6EP_ADL_N_PCI_ID:
		ipu_ver = IPU_VER_6EP;
		isp->cpd_fw_name = IPU6EPADLN_FIRMWARE_NAME;
		isp->cpd_fw_name_new = IPU6EPADLN_FIRMWARE_NAME_NEW;
		break;
	case IPU6EP_MTL_PCI_ID:
		ipu_ver = IPU_VER_6EP_MTL;
		isp->cpd_fw_name = is_es ? IPU6EPMTLES_FIRMWARE_NAME
					 : IPU6EPMTL_FIRMWARE_NAME;
		isp->cpd_fw_name_new = is_es ? IPU6EPMTLES_FIRMWARE_NAME_NEW
					     : IPU6EPMTL_FIRMWARE_NAME_NEW;
		break;
	default:
		WARN(1, "Unsupported IPU device");
		return -ENODEV;
	}

	ipu_internal_pdata_init();

	isys_base = isp->base + isys_ipdata.hw_variant.offset;
	psys_base = isp->base + psys_ipdata.hw_variant.offset;

	dev_dbg(&pdev->dev, "isys_base: 0x%lx\n", (unsigned long)isys_base);
	dev_dbg(&pdev->dev, "psys_base: 0x%lx\n", (unsigned long)psys_base);

	rval = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(dma_mask));
	if (rval) {
		dev_err(&pdev->dev, "Failed to set DMA mask (%d)\n", rval);
		return rval;
	}

	dma_set_max_seg_size(&pdev->dev, UINT_MAX);

	rval = ipu_pci_config_setup(pdev);
	if (rval)
		return rval;

	rval = devm_request_threaded_irq(&pdev->dev, pdev->irq,
					 ipu_buttress_isr,
					 ipu_buttress_isr_threaded,
					 IRQF_SHARED, IPU_NAME, isp);
	if (rval) {
		dev_err(&pdev->dev, "Requesting irq failed(%d)\n", rval);
		return rval;
	}

	rval = ipu_buttress_init(isp);
	if (rval)
		return rval;

	dev_dbg(&pdev->dev, "cpd file name: %s\n", isp->cpd_fw_name);
	rval = request_cpd_fw(&isp->cpd_fw, isp->cpd_fw_name, &pdev->dev);
	if (rval == -ENOENT) {
		/* Try again with new FW path */
		dev_dbg(&pdev->dev, "cpd file name: %s\n",
			isp->cpd_fw_name_new);
		rval = request_cpd_fw(&isp->cpd_fw, isp->cpd_fw_name_new,
				      &pdev->dev);
	}

	if (rval) {
		dev_err(&isp->pdev->dev, "Requesting signed firmware failed\n");
		goto buttress_exit;
	}

	rval = ipu_cpd_validate_cpd_file(isp, isp->cpd_fw->data,
					 isp->cpd_fw->size);
	if (rval) {
		dev_err(&isp->pdev->dev, "Failed to validate cpd\n");
		goto out_ipu_bus_del_devices;
	}

#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA)
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
	rval = request_firmware(&isp->spdata_fw, IPU_SPDATA_NAME, &pdev->dev);
	if (rval)
		dev_warn(&isp->pdev->dev, "no spdata replace, using default\n");
	else
		fixup_spdata(isp->spdata_fw->data, pdev->dev.platform_data);
#endif
#endif
	rval = ipu_trace_add(isp);
	if (rval)
		dev_err(&pdev->dev, "Trace support not available\n");

	/*
	 * NOTE Device hierarchy below is important to ensure proper
	 * runtime suspend and resume order.
	 * Also registration order is important to ensure proper
	 * suspend and resume order during system
	 * suspend. Registration order is as follows:
	 * isys->psys
	 */
	isys_ctrl = devm_kzalloc(&pdev->dev, sizeof(*isys_ctrl), GFP_KERNEL);
	if (!isys_ctrl) {
		rval = -ENOMEM;
		goto out_ipu_bus_del_devices;
	}

	/* Init butress control with default values based on the HW */
	memcpy(isys_ctrl, &isys_buttress_ctrl, sizeof(*isys_ctrl));

	isp->isys = ipu_isys_init(pdev, &pdev->dev,
				  isys_ctrl, isys_base,
				  &isys_ipdata,
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA)
				  pdev->dev.platform_data,
#endif
				  0);
	if (IS_ERR(isp->isys)) {
		rval = PTR_ERR(isp->isys);
		goto out_ipu_bus_del_devices;
	}

	if (isys_freq_override >= BUTTRESS_MIN_FORCE_IS_FREQ &&
		isys_freq_override <= BUTTRESS_MAX_FORCE_IS_FREQ) {
		u64 val = isys_freq_override;

		do_div(val, BUTTRESS_IS_FREQ_STEP);
		isys_ctrl->divisor = val;
		dev_info(&isp->pdev->dev,
			 "set isys freq as (%d), actually set (%d)\n",
			 isys_freq_override,
			 isys_ctrl->divisor * BUTTRESS_IS_FREQ_STEP);
	}

	psys_ctrl = devm_kzalloc(&pdev->dev, sizeof(*psys_ctrl), GFP_KERNEL);
	if (!psys_ctrl) {
		rval = -ENOMEM;
		goto out_ipu_bus_del_devices;
	}

	/* Init butress control with default values based on the HW */
	memcpy(psys_ctrl, &psys_buttress_ctrl, sizeof(*psys_ctrl));

	isp->psys = ipu_psys_init(pdev, &isp->isys->dev,
				  psys_ctrl, psys_base,
				  &psys_ipdata, 0);
	if (IS_ERR(isp->psys)) {
		rval = PTR_ERR(isp->psys);
		goto out_ipu_bus_del_devices;
	}

	if (psys_freq_override >= BUTTRESS_MIN_FORCE_PS_FREQ &&
		psys_freq_override <= BUTTRESS_MAX_FORCE_PS_FREQ) {
		u64 val = psys_freq_override;

		do_div(val, BUTTRESS_PS_FREQ_STEP);
		psys_ctrl->divisor = val;
		psys_ctrl->qos_floor = val;
		dev_info(&isp->pdev->dev,
			 "adjusted psys freq from input (%d) and set (%d)\n",
			 psys_freq_override,
			 psys_ctrl->divisor * BUTTRESS_PS_FREQ_STEP);
	}
	rval = pm_runtime_get_sync(&isp->psys->dev);
	if (rval < 0) {
		dev_err(&isp->psys->dev, "Failed to get runtime PM\n");
		goto out_ipu_bus_del_devices;
	}

	rval = ipu_mmu_hw_init(isp->psys->mmu);
	if (rval) {
		dev_err(&isp->pdev->dev, "Failed to set mmu hw\n");
		goto out_ipu_bus_del_devices;
	}

	rval = ipu_buttress_map_fw_image(isp->psys, isp->cpd_fw,
					 &isp->fw_sgt);
	if (rval) {
		dev_err(&isp->pdev->dev, "failed to map fw image\n");
		goto out_ipu_bus_del_devices;
	}

	isp->pkg_dir = ipu_cpd_create_pkg_dir(isp->psys,
					      isp->cpd_fw->data,
					      sg_dma_address(isp->fw_sgt.sgl),
					      &isp->pkg_dir_dma_addr,
					      &isp->pkg_dir_size);
	if (!isp->pkg_dir) {
		rval = -ENOMEM;
		dev_err(&isp->pdev->dev, "failed to create pkg dir\n");
		goto out_ipu_bus_del_devices;
	}

	rval = ipu_buttress_authenticate(isp);
	if (rval) {
		dev_err(&isp->pdev->dev, "FW authentication failed(%d)\n",
			rval);
		goto out_ipu_bus_del_devices;
	}

	ipu_mmu_hw_cleanup(isp->psys->mmu);
	pm_runtime_put(&isp->psys->dev);

#ifdef CONFIG_DEBUG_FS
	rval = ipu_init_debugfs(isp);
	if (rval) {
		dev_err(&pdev->dev, "Failed to initialize debugfs");
		goto out_ipu_bus_del_devices;
	}
#endif

	/* Configure the arbitration mechanisms for VC requests */
	ipu_configure_vc_mechanism(isp);

	val = readl(isp->base + BUTTRESS_REG_SKU);
	dev_info(&pdev->dev, "IPU%u-v%u driver version %d.%d\n",
		 val & 0xf, (val >> 4) & 0x7,
		 IPU_MAJOR_VERSION,
		 IPU_MINOR_VERSION);

	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&pdev->dev);

	isp->ipu_bus_ready_to_probe = true;

	return 0;

out_ipu_bus_del_devices:
	if (isp->pkg_dir) {
		if (isp->psys) {
			ipu_cpd_free_pkg_dir(isp->psys, isp->pkg_dir,
					     isp->pkg_dir_dma_addr,
					     isp->pkg_dir_size);
			ipu_buttress_unmap_fw_image(isp->psys, &isp->fw_sgt);
		}
		isp->pkg_dir = NULL;
	}
	if (!IS_ERR_OR_NULL(isp->psys) && !IS_ERR_OR_NULL(isp->psys->mmu))
		ipu_mmu_cleanup(isp->psys->mmu);
	if (!IS_ERR_OR_NULL(isp->isys) && !IS_ERR_OR_NULL(isp->isys->mmu))
		ipu_mmu_cleanup(isp->isys->mmu);
	if (!IS_ERR_OR_NULL(isp->psys))
		pm_runtime_put(&isp->psys->dev);
	ipu_bus_del_devices(pdev);
	release_firmware(isp->cpd_fw);
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_USE_PLATFORMDATA)
#if IS_ENABLED(CONFIG_VIDEO_INTEL_IPU_PDATA_DYNAMIC_LOADING)
	release_firmware(isp->spdata_fw);
#endif
#endif
buttress_exit:
	ipu_buttress_exit(isp);

	return rval;
}

static void ipu_pci_remove(struct pci_dev *pdev)
{
	struct ipu_device *isp = pci_get_drvdata(pdev);

#ifdef CONFIG_DEBUG_FS
	ipu_remove_debugfs(isp);
#endif
	ipu_trace_release(isp);

	ipu_cpd_free_pkg_dir(isp->psys, isp->pkg_dir, isp->pkg_dir_dma_addr,
			     isp->pkg_dir_size);

	ipu_buttress_unmap_fw_image(isp->psys, &isp->fw_sgt);

	isp->pkg_dir = NULL;
	isp->pkg_dir_dma_addr = 0;
	isp->pkg_dir_size = 0;

	ipu_mmu_cleanup(isp->psys->mmu);
	ipu_mmu_cleanup(isp->isys->mmu);

	ipu_bus_del_devices(pdev);

	pm_runtime_forbid(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);

	ipu_buttress_exit(isp);

	release_firmware(isp->cpd_fw);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
static void ipu_pci_reset_notify(struct pci_dev *pdev, bool prepare)
{
	struct ipu_device *isp = pci_get_drvdata(pdev);

	if (prepare) {
		dev_err(&pdev->dev, "FLR prepare\n");
		pm_runtime_forbid(&isp->pdev->dev);
		isp->flr_done = true;
		return;
	}

	ipu_buttress_restore(isp);
	if (isp->secure_mode)
		ipu_buttress_reset_authentication(isp);

	ipu_bus_flr_recovery();
	isp->ipc_reinit = true;
	pm_runtime_allow(&isp->pdev->dev);

	dev_err(&pdev->dev, "FLR completed\n");
}
#else
static void ipu_pci_reset_prepare(struct pci_dev *pdev)
{
	struct ipu_device *isp = pci_get_drvdata(pdev);

	dev_warn(&pdev->dev, "FLR prepare\n");
	pm_runtime_forbid(&isp->pdev->dev);
	isp->flr_done = true;
}

static void ipu_pci_reset_done(struct pci_dev *pdev)
{
	struct ipu_device *isp = pci_get_drvdata(pdev);

	ipu_buttress_restore(isp);
	if (isp->secure_mode)
		ipu_buttress_reset_authentication(isp);

	ipu_bus_flr_recovery();
	isp->ipc_reinit = true;
	pm_runtime_allow(&isp->pdev->dev);

	dev_warn(&pdev->dev, "FLR completed\n");
}
#endif

#ifdef CONFIG_PM

/*
 * PCI base driver code requires driver to provide these to enable
 * PCI device level PM state transitions (D0<->D3)
 */
static int ipu_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ipu_device *isp = pci_get_drvdata(pdev);

	isp->flr_done = false;

	return 0;
}

static int ipu_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ipu_device *isp = pci_get_drvdata(pdev);
	struct ipu_buttress *b = &isp->buttress;
	int rval;

	/* Configure the arbitration mechanisms for VC requests */
	ipu_configure_vc_mechanism(isp);

	ipu_buttress_set_secure_mode(isp);
	isp->secure_mode = ipu_buttress_get_secure_mode(isp);
	dev_info(dev, "IPU in %s mode\n",
		 isp->secure_mode ? "secure" : "non-secure");

	ipu_buttress_restore(isp);

	rval = ipu_buttress_ipc_reset(isp, &b->cse);
	if (rval)
		dev_err(&isp->pdev->dev, "IPC reset protocol failed!\n");

	rval = pm_runtime_get_sync(&isp->psys->dev);
	if (rval < 0) {
		dev_err(&isp->psys->dev, "Failed to get runtime PM\n");
		return 0;
	}

	rval = ipu_buttress_authenticate(isp);
	if (rval)
		dev_err(&isp->pdev->dev, "FW authentication failed(%d)\n",
			rval);

	pm_runtime_put(&isp->psys->dev);

	return 0;
}

static int ipu_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ipu_device *isp = pci_get_drvdata(pdev);
	int rval;

	ipu_configure_vc_mechanism(isp);
	ipu_buttress_restore(isp);

	if (isp->ipc_reinit) {
		struct ipu_buttress *b = &isp->buttress;

		isp->ipc_reinit = false;
		rval = ipu_buttress_ipc_reset(isp, &b->cse);
		if (rval)
			dev_err(&isp->pdev->dev,
				"IPC reset protocol failed!\n");
	}

	return 0;
}

static const struct dev_pm_ops ipu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(&ipu_suspend, &ipu_resume)
	    SET_RUNTIME_PM_OPS(&ipu_suspend,	/* Same as in suspend flow */
			       &ipu_runtime_resume,
			       NULL)
};

#define IPU_PM (&ipu_pm_ops)
#else
#define IPU_PM NULL
#endif

static const struct pci_device_id ipu_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IPU6_PCI_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IPU6SE_PCI_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IPU6EP_ADL_P_PCI_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IPU6EP_ADL_N_PCI_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IPU6EP_RPL_P_PCI_ID)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IPU6EP_MTL_PCI_ID)},
	{0,}
};
MODULE_DEVICE_TABLE(pci, ipu_pci_tbl);

static const struct pci_error_handlers pci_err_handlers = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
	.reset_notify = ipu_pci_reset_notify,
#else
	.reset_prepare = ipu_pci_reset_prepare,
	.reset_done = ipu_pci_reset_done,
#endif
};

static struct pci_driver ipu_pci_driver = {
	.name = IPU_NAME,
	.id_table = ipu_pci_tbl,
	.probe = ipu_pci_probe,
	.remove = ipu_pci_remove,
	.driver = {
		   .pm = IPU_PM,
		   },
	.err_handler = &pci_err_handlers,
};

static int __init ipu_init(void)
{
	int rval = ipu_bus_register();

	if (rval) {
		pr_warn("can't register ipu bus (%d)\n", rval);
		return rval;
	}

	rval = pci_register_driver(&ipu_pci_driver);
	if (rval) {
		pr_warn("can't register pci driver (%d)\n", rval);
		goto out_pci_register_driver;
	}

	return 0;

out_pci_register_driver:
	ipu_bus_unregister();

	return rval;
}

static void __exit ipu_exit(void)
{
	pci_unregister_driver(&ipu_pci_driver);
	ipu_bus_unregister();
}

module_init(ipu_init);
module_exit(ipu_exit);

#if IS_ENABLED(CONFIG_IPU_BRIDGE) && \
LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
MODULE_IMPORT_NS(INTEL_IPU_BRIDGE);
#endif
MODULE_AUTHOR("Sakari Ailus <sakari.ailus@linux.intel.com>");
MODULE_AUTHOR("Jouni Högander <jouni.hogander@intel.com>");
MODULE_AUTHOR("Antti Laakso <antti.laakso@intel.com>");
MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_AUTHOR("Jianxu Zheng <jian.xu.zheng@intel.com>");
MODULE_AUTHOR("Tianshu Qiu <tian.shu.qiu@intel.com>");
MODULE_AUTHOR("Renwei Wu <renwei.wu@intel.com>");
MODULE_AUTHOR("Bingbu Cao <bingbu.cao@intel.com>");
MODULE_AUTHOR("Yunliang Ding <yunliang.ding@intel.com>");
MODULE_AUTHOR("Zaikuo Wang <zaikuo.wang@intel.com>");
MODULE_AUTHOR("Leifu Zhao <leifu.zhao@intel.com>");
MODULE_AUTHOR("Xia Wu <xia.wu@intel.com>");
MODULE_AUTHOR("Kun Jiang <kun.jiang@intel.com>");
MODULE_AUTHOR("Intel");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel ipu pci driver");
