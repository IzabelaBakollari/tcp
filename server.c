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

static int get_sqe(struct io_uring *ring){

	 struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

	if (!sqe) {
		fprintf(stderr, "Could not get SQE.\n");
		return 1;
	}

	return 0;
}

static int process_cqe(struct io_uring *ring){

	struct io_uring_cqe *cqe;

	int ret = io_uring_submit(ring);

	if (ret < 0) {
		fprintf(stderr, "Error submitting buffers: %s\n", strerror(-ret));
		return 1;
	}

	ret = io_uring_wait_cqe(ring, &cqe);
	
	if (ret < 0) {
		fprintf(stderr, "Error waiting for completion: %s\n", strerror(-ret));
		return 1;
	}

	if (cqe->res < 0) {
		fprintf(stderr, "Error in async operation: %s\n", strerror(-cqe->res));
		return 1;
	}

	io_uring_cqe_seen(ring, cqe);

	return 0;
}

int main(int argc, char const *argv[])
{
	int server_fd, new_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buf[BUF_SZ] = {0};
	struct io_uring ring;
	struct io_uring_sqe sqe;
	
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
		return 1;
	}

	for (;;) {

		bzero(buf, BUF_SZ);
		get_sqe(&ring);
		io_uring_prep_read(&sqe, new_socket, buf, BUF_SZ, 0);
		ret = process_cqe(&ring);
		
		if (ret)
			break;
		
		printf("%s\n",buf);

		bzero(buf, BUF_SZ);
		int n = 0;
		while ((buf[n++] = getchar()) != '\n');
	
		get_sqe(&ring);
		io_uring_prep_write(&sqe, new_socket, buf, strlen(buf), 0);
		ret = process_cqe(&ring);
		
		if (ret)
			break;

		printf("Message sent to the client\n");
	}

	io_uring_queue_exit(&ring);
	close(server_fd);

	return 0;
}