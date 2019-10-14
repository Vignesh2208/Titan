//#include <asm/uaccess.h> /* copy_from_user */
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h> /* min */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>

static const char *filename = "lkmc_mmap";

char * my_buf;
char * aligned_buf;

enum { BUFFER_SIZE = 4 };

struct mmap_info {
    char *data;
};

/*
static void vm_close(struct vm_area_struct *vma)
{
    pr_info("vm_close\n");
}

static int vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
    struct page *page;
    struct mmap_info *info;

    pr_info("vm_fault\n");
    info = (struct mmap_info *)vma->vm_private_data;
    if (info->data) {
        page = virt_to_page(info->data);
        get_page(page);
        vmf->page = page;
    }
    return 0;
}

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
*/

static int mmap(struct file *filp, struct vm_area_struct *vma)
{
    pr_info("mmap\n");
    /*vma->vm_ops = &vm_ops;
    vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
    vma->vm_private_data = filp->private_data;
    vm_open(vma);*/

    unsigned long pfn = virt_to_phys((void *)aligned_buf)>>PAGE_SHIFT;
    unsigned long len = vma->vm_end - vma->vm_start;
    int ret ;

    ret = remap_pfn_range(vma, vma->vm_start, pfn, len, vma->vm_page_prot);
    if (ret < 0) {
        pr_err("could not map the address area\n");
        return -EIO;
    }

    return 0;
}
/*
static int open(struct inode *inode, struct file *filp)
{
    struct mmap_info *info;

    pr_info("open\n");
    info = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);
    pr_info("virt_to_phys = 0x%llx\n", (unsigned long long)virt_to_phys((void *)info));
    info->data = (char *)get_zeroed_page(GFP_KERNEL);
    memcpy(info->data, "asdf", BUFFER_SIZE);
    filp->private_data = info;
    return 0;
}

static ssize_t read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    struct mmap_info *info;
    int ret;

    pr_info("read\n");
    info = filp->private_data;
    ret = min(len, (size_t)BUFFER_SIZE);
    if (copy_to_user(buf, info->data, ret)) {
        ret = -EFAULT;
    }
    return ret;
}

static ssize_t write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
    struct mmap_info *info;

    pr_info("write\n");
    info = filp->private_data;
    if (copy_from_user(info->data, buf, min(len, (size_t)BUFFER_SIZE))) {
        return -EFAULT;
    } else {
        return len;
    }
}

static int release(struct inode *inode, struct file *filp)
{
    struct mmap_info *info;

    pr_info("release\n");
    info = filp->private_data;
    free_page((unsigned long)info->data);
    kfree(info);
    filp->private_data = NULL;
    return 0;
}*/

static const struct file_operations fops = {
    .mmap = mmap,
    //.open = open,
    //.release = release,
    //.read = read,
    //.write = write,
};


static char * alloc_mmap_pages(int npages)
{
    int i;
    char *mem = kmalloc(PAGE_SIZE * npages, GFP_KERNEL);

    if (!mem)
        return mem;

    memset(mem, 0, PAGE_SIZE * npages);
    memcpy(mem, "asdf", BUFFER_SIZE);
    for(i = 0; i < npages * PAGE_SIZE; i += PAGE_SIZE) {
        SetPageReserved(virt_to_page(((unsigned long)mem) + i));
    }

    return mem;
}

static void free_mmap_pages(void *mem, int npages)
{
    int i;

    for(i = 0; i < npages * PAGE_SIZE; i += PAGE_SIZE) {
        ClearPageReserved(virt_to_page(((unsigned long)mem) + i));
    }

    kfree(mem);
}

int __init myinit(void)
{
    proc_create(filename, 0, NULL, &fops);
    my_buf = alloc_mmap_pages(1);
    aligned_buf = PAGE_ALIGN((unsigned long)my_buf);
    return 0;
}

void __exit myexit(void)
{
    free_mmap_pages(my_buf, 1);
    remove_proc_entry(filename, NULL);
}

module_init(myinit)
module_exit(myexit)
MODULE_LICENSE("GPL");

