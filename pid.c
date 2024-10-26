#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thomas Blake Joye");
MODULE_DESCRIPTION(" Linux kernel module to interact with /proc/pid");

static struct proc_dir_entry *our_proc_file;
static long pid = -1;

// Function to read data to /proc/pid using seq_file interface
static int proc_read(struct seq_file *m, void *v) {
    if (pid < 0) {
        seq_printf(m, "Invalid PID\n");
    } else {
        struct pid *pid_struct = find_vpid(pid);
        if (!pid_struct) {
            seq_printf(m, "PID %ld not found\n", pid);
        } else {
            struct task_struct *task = pid_task(pid_struct, PIDTYPE_PID);
            if (task) {
                seq_printf(m, "command = [%s] pid = [%d] state = [%u]\n",
                           task->comm, task->pid, task->__state);
            } else {
                seq_printf(m, "PID %ld not found\n", pid);
            }
        }
    }
    return 0;
}

// Open function for /proc/pid
static int my_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, proc_read, NULL);
}

// Function to write data from user to the kernel space
static ssize_t proc_write(struct file *file, const char __user *usr_buf, size_t count, loff_t *pos) {
    char *k_mem;
    long new_pid;

    k_mem = kmalloc(count + 1, GFP_KERNEL);
    if (!k_mem)
        return -ENOMEM;

    if (copy_from_user(k_mem, usr_buf, count)) {
        kfree(k_mem);
        return -EFAULT;
    }
    k_mem[count] = '\0';

    if (kstrtol(k_mem, 10, &new_pid)) {
        kfree(k_mem);
        return -EINVAL;
    }

    if (new_pid < 0) {
        kfree(k_mem);
        return -EINVAL;
    }

    pid = new_pid;
    printk(KERN_INFO "PID set to %ld\n", pid);
    kfree(k_mem);
    return count;
}

// File operations structure for /proc/pid
static const struct proc_ops proc_fops = {
    .proc_open = my_proc_open,
    .proc_read = seq_read, 
    .proc_write = proc_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

// Initialize the Kernel Module
static int __init pid_module_init(void) {
    our_proc_file = proc_create("pid", 0666, NULL, &proc_fops);
    if (!our_proc_file) {
        return -ENOMEM;
    }
    printk(KERN_INFO "/proc/pid created\n");
    return 0;
}

// Cleanup the Kernel Module
static void __exit pid_module_cleanup(void) {
    proc_remove(our_proc_file);
    printk(KERN_INFO "/proc/pid removed\n");
}

module_init(pid_module_init);
module_exit(pid_module_cleanup);

