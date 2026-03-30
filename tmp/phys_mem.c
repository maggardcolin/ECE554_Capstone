#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/io.h>

static u32 target_addr = 0;
static struct kobject *mem_kobj;

/* Helper to safely read/write physical memory */
static u32 access_phys(u32 phys_addr, u32 val, bool write) {
    void __iomem *virt_addr;
    u32 ret = 0;

    /* Map the physical page (handles 4KB chunks) */
    virt_addr = ioremap(phys_addr, 4);
    if (!virt_addr) {
        pr_err("phys_mem: Failed to map 0x%08x\n", phys_addr);
        return 0;
    }

    if (write) {
        iowrite32(val, virt_addr);
        ret = val;
    } else {
        ret = ioread32(virt_addr);
    }

    iounmap(virt_addr);
    return ret;
}

/* Sysfs: Address Input */
static ssize_t addr_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    kstrtouint(buf, 0, &target_addr);
    return count;
}

/* Sysfs: Write Value */
static ssize_t val_write_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    u32 val;
    kstrtouint(buf, 0, &val);
    access_phys(target_addr, val, true);
    return count;
}

/* Sysfs: Read Value */
static ssize_t val_read_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    u32 val = access_phys(target_addr, 0, false);
    return sprintf(buf, "0x%08x\n", val);
}

static struct kobj_attribute addr_attr = __ATTR(addr_input, 0660, NULL, addr_store);
static struct kobj_attribute write_attr = __ATTR(val_input, 0220, NULL, val_write_store);
static struct kobj_attribute read_attr = __ATTR(val_output, 0440, val_read_show, NULL);

static struct attribute *attrs[] = {
    &addr_attr.attr,
    &write_attr.attr,
    &read_attr.attr,
    NULL,
};

static struct attribute_group attr_group = { .attrs = attrs };

static int __init phys_mem_init(void) {
    mem_kobj = kobject_create_and_add("phys_mem_tool", kernel_kobj);
    if (!mem_kobj) return -ENOMEM;
    return sysfs_create_group(mem_kobj, &attr_group);
}

static void __exit phys_mem_exit(void) {
    kobject_put(mem_kobj);
}

module_init(phys_mem_init);
module_exit(phys_mem_exit);
MODULE_LICENSE("GPL");
