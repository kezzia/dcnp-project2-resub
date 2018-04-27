#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "sendlib.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <signal.h>

typedef int bool;
#define true 1
#define false 0
#define timeout_time 5

FILE *input;

ssize_t suspicious_recvfrom(int sockfd, const void *buff, size_t nbytes,
 int flags, struct sockaddr* from, socklen_t *fromaddrlen);
int handle_alarm();


int main(int argc, char *argv[]) {
  int sock;        // listening socket
  struct  sockaddr_in myaddr;
  struct sockaddr_storage storage;
  socklen_t server_socket_len, storagelen;
  int msglen;
  int port;
  char *server_ip;
  char *szPort;
  char *endptr;
  int MaxMsgLen = 1024;


  char *file_path;
  char *to_format;
  char *target;

  char ack[255];
  int acklen = 255;
  float loss_probability;
  int random_seed;

  // Get command line arguments

  if (argc != 8) {
   printf("Too few args!\n");
   return 1;
  }

  server_ip = argv[1];
  szPort = argv[2];
  file_path = argv[3];
  to_format = argv[4];
  target = argv[5];
  loss_probability = atof(argv[6]);
  random_seed = atoi(argv[7]);

  port = strtol(szPort, &endptr, 0);
  if ( *endptr ) {
    printf("Invalid port number.\n");
    exit(EXIT_FAILURE);
  }

  // Create the listening socket
  if((sock = socket (PF_INET, SOCK_DGRAM, 0)) < 0)
  {
    fprintf(stderr, "Could not create listening socket.\n");
    exit(EXIT_FAILURE);
  }

  memset((char*) &myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_port = htons(port); //This is my port
  myaddr.sin_addr.s_addr = inet_addr(server_ip); // This is my IP

  // Setting the socket length base off of the socket address.

  server_socket_len = sizeof(myaddr);
  storagelen = sizeof(storage);


  // Send Request to Server Program

  while(1) {
    // Sending the First paramenters to the Server

    char msg[1024];
    sprintf(msg, "%s,%s,%s", file_path,to_format,target);
    MaxMsgLen = strlen(msg)+1;

 /* A label we can jump back to to resend in case we time out.
  * Apparently labels work in C */
	RESEND:
    int packet_timeout = 0;
    if (packet_timeout >= 5) {
      printf("Too many timeouts exiting\n");
      return 0;
    }
    msglen = lossy_sendto(loss_probability, random_seed, sock, msg, MaxMsgLen, (struct sockaddr *)&myaddr, server_socket_len);

    acklen = 500;
    int test = suspicious_recvfrom(sock, ack, acklen, 0, (struct sockaddr *)&storage, &storagelen);
    /* if our suspicious_recvfrom does not work then we want to resend the
     * packet */
    if(test == -5) {
      printf("Packet timed out. Resending!\n");
      packet_timeout += 1;
      goto RESEND;
    }
    if(test == -4) {
      printf("An error occured!\n");
      return 0;
    }

    //Opening the File to send the data.
    input = fopen(file_path, "r");
    if(input == NULL) {
      printf("Invalid input file\n");
      return 0;
    }

    //Sending the data from File line by line.
    int maxsize = 1024;
    char line[1024];
    char message[1024];
    while(1) {
      if(fgets(line, sizeof(line), input) == NULL)
      {
        //Sending end to Server so it know we are at end of file
        char end[] = "end";
        memset(message,0,strlen(message));
        sprintf(message, "%s", end);
        maxsize = strlen(message) + 1;

        // The line below is a label to jump back to in order to resend the last packet as there was a timeout.
        RESEND1:
        msglen = lossy_sendto(loss_probability, random_seed, sock, message, maxsize, (struct sockaddr *)&myaddr, server_socket_len);
        acklen = 500;

        test = suspicious_recvfrom(sock, ack, acklen, 0, (struct sockaddr *)&storage, &storagelen);
        //This code would take you back to resend the last bit of data due to a timeout
        if(test == -5)
        {
          printf("Paket timedout at end of file RESENDING\n\n");
          goto RESEND1;
        }
        if(test == -4)
        {
          printf("Some other error occured\n");
          return 0;
        }
        break;
      }

      //Send line by line of the data in the file
      sprintf(message, "%s", line);
      maxsize = strlen(message) +1;

      // The line below is a label to jump back to in order to resend the last packet as there was a timeout.
      RESEND2:
      msglen = lossy_sendto(loss_probability, random_seed, sock, message, maxsize, (struct sockaddr *)&myaddr, server_socket_len);


   /* Expects to recieve an ack from Server for each line of data from file*/

			test = 0;
      test = suspicious_recvfrom(sock, ack, acklen, 0, (struct sockaddr *)&storage, &storagelen);
      //This code would take you back to resend the last bit of data due to a timeout
      if(test == -5) {
        printf("Packet timed out. Resending\n\n");
        goto RESEND2;
      }
      if(test == -4) {
        printf("Error\n");
        return 0;
      }
    }

    //Handle the Last Response
    char ack[1024];
    MaxMsgLen = 1024;
		  test = 0;
    test = recvfrom(sock, ack, MaxMsgLen, 0, (struct sockaddr *)&storage, &storagelen);
    if(test < 0 )
      perror("Recieve error");

    if(ack[0] == 'e' && ack[1] == 'r') {
      printf("\nto_format Error!\n");
    }
    else if(ack[0] == 's' && ack[1] == 'u') {
      printf("\nSuccess!\n");
    }

    //If we get all the way here it was a success
    break;

  }
}

int handle_alarm() {
 /* Throws an error and resets the alarm after an alarm goes off */
	errno = EINTR;
	alarm(0);
	return -1;
}

ssize_t suspicious_recvfrom(int sockfd, const void *buff, size_t nbytes,
 int flags, struct sockaddr* from, socklen_t *fromaddrlen)
{
  /* Stop and wait */
  /*This was Dwight's idea */
  /* can interrupt the rcvfrom so it doesn't get stuck if we receive nothing */
  ssize_t n;

  struct sigaction action;
  sigemptyset(&action.sa_mask);
  action.sa_handler = (void (*)(int))handle_alarm;
  action.sa_flags = 0;
  if(sigaction(SIGALRM, &action, 0) == -1) {
    perror("sigaction");
    return 1;
  }

	//signal(SIGALRM, handle_alarm);
	alarm(2);
  n = recvfrom(sockfd, buff, nbytes, flags, from, fromaddrlen);
  if (n < 0) {
		if (errno == EINTR) {
			/* if this happens we have a timeout */
			return -5;
		}
    else {
      /* some other error */
      perror("The reciving on client error is");
      return -4;
    }
  }
  else {
    /* no error or time out means we can turn off the alarm */
    alarm(0);
  }
  return n;
}
