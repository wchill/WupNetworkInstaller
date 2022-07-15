#include <sys/stat.h>
#include "extusb_devoptab.h"

static inline FSMode
__extusb_fs_convert_mode(mode_t mode) {
    // Convert normal Unix octal permission bits into CafeOS hexadecimal permission bits
    return (FSMode) (((mode & S_IRWXU) << 2) | ((mode & S_IRWXG) << 1) | (mode & S_IRWXO));
}

int
__extusb_fs_chmod(struct _reent *r,
                  const char *path,
                  mode_t mode) {
    if (!path) {
        r->_errno = EINVAL;
        return -1;
    }

    char *fixedPath = __extusb_fs_fixpath(r, path);
    if (!fixedPath) {
        return -1;
    }

    FRESULT fr;
    if (((mode & S_IWUSR) || (mode & S_IWGRP) || (mode & S_IWOTH)) == 0) {
        fr = f_chmod(fixedPath, 0, AM_RDO);
    } else {
        fr = f_chmod(fixedPath, AM_RDO, AM_RDO);
    }

    free(fixedPath);
    if (fr != FR_OK) {
        r->_errno = __extusb_fs_translate_error(fr);
        return -1;
    }

    return 0;
}
