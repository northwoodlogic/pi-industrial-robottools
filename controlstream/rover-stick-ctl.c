#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can.h>
#include <linux/can/raw.h>

int canfd = -1;

int
setup_socketcan()
{
		canfd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
		if (canfd < 0) {
				perror("socket");
				return -1;
		}

		struct ifreq ifr;
		//strcpy(ifr.ifr_name, "vcan0");
		strcpy(ifr.ifr_name, "can0");
		ioctl(canfd, SIOCGIFINDEX, &ifr);

		struct sockaddr_can addr;
		memset(&addr, 0, sizeof(addr));
		addr.can_family = AF_CAN;
		addr.can_ifindex = ifr.ifr_ifindex;

		if (bind(canfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			perror("bind");
			close(canfd);
			return -1;
		}

		return 0;
}

void
send_socketcan(int16_t throttle, int16_t steering)
{
		printf("sending: throttle(%d), steering(%d)\n",
				(int)throttle, (int)steering);

		struct can_frame frame;
		frame.can_id = 0x14;
		//frame.can_id = 0x15; // for testing, i don't actually want to drive any motors yet
		frame.can_dlc = 4;
		frame.data[0] = (uint8_t)(throttle >> 8);
		frame.data[1] = (uint8_t)(throttle);
		frame.data[2] = (uint8_t)(steering >> 8);
		frame.data[3] = (uint8_t)(steering);
		
		write(canfd, &frame, sizeof(frame));

}

static int sockfd = -1;
static int clientok = 0;
struct sockaddr_in clientaddr;

static void
error(char *msg) {
  perror(msg);
  exit(1);
}

static int
start_server()
{
  struct sockaddr_in serveraddr;

  memset(&clientaddr, 0, sizeof(clientaddr));

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  int optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
         (const void *)&optval , sizeof(int));

  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)666);
  
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
       sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  return 0;
}

static int
check_for_incoming(uint8_t buf[], int len)
{
    int clientlen = sizeof(clientaddr);

    int n = recvfrom(sockfd, buf, len, 0,
         (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return -1;

        error("Unknown error");
    }
    return n;
}

int main(int argc, char *argv[]) {
	
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s, j;
    size_t len;
    ssize_t nread;

    uint8_t buf[64];

    start_server();
    

	if (setup_socketcan()) {
		return 1;
	}

	while (1) {
		int n = check_for_incoming(buf, 64);
		if (n < 4)
			continue;

		int16_t throttle = (uint16_t)buf[0] << 8 | buf[1];
		int16_t steering = (uint16_t)buf[2] << 8 | buf[3];

		send_socketcan(throttle, steering);
	}
	
	return 0;
}
