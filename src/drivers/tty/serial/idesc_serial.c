/**
 * @file
 *
 * @date Dec 18, 2013
 * @author: Anton Bondarev
 */
#include <assert.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <mem/misc/pool.h>

#include <fs/idesc.h>

#include <drivers/tty.h>
#include <drivers/ttys.h>
#include <drivers/char_dev.h>
#include <drivers/serial/uart_device.h>

#include "idesc_serial.h"

#include <framework/mod/options.h>

#define MAX_SERIALS \
	OPTION_GET(NUMBER, serial_quantity)

POOL_DEF(uart_ttys, struct tty_uart, MAX_SERIALS);

#define idesc_to_uart(desc) \
	(((struct  tty_uart*)desc)->uart)

static const struct idesc_ops idesc_serial_ops;

static ssize_t serial_read(struct idesc *idesc, const struct iovec *iov, int cnt) {
	void *buf;
	size_t nbyte;
	struct uart *uart;

	assert(iov);
	buf = iov->iov_base;
	assert(cnt == 1);
	nbyte = iov->iov_len;

	assert(buf);
	assert(idesc);
	assert(idesc->idesc_ops == &idesc_serial_ops);
	assert(idesc->idesc_amode & S_IROTH);

	if (!nbyte) {
		return 0;
	}

	uart = idesc_to_uart(idesc);
	assert(uart);
	assert(uart->tty);

	return tty_read(uart->tty, (char *) buf, nbyte);
}

static ssize_t serial_write(struct idesc *idesc, const struct iovec *iov, int cnt) {
	void *buf;
	size_t nbyte;
	int ch;
	struct uart *uart;
	size_t written, left;

	assert(iov);
	buf = iov->iov_base;
	assert(cnt == 1);
	nbyte = iov->iov_len;

	assert(buf);
	assert(idesc);
	assert(idesc->idesc_ops == &idesc_serial_ops);
	assert(idesc->idesc_amode & S_IWOTH);

	left = nbyte;
	uart = idesc_to_uart(idesc);
	assert(uart);
	assert(uart->tty);

	do {
		written = tty_write(uart->tty, buf, left);

		while (-1 != (ch = tty_out_getc(uart->tty))) {
			uart_putc(uart, ch);
		}

		left -= written;
		buf = (void *)((char *)buf + written);
	} while (left != 0);

	return (ssize_t)nbyte;
}

static void serial_close(struct idesc *idesc) {
	struct uart *uart;
	int res;

	assert(idesc);
	assert(idesc->idesc_ops == &idesc_serial_ops);

	uart = idesc_to_uart(idesc);
	assert(uart);
	res = uart_close(uart);
	assert(res == 0); /* TODO */

	idesc_serial_close(idesc);

}
static int serial_ioctl(struct idesc *idesc, int request, void *data) {
	struct uart *uart;

	assert(idesc);
	assert(idesc->idesc_ops == &idesc_serial_ops);

	uart = idesc_to_uart(idesc);
	assert(uart);

	return tty_ioctl(uart->tty, request, data);
}

static int serial_status(struct idesc *idesc, int mask) {
	struct uart *uart;
	int res = 0;

	assert(idesc);

	if (!mask) {
		return 0;
	}
	uart = idesc_to_uart(idesc);
	assert(uart);

	if (mask & POLLIN) {
		/* how many we can read */
		res += tty_status(uart->tty, POLLIN);
	}

	if (mask & POLLOUT) {
		/* how many we can write */
		res += INT_MAX;
	}

	if (mask & POLLERR) {
		/* is there any exeptions */
		res += 0; //TODO Where is errors counter
	}

	return res;
}

static const struct idesc_ops idesc_serial_ops = {
	.id_readv = serial_read,
	.id_writev = serial_write,
	.ioctl = serial_ioctl,
	.close = serial_close,
	.status = serial_status,
	.fstat = char_dev_idesc_fstat,
};

const struct idesc_ops *idesc_serial_get_ops(void){
	return &idesc_serial_ops;
}
