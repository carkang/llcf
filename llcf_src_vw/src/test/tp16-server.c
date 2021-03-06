/*
 *  $Id: tp16-server.c,v 1.8 2005/12/19 13:39:15 hartko Exp $
 */

/*
 *  Low Level CAN Framework
 *
 *  Copyright (c) 2005 Volkswagen Group Electronic Research
 *  38436 Wolfsburg, GERMANY
 *
 *  contact email: llcf@volkswagen.de
 *
 *  Idea, Design, Implementation:
 *  Oliver Hartkopp <oliver.hartkopp@volkswagen.de>
 *  Dr. Urs Thuermann <urs.thuermann@volkswagen.de>
 *  Matthias Brukner <m.brukner@trajet.de>
 *
 *  Neither Volkswagen Group nor the authors admit liability
 *  nor provide any warranty for any of this software.
 *  This material is provided "AS-IS".
 *
 *  Until the distribution is granted by the Volkswagen rights
 *  department this sourcecode is under non disclosure and must
 *  only be used within projects with the Volkswagen Group.
 *
 */

#define DYNCHS

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "af_can.h"
#include "tp16.h"

#ifdef DYNCHS

#include "raw.h"

#define LOCAL_TP_ID  2
#define REMOTE_TP_ID 1
#define LOCAL_TP_TX_CHANNEL 0x62
#define BUS_SPECIFIC_OFFSET 0x700
#endif

#define DEV "vcan2"

char buffer[4096];

int main(int argc, char **argv)
{
    int s, n, i, nbytes, sizeofpeer=sizeof(struct sockaddr_can);
    struct sockaddr_can addr, peer;
    struct ifreq ifr;

#ifdef DYNCHS

    struct can_filter rfilter;
    struct can_frame frame;

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
	perror("raw socket");
	return 1;
    }

    /* listen only for CAN-IDs 0x200 - 0x2FF */

    rfilter.can_id   = 0x200;
    rfilter.can_mask = 0xF00;

    setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

    addr.can_family = AF_CAN;
    strcpy(ifr.ifr_name, DEV);
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	perror("raw bind");
	return 1;
    }

    memset(&frame, 0, sizeof(frame)); /* clear receive frame */

    while (frame.can_dlc == 0) {

	if ((nbytes = read(s, &frame, sizeof(frame))) < 0)
	    perror("raw read");

	if (frame.data[0] != LOCAL_TP_ID) {
	    printf("No CHS for me: LOCAL_TP_ID %d(%d)\n",
		   frame.data[0], LOCAL_TP_ID);
	    frame.can_dlc = 0; /* stay in while () */
	}
	if (frame.can_dlc != 3 ) {
	    printf("No CHS for me: can_dlc %d(3)\n",
		   frame.can_dlc);
	    frame.can_dlc = 0; /* stay in while () */
	}
    }

    /* remark: opposite RX is my TX :o) */
    addr.can_addr.tp16.tx_id  = BUS_SPECIFIC_OFFSET + LOCAL_TP_TX_CHANNEL;
    addr.can_addr.tp16.rx_id  = BUS_SPECIFIC_OFFSET + frame.data[2];

    /* build reply */
    frame.data[0] = frame.can_id & 0xFF; /* destination */
    frame.can_id = 0x200 + LOCAL_TP_ID; /* sender */

    frame.data[1] = 0xD0; /* positive reply */
    frame.data[2] = LOCAL_TP_TX_CHANNEL;

    if (write(s, &frame, sizeof(frame)) < 0)
      perror("raw write");


    close(s); /* RAW socket is not needed anymore */

#endif /* DYNCHS */

    if ((s = socket(PF_CAN, SOCK_SEQPACKET, CAN_TP16)) < 0) {
	perror("socket");
	exit(1);
    }

#ifdef DYNCHS

    /* addr is already created at this point */

#else

    /* test setup without dynamic channel setup */

    addr.can_family = AF_CAN;
    strcpy(ifr.ifr_name, DEV);
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_ifindex = ifr.ifr_ifindex;
    addr.can_addr.tp16.tx_id  = 0x760;
    addr.can_addr.tp16.rx_id  = 0x740;

#endif

    printf("\nWait for connection on %s: tx_id=0x%03X rx_id=0x%03X ... ",
	   DEV, addr.can_addr.tp16.tx_id, addr.can_addr.tp16.rx_id);
    fflush(stdout);

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	perror("bind");
	exit(1);
    }

    listen(s, 1);

    if ((n = accept(s, (struct sockaddr *)&peer, &sizeofpeer)) < 0) {
	perror("accept");
	exit(1);
    }

    printf("connected!\n\nWaiting for message ... ");
    fflush(stdout);

    nbytes = read(n, buffer, 4096);
    printf("ok.\nReceived %d bytes: '", nbytes);
    buffer[nbytes] = 0; /* ensure terminated string */
    printf("%s'\n\n", buffer);

    for (i=0; i<nbytes; i++)
	buffer[i] = toupper(buffer[i]);

    printf("Sending modified message '%s' ...", buffer);
    write(n, buffer, nbytes);
    printf(" done!\n\n");

    /* hm - this looks like a workarround to ensure the data to be */
    /* sent until the connection is closed ... fix this in tp16.c? */
    /* what is the correct behaviour here?? */
    sleep(4); /* 4 seconds to check the AUTO_DISCONNECT feature */

    printf("Closing connection.\n\n");

    close(n);
    close(s);

    return 0;
}
