#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/io_uring.h>
#include <stdlib.h>
#include <liburing.h>
#include <limits.h>

int fixed_buffers(struct io_uring *ring) {

	struct iovec iov;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	int i;
	int size = 4096;
	char buf[size];

	iov.iov_base = buf;
	iov.iov_len = size;

	for (i = 0; i<=size; i++) {
		if (i%2 == 0)
			buf[i] = 'y';
		else
			buf[i] = '\n';
	}

	int ret = io_uring_register_buffers(ring, &iov, 1);
	
	if (ret) {
		fprintf(stderr, "Error registering buffers: %s\n", strerror(-ret));
		return 1;
	}
	sqe = io_uring_get_sqe(ring);

	if (!sqe) {
		fprintf(stderr, "Could not get SQE.\n");
		return 1;
	}
	
	io_uring_prep_write_fixed(sqe, 1, iov.iov_base, size, 0, 0);

	for (;;) {
		io_uring_submit(ring);
	}

	ret = io_uring_wait_cqe(ring, &cqe);
	
	if (ret < 0) {
		fprintf(stderr, "Error waiting for completion: %s\n",
		strerror(-ret));
		return 1;
	}

	if (cqe->res < 0) {
		fprintf(stderr, "Error in async operation: %s\n", strerror(-cqe->res));
	}

	io_uring_cqe_seen(ring, cqe);

	return 0;

}

int main() {

	struct io_uring ring;

	int ret = io_uring_queue_init(8, &ring, 0);

	if (ret) {
		fprintf(stderr, "Unable to setup io_uring: %s\n", strerror(-ret));
		return 1;
	}

	fixed_buffers(&ring);

	io_uring_queue_exit(&ring);

	return 0;

}
