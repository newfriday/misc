/*
 * Test PCI device
 *
 * Copyright (c) 2023
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "hw/pci/pci.h"
#include "hw/hw.h"
#include "hw/pci/msi.h"
#include "qemu/timer.h"
#include "qemu/main-loop.h" /* iothread mutex */
#include "qemu/module.h"
#include "qapi/visitor.h"
#include "migration/vmstate.h"
#include "hw/qdev-properties.h"
#include "qemu/error-report.h"

#define PCI_VENDOR_ID_CTC               0x0000
#define PCI_DEVICE_ID_CTC               0x0001

#define TYPE_PCI_CTC_DEVICE "test_demo"
#define CTCDEV(obj)        OBJECT_CHECK(CTCDevState, obj, TYPE_PCI_CTC_DEVICE)

#define CTCDEV_DEBUG(fmt, ...) \
    do { error_report("[test_demo|0x0000,0x00001]"fmt, ## __VA_ARGS__); } while (0)

#define CTC_MAX_ID  4096 
#define CTC_DEFAULT_ID  "hello-00"
#define CTC_BAR_SIZE    CTC_MAX_ID

typedef struct {
    PCIDevice pdev;
    MemoryRegion mmio;
    char *idstr;
    uint8_t devid[CTC_MAX_ID];
} CTCDevState;


static int ctc_post_load(void *opaque, int version_id)
{
    CTCDevState *cdev = opaque;

    CTCDEV_DEBUG("migration: device id string: %s\n", (char *)cdev->devid);

    return 0;
}

static const VMStateDescription vmstate_ctc = {
    .name = "vdagent",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = ctc_post_load,
    .fields = (VMStateField[]) {
  	VMSTATE_PCI_DEVICE(pdev, CTCDevState),
        VMSTATE_BUFFER(devid, CTCDevState),
        VMSTATE_END_OF_LIST()
    },
};

static uint64_t ctc_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    CTCDevState *cdev = opaque;
    uint64_t val = ~0ULL;

    if (addr + size > CTC_BAR_SIZE) {
        CTCDEV_DEBUG("mmio_read: addr 0x%lx, size %u, val %lu", addr, size, val);
        return val;
    }

    switch (size) {
    case 1:
        val = cdev->devid[addr];
	break;
    case 2:
        val =  *((uint16_t *)(cdev->devid + addr));
	break;
    case 4:
        val = *((uint32_t *)(cdev->devid + addr));
	break;
    case 8:
        val = *((uint64_t *)(cdev->devid + addr));
	break;
    default:
	break;
    }
    CTCDEV_DEBUG("mmio_read: addr 0x%lx, size %u, val %lu", addr, size, val);
    return val;
}

static void ctc_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                unsigned size)
{
    CTCDevState *cdev = opaque;

    CTCDEV_DEBUG("mmio_write addr 0x%lx, size %u, val %lu", addr, size, val);
    if (addr + size > CTC_BAR_SIZE) {
        return;
    }

    switch (size) {
    case 1:
        cdev->devid[addr] = val;
        break;
    case 2:
        *((uint16_t *)(cdev->devid + addr)) = (uint16_t)val;
        break;
    case 4:
        *((uint32_t *)(cdev->devid + addr)) =  (uint32_t)val;
        break;
    case 8:
        *((uint64_t *)(cdev->devid + addr)) = val;
        break;
    default:
        return;
    }
}

static const MemoryRegionOps ctc_mmio_ops = {
    .read = ctc_mmio_read,
    .write = ctc_mmio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },

};

static void pci_ctc_realize(PCIDevice *pdev, Error **errp)
{
    CTCDevState *cdev = CTCDEV(pdev);
    uint8_t *pci_conf = pdev->config;

    pci_config_set_interrupt_pin(pci_conf, 1);

    if (cdev->idstr) {
        memcpy(cdev->devid, cdev->idstr, strlen(cdev->idstr) + 1);
    }

    if (msi_init(pdev, 0, 1, true, false, errp)) {
        return;
    }

    memory_region_init_io(&cdev->mmio, OBJECT(cdev), &ctc_mmio_ops, cdev,
                          "ctc-mmio", CTC_BAR_SIZE);
    pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_MEM_PREFETCH | PCI_BASE_ADDRESS_SPACE_MEMORY, &cdev->mmio);
}

static void pci_ctc_uninit(PCIDevice *pdev)
{
    msi_uninit(pdev);
}

static void ctc_instance_init(Object *obj)
{
    CTCDevState *cdev = CTCDEV(obj);

    memcpy(cdev->devid, CTC_DEFAULT_ID, strlen(CTC_DEFAULT_ID) + 1);
}

static Property ctc_dev_properties[] = {
    DEFINE_PROP_STRING("idstr", CTCDevState, idstr),
    DEFINE_PROP_END_OF_LIST(),
};

static void ctc_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(class);

    device_class_set_props(dc, ctc_dev_properties);
    dc->vmsd = &vmstate_ctc;
    k->realize = pci_ctc_realize;
    k->exit = pci_ctc_uninit;
    k->vendor_id = PCI_VENDOR_ID_CTC;
    k->device_id = PCI_DEVICE_ID_CTC;
    k->revision = 0x10;
    k->class_id = PCI_CLASS_OTHERS;

    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static void pci_ctc_register_types(void)
{
    static InterfaceInfo interfaces[] = {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    };
    static const TypeInfo ctc_info = {
        .name          = TYPE_PCI_CTC_DEVICE,
        .parent        = TYPE_PCI_DEVICE,
        .instance_size = sizeof(CTCDevState),
        .instance_init = ctc_instance_init,
        .class_init    = ctc_class_init,
        .interfaces = interfaces,
    };

    type_register_static(&ctc_info);
}
type_init(pci_ctc_register_types)
