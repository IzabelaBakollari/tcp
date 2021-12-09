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

#define PORT   8080
#define BUF_SZ 1024

int main(int argc, char const *argv[])
{
	int server_fd, new_socket;
	//int valread;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buf[BUF_SZ] = {0};
	char *hello = "Hello from server";
	struct io_uring ring;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;

	
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

	//valread = read( new_socket , buf, BUF_SZ);
	//printf("%s\n",buf);
	//send(new_socket , hello , strlen(hello) , 0 );
	//printf("Hello message sent\n");

	// Start using io_uring library

	int ret = io_uring_queue_init(8, &ring, 0);

	if (ret) {
		fprintf(stderr, "Unable to setup io_uring: %s\n", strerror(-ret));
		return 1;
	}

	sqe = io_uring_get_sqe(&ring);

	if (!sqe) {
		fprintf(stderr, "Could not get SQE.\n");
		return 1;
	}

	//void io_uring_prep_read(struct io_uring_sqe *sqe, int fd, void *buf, unsigned nbytes, off_t offset)

	io_uring_prep_read(sqe, new_socket, buf, BUF_SZ, 0);
	sqe->flags |= IOSQE_IO_LINK;

	printf("%s\n",buf);


	sqe = io_uring_get_sqe(&ring);

	if (!sqe) {
		fprintf(stderr, "Could not get SQE.\n");
		return 1;
	}

	//void io_uring_prep_write(struct io_uring_sqe *sqe, int fd, const void *buf, unsigned nbytes, off_t offset)
	
	io_uring_prep_write(sqe, new_socket, hello, BUF_SZ, 0);
	sqe->flags |= IOSQE_IO_LINK;

	printf("Hello message sent\n");


	ret = io_uring_submit_and_wait(&ring, 2);

	if (ret < 0) {
		fprintf(stderr, "Error submitting buffers: %s\n", strerror(-ret));
		return 1;
	}

	for (int i = 0; i < 2; i++) { 

		ret = io_uring_peek_cqe(&ring, &cqe);
	
		if (ret < 0) {
			fprintf(stderr, "Error waiting for completion: %s\n", strerror(-ret));
			return 1;
		}

		if (cqe->res < 0) {
			fprintf(stderr, "Error in async operation: %s\n", strerror(-cqe->res));
		}

		io_uring_cqe_seen(&ring, cqe);
	}

	io_uring_queue_exit(&ring);

	return 0;
}