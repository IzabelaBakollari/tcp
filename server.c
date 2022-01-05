#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/io_uring.h>
#include <stdlib.h>
#include <liburing.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define PORT	8080
#define BUF_SZ	4096
#define RING_SZ	8

struct io_data {
	int read;
};

static int get_sqe(struct io_uring *ring, struct io_uring_sqe **psqe){

	*psqe = io_uring_get_sqe(ring);

	if (!*psqe) {
		fprintf(stderr, "Could not get SQE.\n");
		exit(EXIT_FAILURE);
	}

	return 0;
}

static int submit(struct io_uring *ring, struct io_uring_cqe **pcqe){

	int submit = io_uring_submit_and_wait(ring, 1);

	if (submit < 0) {
		fprintf(stderr, "Error on submition: %s\n", strerror(-submit));
		exit(EXIT_FAILURE);
 	}
	return 0;
}

static int process(struct io_uring *ring, struct io_uring_cqe **pcqe){

	int ret = io_uring_peek_cqe(ring, pcqe);

	if (ret == -EAGAIN)
		return 11;

	if (ret < 0) {
		fprintf(stderr, "Error waiting for completion: %s\n",strerror(-ret));
		exit(EXIT_FAILURE);
	}

	int res = (*pcqe)->res;

	if (res < 0) {
		fprintf(stderr, "Error in async operation: %s\n", strerror(-res));
		exit(EXIT_FAILURE);
	}

	io_uring_cqe_seen(ring, *(pcqe));

	return res;
}

int main(int argc, char const *argv[])
{
	int server_fd, new_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buf[2][BUF_SZ];
	struct io_data data[2];
	struct io_uring ring;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	struct io_data *data_cqe;
	
	// Creating socket file descriptor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
	
	// Forcefully attaching socket to the port 8080
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
			&opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);
	
	// Forcefully attaching socket to the port 8080
	if (bind(server_fd, (struct sockaddr *)&address,
		sizeof(address))<0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	printf("Listener on port %d \n", PORT);

	if (listen(server_fd, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}
	if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
					(socklen_t*)&addrlen))<0)
	{
		perror("accept");
		exit(EXIT_FAILURE);
	}

	int ret = io_uring_queue_init(RING_SZ, &ring, 0);

	if (ret) {
		fprintf(stderr, "Unable to setup io_uring: %s\n", strerror(-ret));
		exit(EXIT_FAILURE);
	}

	data[0].read = 3;
	data[1].read = 4;

	for (;;) {

		if (data[0].read == 3){
			get_sqe(&ring, &sqe);
			io_uring_prep_read(sqe, new_socket, buf[0], BUF_SZ, 0);
			data[0].read = 1;
			io_uring_sqe_set_data(sqe, &data[0]);

		}

		if (data[0].read == 4){
			get_sqe(&ring, &sqe);
			io_uring_prep_read(sqe, new_socket, buf[1], BUF_SZ, 0);
			data[1].read = 2;
			io_uring_sqe_set_data(sqe, &data[1]);
		}

		submit(&ring, &cqe);

		for (;;) {

			int res = process(&ring, &cqe);

			if (res == 11)
				break;

			data_cqe = io_uring_cqe_get_data(cqe);

			if (data_cqe->read == 1){
				get_sqe(&ring, &sqe);
				io_uring_prep_write(sqe, new_socket, buf[0], res, 0);
				data[0].read = 3;
				io_uring_sqe_set_data(sqe, &data[0]);
			} 
			else if (data_cqe->read == 2){
				get_sqe(&ring, &sqe);
				io_uring_prep_write(sqe, new_socket, buf[1], res, 0);
				data[1].read = 4;
				io_uring_sqe_set_data(sqe, &data[1]);
			}
			else {
				break;
			}	
			submit(&ring, &cqe);		
		}

	}

	io_uring_queue_exit(&ring);
	close(server_fd);

	return 0;
}