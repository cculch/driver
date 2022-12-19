#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#define NUM_GPIO_PINS	3
#define MAX_GPIO_NUMBER	4
#define BUF_SIZE	4
#define PATH_SIZE	20

int main(int argc, char *argv[])
{
	char path[PATH_SIZE], buf[BUF_SIZE];
	int i=0, fd=0;

	snprintf(path, sizeof(path), "/dev/LED_2");
	fd = open(path, O_WRONLY);
	if(fd < 0 )
	{
		perror("Error opening GPIO pin");
		exit(EXIT_FAILURE);
	}

	printf("Set GPIO pins\n");
	strncpy(buf, "0", 1);
	buf[1] = '\0';

	if( write(fd, buf, sizeof(buf)) < 0)
	{
		perror("write, set pin");
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}
