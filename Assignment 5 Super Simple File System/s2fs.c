#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/time.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pratik Nagelia");

#define S2FS_MAGIC 0x19980122

static struct inode *s2fs_make_inode(struct super_block *sb, int mode) {
    struct inode *ret = new_inode(sb);
    kgid_t gid = KGIDT_INIT(0);
    kuid_t uid = KUIDT_INIT(0);

    if (ret) {
        ret->i_ino = get_next_ino();
        ret->i_sb = sb;
        ret->i_mode = mode;
        ret->i_uid = uid; 
        ret->i_gid = gid;
        ret->i_blocks = 0;
        ret->i_atime = ret->i_mtime = ret->i_ctime = current_time(ret);
    }
    return ret;
}

static struct dentry *s2fs_create_dir (struct super_block *sb, struct dentry *parent, const char *dir_name) {
    struct dentry *dentry;
    struct inode *inode;
    struct qstr qname;

    qname.name = dir_name;
    qname.len = strlen(dir_name);
    qname.hash = full_name_hash(parent, qname.name, qname.len);
    dentry = d_alloc(parent, &qname);

    if (!dentry)
        goto out;

    inode = s2fs_make_inode(sb, S_IFDIR | 0777);
    if (!inode)
        goto out_dput;
    inode->i_op = &simple_dir_inode_operations;
    inode->i_fop = &simple_dir_operations;

    d_add(dentry, inode);
    return dentry;

      out_dput:
    dput(dentry);
      out:
    return 0;
}

static int s2fs_open(struct inode *inode, struct file *filp) {
    return 0;
}

static ssize_t s2fs_read_file(struct file *filp, char *buf,
                              size_t count, loff_t *offset) {
    
    int len = strlen("Hello World!\n");

    if (*offset > len)
        return 0;
    if (count > len - *offset)
        count = len - *offset;

    if (copy_to_user(buf, "Hello World!\n" + *offset, count))
        return -EFAULT;
    *offset += len;
    return count;
}

static ssize_t s2fs_write_file(struct file *filp, const char *buf,
                               size_t count, loff_t *offset) {
    return 0;
}

static struct file_operations s2fs_fops = {
        .open = s2fs_open,
        .read = s2fs_read_file,
        .write = s2fs_write_file,
};

static struct dentry *s2fs_create_file(struct super_block *sb,
                                       struct dentry *dir,
                                       const char *file_name) {
    struct dentry *dentry;
    struct inode *inode;
    struct qstr qname;

    qname.name = file_name;
    qname.len = strlen(file_name);
    qname.hash = full_name_hash(dir, qname.name, qname.len);

    dentry = d_alloc(dir, &qname);
    if (!dentry)
        goto out;
    inode = s2fs_make_inode(sb, S_IFREG | 0644);
    if (!inode)
        goto out_dput;
    inode->i_fop = &s2fs_fops;

    d_add(dentry, inode);
    return dentry;

    out_dput:
    dput(dentry);
    out:
    return 0;
}

static struct super_operations s2fs_s_ops = {
        .statfs = simple_statfs,
        .drop_inode = generic_delete_inode,
};

static int s2fs_fill_super(struct super_block *sb, void *data, int silent) {
    struct inode *root;
    struct dentry *root_dentry;
    struct dentry *subdirectory;

    sb->s_magic = S2FS_MAGIC;
    sb->s_op = &s2fs_s_ops;

    root = s2fs_make_inode(sb, S_IFDIR | 0755);
    if (!root)
        goto out;
    root->i_op = &simple_dir_inode_operations;
    root->i_fop = &simple_dir_operations;

    root_dentry = d_make_root(root);
    if (!root_dentry)
        goto out_iput;
    sb->s_root = root_dentry;

    subdirectory = s2fs_create_dir(sb, root_dentry, "foo");
    if (subdirectory)
        s2fs_create_file(sb, subdirectory, "bar");
    return 0;

    out_iput:
    iput(root);
    out:
    return -ENOMEM;
}

static struct dentry *s2fs_get_super(struct file_system_type *fst,
                                     int flags,
                                     const char *devname, void *data) {
    return mount_nodev(fst, flags, data, s2fs_fill_super);
}

static struct file_system_type s2fs_type = {
        .owner = THIS_MODULE,
        .name = "s2fs",
        .mount = s2fs_get_super,
        .kill_sb = kill_litter_super,
};

static int __init s2fs_init(void) {
    return register_filesystem(&s2fs_type);
}

static void __exit s2fs_exit(void) {
    unregister_filesystem(&s2fs_type);
}

module_init(s2fs_init);
module_exit(s2fs_exit);