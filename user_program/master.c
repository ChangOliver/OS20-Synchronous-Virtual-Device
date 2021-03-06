#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>

#define PAGE_SIZE 4096
#define BUF_SIZE 512
#define MAP_SIZE PAGE_SIZE * 1

size_t get_filesize(const char* filename) {
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}

size_t min(const size_t a, const size_t b) {
	return a < b ? a : b;
}

void printDmesg(int dev_fd, const unsigned long file_address) {
	ioctl(dev_fd, 0x12345676, file_address);
}

int main (int argc, char* argv[]) {
	char buf[BUF_SIZE];
	int i, dev_fd, file_fd;// the fd for the device and the fd for the input file
	size_t ret, file_size, total_size = 0, offset = 0;
	char file_name[50] = {0}, method[20] = {0};
	char *kernel_address = NULL, *file_address = NULL;
	struct timeval start;
	struct timeval end;
	double trans_time; //calulate the time between the device is opened and it is closed
	int N;

	N = atoi(argv[1]);
	strcpy(method, argv[argc-1]);

	if((dev_fd = open("/dev/master_device", O_RDWR)) < 0) {
		perror("failed to open /dev/master_device\n");
		return 1;
	}

	gettimeofday(&start ,NULL);
	for (int i = 0; i < N; i++) {
		if(ioctl(dev_fd, 0x12345677) == -1) { //0x12345677 : create socket and accept the connection from the slave 
			perror("ioctl server create socket error\n");
			return 1;
		}
		offset = 0;
		if((file_fd = open(argv[2+i], O_RDWR)) < 0) {
			perror("failed to open input file\n");
			return 1;
		}

		if((file_size = get_filesize(argv[2+i])) < 0) {
			perror("failed to get filesize\n");
			return 1;
		}
		total_size += file_size;

		switch(method[0]) {
			case 'f': //fcntl : read()/write()
				do {
					ret = read(file_fd, buf, sizeof(buf)); // read from the input file
					write(dev_fd, buf, ret);//write to the the device
				} while(ret > 0);
				break;
			case 'm': //mmap
	            while (offset < file_size) {
					size_t transfer_size = min(MAP_SIZE, file_size - offset);

	                if((file_address = mmap(NULL, transfer_size, PROT_READ, MAP_SHARED, file_fd, offset)) == MAP_FAILED) {
	                    perror("mapping input file");
	                    return 1;
	                }
	                if((kernel_address = mmap(NULL, transfer_size, PROT_WRITE, MAP_SHARED, dev_fd, offset)) == MAP_FAILED) {
	                    perror("mapping output device");
	                    return 1;
	                }
	                memcpy(kernel_address, file_address, transfer_size);
	                offset += transfer_size;
	                ioctl(dev_fd, 0x12345678, transfer_size);
	                printDmesg(dev_fd, (unsigned long)file_address);
	                munmap(file_address, transfer_size);
	            }
	            break;
		}
		if(ioctl(dev_fd, 0x12345679) == -1) { // end sending data, close the connection
			perror("ioctl server exits error\n");
			return 1;
		}
		close(file_fd);
	}
	close(dev_fd);

	gettimeofday(&end, NULL);
	trans_time = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec)*0.0001;
	printf("Transmission time: %lf ms, File size: %lu bytes\n", trans_time, total_size);

	return 0;
}