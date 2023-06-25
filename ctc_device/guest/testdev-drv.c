#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#define PCI_VENDOR_ID_TEST 0x0000
#define PCI_DEVICE_ID_TEST 0x0001
//#define PCI_VENDOR_ID_TEST 0x1b36
//#define PCI_DEVICE_ID_TEST 0x0005
#define DRV_NAME	   "pci_test_driver"

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

	return ioremap_nocache(res->start, resource_size(res));
}

static int ctc_dump_devid(struct pci_dev *pdev)
{
        pr_info("idstr %s\n", data->hw_addr);
        return 0;
}

static int ctc_show(struct seq_file *m, void *v) {
        ctc_dump_devid(data->pci_dev);
        return 0;
}

static int ctc_proc_open(struct inode *inode, struct file *file) {
        return single_open(file, ctc_show, NULL);
}                                                                                   
/*                                                                                  
** This function will be called when we close the Device file
*/                       
static int ctc_proc_release(struct inode *inode, struct file *file)
{                                  
        //pr_info("CTC Device File Closed...!!!\n");                                  
        return 0;
}

/*
** This function will be called when we read the Device file
*/
static ssize_t ctc_proc_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
        ctc_dump_devid(data->pci_dev);
        return 0;
}

ssize_t ctc_proc_write (struct file *file, const char __user *buf, size_t len,
                        loff_t *l)
{
        int length;
        char str[4096] = { 0 };

	if (len > test_length + 1) {
		pr_info("the argument is too long. max size is %d\n", test_length);
		return -1;
	}

	
        length = strncpy_from_user(str, buf, len);
	str[length -1 ] = '\0';
	// str[length] = '\0';
	pr_info("write str:%s\n", str);
	//pr_info("write length expect: %d, len: %d, str:%s\n", length, len, str);
	memcpy(data->hw_addr, str, length);

        return len;
}

static const struct file_operations proc_fops = {
        .owner = THIS_MODULE,
        .open = ctc_proc_open,
        .read = ctc_proc_read,
        .write = ctc_proc_write,
        .release = ctc_proc_release,
};


static int testdev_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int err = -EIO;
	unsigned int bar = 0;
	const char *my_str = "helloxxxx-00";
	
	dev_info(&pdev->dev, " >>> testdev_probe....\n");
	printk(KERN_INFO, " >>> testdev_probe....\n");

	data = kzalloc(sizeof(*data), GFP_KERNEL);
    	if (!data) {
        	dev_err(&pdev->dev, "Failed to allocate device data\n");
        	err =  -ENOMEM;
		goto err_out_mem;
    	}
	data->pci_dev = pdev;	
	
	if ((err = pci_enable_device(pdev))) {
        	dev_err(&pdev->dev,"Cannot enable test PCI device, aborting\n");
		goto err_out_free_dev;
	}	
	
	if ((pci_resource_flags(pdev, bar) & IORESOURCE_MEM) == 0) {
		dev_err(&pdev->dev, "no memory in bar");
		goto err_out_disable_pdev;
	}
	
	if ((err = pci_request_regions(pdev, DRV_NAME))) {
		dev_err(&pdev->dev, "Cannot obtain PCI resources, aborting\n");
		goto err_out_disable_pdev;
	}	

	data->hw_addr = pci_ioremap_bar_test(pdev, bar);
	if (!data->hw_addr){
		dev_err(&pdev->dev, "aaaa Cannot ioremap PCI resources, aborting\n");
		goto err_out_free_res;
	}
	
        data->proc_parent = proc_mkdir("ctc",NULL);
        if( data->proc_parent == NULL )
        {
        	pr_info("Error creating proc entry");
		goto err_out_free_proc;
        }
        
        /*Creating Proc entry under "/proc/etx/" */
        proc_create("devid", 0666, data->proc_parent, &proc_fops);
	ctc_dump_devid(pdev);
        pci_set_drvdata(pdev, data);
	err = 0;
	dev_info(&pdev->dev, " testdev_probe success\n");
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
	dev_err(&pdev->dev, " testdev_probe fail\n");
	return err;
}

static void testdev_remove(struct pci_dev *pdev)
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

static const struct pci_device_id testdev_pci_id_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_TEST, PCI_DEVICE_ID_TEST) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, testdev_pci_id_table);

static struct pci_driver testdev_driver = {
	.name     = DRV_NAME,
	.id_table = testdev_pci_id_table,
	.probe    = testdev_probe,
	.remove   = testdev_remove,
};

module_pci_driver(testdev_driver);

MODULE_DESCRIPTION("just for test pci driver");
MODULE_LICENSE("GPL");
