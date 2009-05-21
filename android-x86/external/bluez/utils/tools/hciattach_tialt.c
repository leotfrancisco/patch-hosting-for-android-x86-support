/*
 *
 *	BlueZ - Bluetooth protocol stack for Linux
 *
 *	Copyright (C) 2000-2001  Qualcomm Incorporated
 *	Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *	Copyright (C) 2002-2008  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/endian.h>
#include <sys/uio.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define FAILIF(x, args...) do {   \
	if (x) {					  \
		fprintf(stderr, ##args);  \
		return -1;				  \
	}							  \
} while(0)

typedef struct {
	uint8_t uart_prefix;
	hci_event_hdr hci_hdr;
	evt_cmd_complete cmd_complete;
	uint8_t status;
	uint8_t data[16];
} __attribute__((packed)) command_complete_t;

extern int read_hci_event(int fd, unsigned char* buf, int size);
extern int set_speed(int fd, struct termios *ti, int speed);

static int read_command_complete(int fd, uint16_t opcode) {
	command_complete_t resp;
	/* Read reply. */
	FAILIF(read_hci_event(fd, (unsigned char *)&resp, sizeof(resp)) < 0,
		   "Failed to read response for opcode %#04x\n", opcode);
	
	/* Parse speed-change reply */
	FAILIF(resp.uart_prefix != HCI_EVENT_PKT,
		   "Error in response: not an event packet, but %#02X!\n", 
		   resp.uart_prefix);

	FAILIF(resp.hci_hdr.evt != EVT_CMD_COMPLETE, /* event must be event-complete */
		   "Error in response: not a cmd-complete event, "
		   "but 0x%02x!\n", resp.hci_hdr.evt);

	FAILIF(resp.hci_hdr.plen < 4, /* plen >= 4 for EVT_CMD_COMPLETE */
		   "Error in response: plen is not >= 4, but %#02X!\n",
		   resp.hci_hdr.plen);

	/* cmd-complete event: opcode */
	FAILIF(resp.cmd_complete.opcode != (uint16_t)opcode,
		   "Error in response: opcode is %#04X, not %#04X!",
		   resp.cmd_complete.opcode, opcode);

	FAILIF(resp.status != 0,
		"Error in response: status is %x for opcode %#04x\n", resp.status,
		   opcode);

	return resp.status == 0 ? 0 : -1;
}

typedef struct {
	uint8_t uart_prefix;
	hci_command_hdr hci_hdr;
	uint32_t speed;
} __attribute__((packed)) texas_speed_change_cmd_t;

static int texas_change_speed(int fd, struct termios *ti, uint32_t speed) {

	/* Send a speed-change request */
	texas_speed_change_cmd_t cmd;
	int n;

	cmd.uart_prefix = HCI_COMMAND_PKT;
	cmd.hci_hdr.opcode = 0xff36;
	cmd.hci_hdr.plen = sizeof(uint32_t);
	cmd.speed = speed;

	printf("Setting speed to %d\n", speed);
	n = write(fd, &cmd, sizeof(cmd));
	if (n < 0) {
		perror("Failed to write speed-set command");
		return -1;
	}

	if (n < (int)sizeof(cmd)) {
		fprintf(stderr, "Wanted to write %d bytes, could only write %d. "
				"Stop\n", (int)sizeof(cmd), n);
		return -1;
	}

	/* Parse speed-change reply */
	if (read_command_complete(fd, 0xff36) < 0) {
		return -1;
	}

	if (set_speed(fd, ti, speed) < 0) {
		perror("Can't set baud rate");
		return -1;
	}

	printf("Texas speed changed to %d.\n", speed);
	return 0;
}

static int texas_load_firmware(int fd, const char *firmware) {
	int fw;

	printf("Loading firmware from %s...\n", firmware);
	fw = open(firmware, O_RDONLY);
	FAILIF(fw < 0, "Could not open firmware file %s: %s (%d).\n",
		   firmware, strerror(errno), errno);

	while (1) {
		unsigned char cmd[1024];
		hci_command_hdr *hci_cmd;
		unsigned int n;

		/* read in H4 packets from firmware file */
		n = read(fw, cmd, sizeof(hci_command_hdr) + 1);
		if (!n) break;
		FAILIF(n != sizeof(hci_command_hdr) + 1,
			   "Could not read H4 + HCI header!\n");
		FAILIF(cmd[0] != HCI_COMMAND_PKT,
			   "Command is not an H4 command packet!\n");
		hci_cmd = (hci_command_hdr *)&cmd[1];

		FAILIF(read(fw, &cmd[sizeof(hci_command_hdr) + 1], hci_cmd->plen)
			   != hci_cmd->plen,
			   "Could not read %d bytes of data for opcode %#04X!\n",
			   hci_cmd->plen, hci_cmd->opcode);

		/* write h4 packet to chip */
//		printf("opcode %#04X (%d bytes data)\n", hci_cmd->opcode, hci_cmd->plen);
		n = write(fd, cmd, sizeof(hci_command_hdr) + 1 + hci_cmd->plen);
		FAILIF(n != sizeof(hci_command_hdr) + 1 + hci_cmd->plen,
			   "Could not send entire command (sent only %d bytes)!\n",
			   n);
		if (read_command_complete(fd, hci_cmd->opcode) < 0) {
			return -1;
		}
	}
	close(fw);

	printf("Firmware load successful.\n");

	return 0;
}

int texasalt_init(int fd, int speed, struct termios *ti)
{
	struct timespec tm = {0, 50000};
	char cmd[4];
	unsigned char resp[100];  /* Response */
	int n;

	memset(resp,'\0', 100);

	/* It is possible to get software version with manufacturer specific
	   HCI command HCI_VS_TI_Version_Number. But the only thing you get more
	   is if this is point-to-point or point-to-multipoint module */

	/* Get Manufacturer and LMP version */
	cmd[0] = HCI_COMMAND_PKT;
	cmd[1] = 0x01;
	cmd[2] = 0x10;
	cmd[3] = 0x00;

	do {
		n = write(fd, cmd, 4);
		if (n < 0) {
			perror("Failed to write init command (READ_LOCAL_VERSION_INFORMATION)");
			return -1;
		}
		if (n < 4) {
			fprintf(stderr, "Wanted to write 4 bytes, could only write %d. Stop\n", n);
			return -1;
		}

		/* Read reply. */
		if (read_hci_event(fd, resp, 100) < 0) {
			perror("Failed to read init response (READ_LOCAL_VERSION_INFORMATION)");
			return -1;
		}

		/* Wait for command complete event for our Opcode */
	} while (resp[4] != cmd[1] && resp[5] != cmd[2]);

	/* Verify manufacturer */
	if ((resp[11] & 0xFF) != 0x0d)
		fprintf(stderr,"WARNING : module's manufacturer is not Texas Instrument\n");

	/* Print LMP version */
	fprintf(stderr, "Texas module LMP version : %#02X\n", resp[10] & 0xFF);

	/* Print LMP subversion */
	{
		uint16_t lmp_subv = resp[13] | (resp[14] << 8);
		uint16_t brf_chip = (lmp_subv & 0x7c00) >> 10;
		static const char *c_brf_chip[5] = {
			"unknown",
			"unknown",
			"brf6100",
			"brf6150",
			"brf6300"
		};

		fprintf(stderr, "Texas module LMP sub-version : %#04X\n", lmp_subv);

		fprintf(stderr,
				"\tinternal version freeze: %d\n"
				"\tsoftware version: %d\n"
				"\tchip: %s (%d)\n",
				lmp_subv & 0x7f,
				((lmp_subv & 0x8000) >> (15-3)) | ((lmp_subv & 0x380) >> 7),
				((brf_chip > 4) ? "unknown" : c_brf_chip[brf_chip]),
				brf_chip);

		char fw[100];
		sprintf(fw, "/etc/firmware/%s.bin", c_brf_chip[brf_chip]);

		texas_change_speed(fd, ti, speed);

		texas_load_firmware(fd, fw);
	}
	nanosleep(&tm, NULL);
	return 0;
}
