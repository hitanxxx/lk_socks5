#include "common.h"

sys_data_t *sys_file_read_value(char *path) {
    struct stat fstat;
    int fd = 0;
    int flen = 0;
    sys_data_t *data;
    ssize_t readn = 0;

    if(!path) return NULL;

    if(0 != stat(path, &fstat)) {
        err("file [%s] stat err\n", path);
        return NULL;
    }
    fd = open(path, O_RDONLY);
    if(fd < 0) {
        err("file [%s] open err. [%d]\n", path, errno);
        return NULL;
    }
    flen = (int)fstat.st_size;
    data = sys_alloc(sizeof(sys_data_t) + flen + 1);
    if(!data) {
        err("file [%s] alloc data err. [%d]\n", path, errno);
        close(fd);
        return NULL;
    }
    data->datan = flen;

    while(readn < data->datan) {
        ssize_t ret = read(fd, data->data + readn, data->datan - readn);
        if(ret <= 0) {
            if(ret == 0) {
                err("file [%s] read eof\n", path);
            } else {
                err("file [%s] read err. [%d]\n", path, errno);
            }
            close(fd);
            return NULL;
        }
        readn += ret;
    }
    close(fd);
    return data;
}

int sys_file_write_data(char *fname, char *data, int datan) {
    int fd = -1;
    ssize_t writen = 0;

    if (!fname || !data) {
        err("param is null.\n");
        return -1;
    }

    fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0777);
    if (fd <= 0) {
        err("file %s open failed. [%d]\n", fname, errno);
        return -1;
    }
    while (writen < datan) {
        ssize_t ret = write(fd, data + writen, datan - writen);
        if (ret <= 0) {
            err("file %s write failed. [%d]\n", fname, errno);
            close(fd);
            return -1;
        }
        writen += ret;
    }
    close(fd);
    return 0;
}

int sys_file_exist(const char *fname) {
    FILE *file = fopen(fname, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

int sys_directory_exist(const char *fpath) {
    struct stat statbuf;
    if (stat(fpath, &statbuf) != 0) {
        return 0;
    }
   
    return S_ISDIR(statbuf.st_mode);
}


