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
#include <errno.h>
#include <assert.h>

#define BUF_SZ   4096
#define RING_SZ  8

int fixed_buffers(struct io_uring *ring) {

	struct iovec iov;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe[RING_SZ];
	int i;
	unsigned int nr = 0;
	unsigned int batch_sz = 8;
	off_t r = 0;
	char buf[BUF_SZ];


	iov.iov_base = buf;
	iov.iov_len = BUF_SZ;
	
	for (i = 0; i<BUF_SZ; i++) {
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

	for (;;) {

		for (i = 0; i<batch_sz; i++) {

			sqe = io_uring_get_sqe(ring);

			if (!sqe) {
				fprintf(stderr, "Could not get SQE.\n");
				return 1;
			}
	
			io_uring_prep_write_fixed(sqe, 1, iov.iov_base, BUF_SZ, r, 0);

			r += BUF_SZ;
			
			nr++; 
		}

		assert(nr <= RING_SZ);

		ret = io_uring_submit_and_wait(ring, 1);

		if (ret < 0) {
			fprintf(stderr, "Error submitting buffers: %s\n", strerror(-ret));
			return 1;
		}

		batch_sz = io_uring_peek_batch_cqe(ring, cqe, RING_SZ);

		nr -= batch_sz;
		
		for (i=0; i < batch_sz; i++) {

			if (cqe[i]->res == -EPIPE)
				return 0;

			if (cqe[i]->res < 0) {
				fprintf(stderr, "Error in async operation: %s\n", strerror(-cqe[i]->res));
				return 1;
			}
		}

		io_uring_cq_advance(ring, batch_sz);
	}

	return 0;

}

int main() {

	struct io_uring ring;

	int ret = io_uring_queue_init(RING_SZ, &ring, 0);

	if (ret) {
		fprintf(stderr, "Unable to setup io_uring: %s\n", strerror(-ret));
		return 1;
	}

	ret = fixed_buffers(&ring);

	io_uring_queue_exit(&ring);

	return ret;

}