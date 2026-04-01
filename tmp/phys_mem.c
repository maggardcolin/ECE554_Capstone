#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/io.h>
#include <linux/mm.h>

/* Use u64 to support 36-bit and 40-bit ZynqMP physical addresses */
static u64 target_addr = 0;
static struct kobject *mem_kobj;

/* Helper to safely read/write physical memory */
static u32 access_phys(u64 phys_addr, u32 val, bool write) {
    void *virt_addr;
    u32 ret = 0;

    /* * Attempt to map with Write-Combine (ideal for DMA buffers).
     * This ensures the CPU doesn't sit on cached data that the 
     * DMA engine might have updated in RAM.
     */
    virt_addr = memremap(phys_addr, 4, MEMREMAP_WC);
    
    /* Fallback to ioremap for non-RAM (FPGA IP) regions */
    if (!virt_addr) {
        virt_addr = ioremap(phys_addr, 4);
    }

    if (!virt_addr) {
        pr_err("phys_mem: Failed to map 0x%llx\n", phys_addr);
        return 0;
    }

    if (write) {
        *(volatile u32 *)virt_addr = val;
        ret = val;
    } else {
        ret = *(volatile u32 *)virt_addr;
    }

    /* Clean up the mapping based on how it was created */
    if (unlikely(((unsigned long)virt_addr >= VMALLOC_START && 
                  (unsigned long)virt_addr < VMALLOC_END))) {
        iounmap(virt_addr);
    } else {
        memunmap(virt_addr);
    }

    return ret;
}

/* Sysfs: Address Input (Supports 64-bit hex/decimal) */
static ssize_t addr_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    /* kstrtoull handles up to 64-bit values */
    if (kstrtoull(buf, 0, &target_addr) < 0)
        return -EINVAL;
    return count;
}

/* Sysfs: Write Value (32-bit Word) */
static ssize_t val_write_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    u32 val;
    if (kstrtouint(buf, 0, &val) < 0)
        return -EINVAL;
    
    access_phys(target_addr, val, true);
    return count;
}

/* Sysfs: Read Value */
static ssize_t val_read_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    u32 val = access_phys(target_addr, 0, false);
    return sprintf(buf, "0x%08x\n", val);
}

/* Define attributes with appropriate permissions */
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
