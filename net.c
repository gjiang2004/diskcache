#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

typedef struct {
  uint32_t opcode;
  uint8_t info;
  uint8_t payload[JBOD_BLOCK_SIZE];
} message_t;

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
bool nread(int fd, int len, uint8_t *buf) {
  int count, remain = len;
  while (remain > 0) {
    count = read(fd, buf, remain);
    remain -= count;
    if (count == 0) {
      return false;
    }
    buf = buf + count;
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
bool nwrite(int fd, int len, uint8_t *buf) {
  int count, remain = len;
  while (remain > 0) {
    count = write(fd, buf, remain);
    remain -= count;
    if (count == 0) {
      return false;
    }
    buf = buf + count;
  }
  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
bool recv_packet(int fd, uint32_t *op, uint8_t *ret, uint8_t *block) {
  message_t msg;
  msg.opcode = htonl(*op);
  msg.info = 0;
  if (nread(fd, HEADER_LEN, (uint8_t *) &msg) == false) {
    *ret = -1;
    return false;
  }
  int opcode = ntohl(msg.opcode);
  *op = opcode;
  int info = msg.info;
  if (info & 1) {
    *ret = -1;
    return false;
  } else {
    *ret = 0;
  }

  if (info & 2) {
    if (nread(fd, JBOD_BLOCK_SIZE, block) == false) {
      *ret = -1;
      return false;
    }
  }
  return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
bool send_packet(int sd, uint32_t op, uint8_t *block) {
  message_t msg;
  msg.opcode = htonl(op);
  msg.info = 0;
  op = op & 0x000FFFFF;
  op = op >> 12;
  if (block != NULL && op == JBOD_WRITE_BLOCK) {
    msg.info = 2;
    memcpy(msg.payload, block, JBOD_BLOCK_SIZE);
    if(nwrite(sd, JBOD_BLOCK_SIZE + HEADER_LEN, (uint8_t *) &msg) == false) {
      return false;
    }
  } else {
    if (nwrite(sd, HEADER_LEN, (uint8_t *) &msg) == false) {
      return false;
    }
  }
  return true;
}

/* connect to server and set the global client variable to the socket */
bool jbod_connect(const char *ip, uint16_t port) {
  cli_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (cli_sd == -1) {
    printf("Error on socket creation [%s]\n", strerror(errno));
    return false;
  }

  struct sockaddr_in caddr;
  caddr.sin_family = AF_INET;
  caddr.sin_port = htons(port);
  if (inet_aton(ip, &caddr.sin_addr) == 0) {
    return false;
  }

  if (connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1) {
    return false;
  }

  return true;
}

void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}

int jbod_client_operation(uint32_t op, uint8_t *block) {
  int opreceive = op;
  int ret = 0;
  //fprintf(stderr, "jbod op = %x, cli_sd = %d, block = %p\n", op, cli_sd, block);
  if (send_packet(cli_sd, op, block)) {
    //fprintf(stderr, "send done\n");
    if (recv_packet(cli_sd, (uint32_t *) &opreceive, (uint8_t *) &ret, (uint8_t *) block)) {
      //fprintf(stderr, "recv done\n");
      return 0;
    }
  }
  return -1;
}
