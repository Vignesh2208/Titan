#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h> /* min */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>

static const char *filename = "lkmc_mmap";

int * my_buf;

/* After unmap. */
static void vm_close(struct vm_area_struct *vma)
{
    pr_info("vm_close\n");
}

/* First page access. */
static int vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
    struct page *page;

    pr_info("vm_fault\n");
    /*vmf->page = vmalloc_to_page(my_buf);
	pr_info("Got page");
    get_page(vmf->page);
	pr_info("finished vm_fault");*/
	int *info = (int *)vma->vm_private_data;
    if (info) {
        page = virt_to_page(info);
		pr_info("Got page");
        get_page(page);
        vmf->page = page;
    }
	pr_info("finished vm_fault");
    return 0;
}

/* Aftr mmap. TODO vs mmap, when can this happen at a different time than mmap? */
static void vm_open(struct vm_area_struct *vma)
{
    pr_info("vm_open\n");
}

static struct vm_operations_struct vm_ops =
{
    .close = vm_close,
    .fault = vm_fault,
    .open = vm_open,
};

static int proc_mmap(struct file *filp, struct vm_area_struct *vma)
{
    vma->vm_ops = &vm_ops;
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_private_data = filp->private_data;
    return 0;
}



static int proc_open(struct inode *inode, struct file *filp)
{
    struct mmap_info *info;

    pr_info("proc_open\n");
	if (!my_buf);
	my_buf = (int *)get_zeroed_page(GFP_KERNEL);
    filp->private_data = my_buf;
    return 0;
}

static const struct file_operations fops = {
    .mmap = proc_mmap,
	.open = proc_open,
	.owner = THIS_MODULE,
};

int __init myinit(void)
{
	int i;
	//my_buf = (int *)kmalloc(sizeof(int)*10, GFP_KERNEL);
	my_buf = NULL;
	printk (KERN_INFO "Creating Proc file ...");
	/*if (my_buf != NULL) {
		pr_info("Kmalloc user Succeeded !");
		for (i = 0; i < 10; i++)
			my_buf[i] = 0;
	} else {
		pr_info("Kmalloc user FAILED !");
	}*/
    proc_create(filename, 0, NULL, &fops);
	
    return 0;
}

void __exit  myexit(void)
{	//kfree(my_buf);
	if (my_buf)
		free_page((unsigned long)my_buf);
	printk (KERN_INFO "Deleting Proc file ...");
    remove_proc_entry(filename, NULL);
	
}

module_init(myinit)
module_exit(myexit)
MODULE_LICENSE("GPL");