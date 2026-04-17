#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "optee_private.h"
#include "mitee_proc.h"

static ssize_t mitee_concurrency_proc_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *pos)
{
    struct optee *optee = pde_data(file_inode(file));
    unsigned int val;
    int err;
    char buffer[16];

    if (!buf || count == 0 || count >= sizeof(buffer))
        return -EINVAL;

    if (copy_from_user(buffer, buf, count))
        return -EFAULT;
    
    buffer[count] = '\0';

    err = kstrtouint(buffer, 10, &val);
    if (err)
        return err;

    if (val == 1) {
        pr_info("mitee_concurrency_proc_write: enable\n");
        optee->concurrency_flags |= 0x200000000ULL;
    } else if (val == 0) {
        pr_info("mitee_concurrency_proc_write: disable\n");
        optee->concurrency_flags &= ~0x200000000ULL;
        optee->concurrency_flags |= 0x100000000ULL;
    } else {
        pr_info("mitee_concurrency_proc_write: unsupported %u\n", val);
    }
    
    return count;
}

static int mitee_concurrency_proc_open(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct proc_ops mitee_concurrency_fops = {
    .proc_open = mitee_concurrency_proc_open,
    .proc_write = mitee_concurrency_proc_write,
    .proc_lseek = default_llseek,
};

int mitee_proc_init(struct optee *optee)
{
    struct proc_dir_entry *entry;
    
    entry = proc_create_data("mitee_concurrency", 0644, NULL, &mitee_concurrency_fops, optee);
    if (!entry) {
        pr_err("failed to create mitee_concurrency proc entry\n");
        return -ENOMEM;
    }
    optee->concurrency_proc = entry;
    return 0;
}

void mitee_proc_deinit(struct optee *optee)
{
    if (optee->concurrency_proc) {
        remove_proc_entry("mitee_concurrency", NULL);
    }
}
