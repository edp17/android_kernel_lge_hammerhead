/*
 * Minimal memfd_create compatibility syscall for old Android kernels.
 *
 * This intentionally does not implement file sealing. It exists for
 * userspace which only needs an anonymous, shmem-backed, mmap-able file,
 * notably Waydroid's LineageOS 18.1 hwcomposer.
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/limits.h>
#include <linux/mm.h>
#include <linux/shmem_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#define MFD_NAME_PREFIX                 "memfd:"
#define MFD_NAME_PREFIX_LEN             6
#define MFD_NAME_MAX_LEN                (NAME_MAX - MFD_NAME_PREFIX_LEN)

SYSCALL_DEFINE2(memfd_create, const char __user *, uname,
                unsigned int, flags)
{
        struct file *file;
        char *name;
        long len;
        int fd;
        int error;

        /*
         * This compatibility syscall is intentionally provided only for
         * Waydroid hwcomposer's flags == 0 call. Host consumers such as
         * systemd and LXC require fuller memfd semantics; report ENOSYS
         * for every non-zero flag so they use their established fallback.
         */
        if (flags != 0)
                return -ENOSYS;

        /*
         * strnlen_user() includes the terminating NUL in its return value.
         * Keep the full "memfd:<name>" dentry name within NAME_MAX.
         */
        len = strnlen_user(uname, MFD_NAME_MAX_LEN + 1);
        if (len <= 0)
                return -EFAULT;
        if (len > MFD_NAME_MAX_LEN)
                return -EINVAL;

        name = kmalloc(MFD_NAME_PREFIX_LEN + len, GFP_KERNEL);
        if (!name)
                return -ENOMEM;

        memcpy(name, MFD_NAME_PREFIX, MFD_NAME_PREFIX_LEN);
        if (copy_from_user(name + MFD_NAME_PREFIX_LEN, uname, len)) {
                error = -EFAULT;
                goto err_name;
        }

        /*
         * Detect a userspace race which changes or removes the terminating NUL
         * after strnlen_user() but before copy_from_user().
         */
        if (name[MFD_NAME_PREFIX_LEN + len - 1] != '\0') {
                error = -EFAULT;
                goto err_name;
        }

        fd = get_unused_fd();
        if (fd < 0) {
                error = fd;
                goto err_name;
        }

        /*
         * A zero-length shmem file has the behaviour required by memfd_create:
         * userspace can size it with ftruncate() and map it MAP_SHARED.
         */
        file = shmem_file_setup(name, 0, VM_NORESERVE);
        if (IS_ERR(file)) {
                error = PTR_ERR(file);
                goto err_fd;
        }

        /*
         * shmem_file_setup() in this 3.4 kernel creates the file with only
         * FMODE_READ | FMODE_WRITE. Add the normal memfd mode bits explicitly
         * so lseek(), pread() and pwrite() have regular-file semantics.
         */
        file->f_mode |= FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE;
        file->f_flags |= O_LARGEFILE;

        fd_install(fd, file);
        kfree(name);
        return fd;

err_fd:
        put_unused_fd(fd);
err_name:
        kfree(name);
        return error;
}
