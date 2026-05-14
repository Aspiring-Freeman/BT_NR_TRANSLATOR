/**
 * @file    syscalls_stub.c
 * @brief   newlib-nano 裸机系统调用桩实现
 *
 * 目的:
 *   newlib-nano (libg_nano.a) 中的 closer.o / lseekr.o / readr.o / writer.o
 *   嵌入了 .gnu.warning.<sym> 段, 一旦它们被链接进来就会产生:
 *     "_close is not implemented and will always fail" 等链接器警告.
 *   只要任何代码 (常见: printf 系列、scanf、fopen 等) 引用了
 *   _close_r / _lseek_r / _read_r / _write_r, 上述目标文件就会被拉入.
 *
 *   本文件提供这几个 reentrant 版本的强符号定义, 直接覆盖 libc 中的版本,
 *   阻止 closer.o 等被链接, 从而消除噪声警告. 行为与 nosys 一致 (返回 -1
 *   并设置 errno=ENOSYS), 不增加 flash 占用.
 */
#include <errno.h>
#include <reent.h>
#include <sys/types.h>

int _close_r(struct _reent *r, int fd) {
  (void)fd;
  r->_errno = ENOSYS;
  return -1;
}

_off_t _lseek_r(struct _reent *r, int fd, _off_t off, int whence) {
  (void)fd;
  (void)off;
  (void)whence;
  r->_errno = ENOSYS;
  return (_off_t)-1;
}

_ssize_t _read_r(struct _reent *r, int fd, void *buf, size_t cnt) {
  (void)fd;
  (void)buf;
  (void)cnt;
  r->_errno = ENOSYS;
  return -1;
}

_ssize_t _write_r(struct _reent *r, int fd, const void *buf, size_t cnt) {
  (void)fd;
  (void)buf;
  (void)cnt;
  r->_errno = ENOSYS;
  return -1;
}
