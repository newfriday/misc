#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#define IVSHMEM_PCI_VENDOR_ID 0x1af4
#define IVSHMEM_PCI_DEVICE_ID 0x1110
#define DRV_NAME	      "ivshmem_demo_driver"
#define IVSHMEM_REGISTER_BAR  0
#define IVSHMEM_MEMORY_BAR    2

static int  test_length = 64;
module_param(test_length, int, 0644);
MODULE_PARM_DESC(test_length, "narrow input string test_length");

struct testdev_data {
    struct pci_dev *pci_dev;
    void  __iomem *hw_addr;
    struct proc_dir_entry *proc_parent;
};

static struct testdev_data *data;
void __iomem *pci_ioremap_bar_test(struct pci_dev *pdev, int bar)
{
    struct resource *res = &pdev->resource[bar];

    /*
     * Make sure the BAR is actually a memory resource, not an IO resource
     */
    if (res->flags & IORESOURCE_UNSET || !(res->flags & IORESOURCE_MEM)) {
        pci_warn(pdev, "iaaa can't ioremap BAR %d: %pR\n", bar, res);
        return NULL;
    }
    printk(KERN_INFO "res->start = 0x%08x\n", res->start);
    printk(KERN_INFO "resource_size(res) =  0x%08x\n", resource_size(res));

    return ioremap_cache(res->start, resource_size(res));
}

static int ivshmem_dump_devid(struct pci_dev *pdev)
{
    pr_info("read str:%s\n", data->hw_addr);
    return 0;
}

static int ivshmem_show(struct seq_file *m, void *v)
{
    ivshmem_dump_devid(data->pci_dev);
    return 0;
}

static int ivshmem_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, ivshmem_show, NULL);
}

/*
** This function will be called when we close the Device file
*/
static int ivshmem_proc_release(struct inode *inode, struct file *file)
{
    return 0;
}

/*
** This function will be called when we read the Device file
*/
static ssize_t ivshmem_proc_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    ivshmem_dump_devid(data->pci_dev);
    return 0;
}

ssize_t ivshmem_proc_write(struct file *file, const char __user *buf, size_t len, loff_t *l)
{
    int length;
    char str[4096] = { 0 };

    if (len > test_length + 1) {
        pr_info("the argument is too long. max size is %d\n", test_length);
        return -1;
    }

    length = strncpy_from_user(str, buf, len);
    str[length - 1 ] = '\0';
    pr_info("write str:%s\n", str);
    memcpy(data->hw_addr, str, length);

    return len;
}

static const struct proc_ops ivshmem_proc_fops =
{
    .proc_open = ivshmem_proc_open,
    .proc_read = ivshmem_proc_read,
    .proc_write = ivshmem_proc_write,
    .proc_release = ivshmem_proc_release,
};

static int ivshmem_dev_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    int err = -EIO;

    dev_info(&pdev->dev, "ivshmem_dev probe ....\n");
    printk(KERN_INFO, "ivshmem_dev probe ....\n");

    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (!data) {
        dev_err(&pdev->dev, "Failed to allocate device data\n");
        err =  -ENOMEM;
        goto err_out_mem;
    }
    data->pci_dev = pdev;

    if ((err = pci_enable_device(pdev))) {
        dev_err(&pdev->dev,"Cannot enable ivshmem PCI device, aborting\n");
        goto err_out_free_dev;
    }

    if ((pci_resource_flags(pdev, IVSHMEM_REGISTER_BAR) & IORESOURCE_MEM) == 0 ||
            (pci_resource_flags(pdev, IVSHMEM_MEMORY_BAR) & IORESOURCE_MEM) == 0) {
        dev_err(&pdev->dev, "no memory in bar");
        goto err_out_disable_pdev;
    }

    if ((err = pci_request_regions(pdev, DRV_NAME))) {
        dev_err(&pdev->dev, "Cannot obtain PCI resources, aborting\n");
        goto err_out_disable_pdev;
    }

    data->hw_addr = pci_ioremap_bar_test(pdev, IVSHMEM_MEMORY_BAR);
    if (!data->hw_addr){
        dev_err(&pdev->dev, "aaaa Cannot ioremap PCI resources, aborting\n");
        goto err_out_free_res;
    }

    data->proc_parent = proc_mkdir("ivshmem_demo",NULL);
    if( data->proc_parent == NULL )
    {
        pr_info("Error creating proc entry");
        goto err_out_free_proc;
    }

    /*Creating Proc entry under /proc/ivshmem_demo/bar2 */
    proc_create("bar2", 0666, data->proc_parent, &ivshmem_proc_fops);
    ivshmem_dump_devid(pdev);
    pci_set_drvdata(pdev, data);
    err = 0;
    dev_info(&pdev->dev, " ivshmem_dev_probe success\n");
    return err;

err_out_free_proc:
    proc_remove(data->proc_parent);
err_out_free_res:
    pci_release_regions(pdev);
err_out_disable_pdev:
    pci_disable_device(pdev);
err_out_free_dev:
    kfree(data);
err_out_mem:
    dev_err(&pdev->dev, " ivshmem_dev_probe fail\n");
    return err;
}
static void ivshmem_dev_remove(struct pci_dev *pdev)
{
    proc_remove(data->proc_parent);
    /* Free IO remapping */
    iounmap(data->hw_addr);
    /* Free up the mem region */
    pci_release_regions(pdev);

    pci_disable_device(pdev);

    kfree(data);
    data = NULL;
    dev_info(&pdev->dev, "Removed\n");
}

static const struct pci_device_id ivshmem_pci_id_table[] =
{
    { PCI_DEVICE(IVSHMEM_PCI_VENDOR_ID, IVSHMEM_PCI_DEVICE_ID) },
    { 0 }
};

MODULE_DEVICE_TABLE(pci, ivshmem_pci_id_table);

static struct pci_driver ivshmem_demo_driver = {
    .name     = DRV_NAME,
    .id_table = ivshmem_pci_id_table,
    .probe    = ivshmem_dev_probe,
    .remove   = ivshmem_dev_remove,
};

module_pci_driver(ivshmem_demo_driver);

MODULE_DESCRIPTION("just for ivshmem demo driver");
MODULE_LICENSE("GPL");
