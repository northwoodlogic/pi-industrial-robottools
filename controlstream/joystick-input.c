
#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

SDL_Joystick *js = NULL;

int canfd = -1;

int
setup_socketcan()
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    /* Obtain address(es) matching host/port */

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    //int s = getaddrinfo("192.168.88.120", "666", &hints, &result);
    int s = getaddrinfo("192.168.0.6", "666", &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
    Try each address until we successfully connect(2).
    If socket(2) (or connect(2)) fails, we (close the socket
    and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        canfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (canfd == -1)
            continue;

        if (connect(canfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

        close(canfd);
    }

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);           /* No longer needed */


#if 0
		canfd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
		if (canfd < 0) {
				perror("socket");
				return -1;
		}

		struct ifreq ifr;
		//strcpy(ifr.ifr_name, "vcan0");
		strcpy(ifr.ifr_name, "slcan0");
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
#endif

		
		return 0;
}

void
send_socketcan(int16_t throttle, int16_t steering)
{
		printf("sending: throttle(%d), steering(%d)\n",
				(int)throttle, (int)steering);

		struct can_frame frame;
		frame.can_id = 0x14;
		frame.can_dlc = 4;
		frame.data[0] = (uint8_t)(throttle >> 8);
		frame.data[1] = (uint8_t)(throttle);
		frame.data[2] = (uint8_t)(steering >> 8);
		frame.data[3] = (uint8_t)(steering);
	
//		write(canfd, &frame, sizeof(frame));

		write(canfd, frame.data, 4);

}



Uint32 tick_cb(Uint32 interval, void* param)
{
	SDL_Event event;
	SDL_UserEvent userevent;

	userevent.type = SDL_USEREVENT;
	userevent.code = 0;
	userevent.data1 = NULL;
	userevent.data2 = NULL;

	event.type = SDL_USEREVENT;
	event.user = userevent;
	SDL_PushEvent(&event);
	return interval;
}

int main(int argc, char *argv[]) {
	if (setup_socketcan()) {
		return 1;
	}

	if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_TIMER) < 0) {
		printf("could not init SDL\n");
		return 1;
	}

	if (SDL_NumJoysticks() < 1) {
		printf("NO joysticks found!\n");
		return 1;
	}

	js = SDL_JoystickOpen(0);
	if (js == NULL) {
		printf("Could not open joystick\n");
		return 1;
	}

	SDL_AddTimer(100, tick_cb, NULL);

	int16_t steering_position = 0;
	int16_t throttle_position = 0;

	SDL_Event e;
	while (1) {
		if (!SDL_WaitEvent(&e))
				continue;

		if (e.type == SDL_USEREVENT) {
			printf("update setpoints on CAN bus\n");
			send_socketcan(throttle_position, steering_position);
		}

//		printf("Polling\n");
		if (e.type == SDL_QUIT) {
			printf("Quitting!\n");
			break;
		}
		if (e.type == SDL_JOYAXISMOTION) {
			/*
			 * On my Logitech pad the left analog stick moving left -- right
			 * is axis zero
			 * The range of motion is
			 *     left  --> -32768   (ccw)
			 *     right -->  32767   (cw)
			 */
			if (e.jaxis.axis == 2) {
				//printf("Steering: %d, value: %d\n", e.jaxis.axis, e.jaxis.value / 64);
				steering_position = (int16_t)(e.jaxis.value / 64);
				if (steering_position < -511)
					steering_position = -511;

				/* This is scaling hack for the specifics of the hardware. platform */
				steering_position = steering_position / 3;
			}

			/* On my logitech pad the right analog stick moving up -- down
			 * is axis 3.
			 * The range of motion is
			 *     up --> -32768
			 *     dn -->  32767
			 *
			 * This sign is opposite of what is needed for the throttle controls so
			 * before sending it to the can bus multiply by -1.
			 */
			if (e.jaxis.axis == 1) {
				// printf("Throttle: %d, value %d\n", e.jaxis.axis, e.jaxis.value);
				throttle_position = (int16_t)((e.jaxis.value / 64) * -1);
				if (throttle_position > 511)
					throttle_position = 511;
			}
		}
	}

	return 0;
}

