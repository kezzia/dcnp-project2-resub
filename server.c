#include <sys/socket.h>    // socket definitions
#include <sys/types.h>    // socket types
#include <arpa/inet.h>    // inet (3) funtions
#include <unistd.h>      // misc. UNIX functions
#include <errno.h>
#include "sendlib.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

typedef int bool;
#define true 1
#define false 0

int ascii_to_int(unsigned long long num);
int binaryToDecimal(long long n);
unsigned long long int_to_ascii(int num);
int amount_ascii(int n);
unsigned long long decimalToBinary(int n);
void pad_binary(unsigned long long n, bool isAmt);
int type0_to_type1(char* message);
int type1_to_type0(char* message);
int read_type(char *message);


FILE *output;

int main(int argc, char *argv[]) {
  int sock;        // listening socket
  struct  sockaddr_in myaddr; // socket address structure for server
  struct sockaddr_storage cliaddr; // socket address structure for client
  socklen_t clilen;    // Client socket length
  int msglen;       // Message length
  int port;         // port number
  char *endptr;        // for strtol()
  float loss_ratio;
  int rseed;
  int Maxlen= 1024;
  char *output_file_path;
  char *output_Name;
  char *error_msg = "error";
  int format;




  if ( argc == 4 ) {
    port = strtol(argv[1], &endptr, 0);
    loss_ratio = atof(argv[2]);
    rseed = atoi(argv[3]);
    if ( *endptr ) {
      fprintf(stderr, "Invalid port number.\n");
      return 0;
    }
  } else if (argc < 2) {
    fprintf(stderr, "Invalid args\n");
    return 0;
  }

  printf("Server running on port %i\n", port);


  // Create the listening socket

  if((sock = socket (PF_INET, SOCK_DGRAM, 0)) < 0) {
    fprintf(stderr, "Error creating listening socket.\n");
    exit(EXIT_FAILURE);
  }

  memset((char*) &myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_port = htons(port); //This is my port
  myaddr.sin_addr.s_addr = htonl( INADDR_ANY );

  // Bind our socket addresss to the socket

  if(bind(sock, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
    fprintf(stderr, "Error\n");
    return 0;
  }

  // Recieve Request from Client Program

  clilen = sizeof(cliaddr);
  while(1) {
    char msg[1024];
    Maxlen = 1024;
	RECEIVE:

    msglen = recvfrom(sock, msg, Maxlen, 0, (struct sockaddr *)&cliaddr, &clilen);
    if(msglen < 0) {
      printf("Did not receive any data\n");
      return 0;
    }
    //extract data drom message received
    char *reply;
    reply = strtok(msg, ",");
    output_file_path = reply;
    reply = strtok(NULL, ",");
    format = atoi(reply);
    reply = strtok(NULL, ",");
    output_Name = reply;

    //This is the ack message for the stop and wait.
    char ack[1024];
    sprintf(ack, "%s", "ack");
    Maxlen = strlen(msg)+1;
    msglen = lossy_sendto(loss_ratio, rseed, sock, ack, Maxlen, (struct sockaddr *)&cliaddr, clilen);
		if (msg < 0) {
			goto RECEIVE;
			printf("The ack didn't send\n");
		}

    //Opening file for writing
    output = fopen(output_Name, "w+");

    //Recieveing the data from file line by line.
    //CHANGING FORMAT OF DATA
    int success = 0;
    while(1)
    {
      char line[1024];
      Maxlen = 1024;
		RECEIVE1:
      //Recieveing data from file line by line.
      msglen = recvfrom(sock, line, Maxlen, 0, (struct sockaddr *)&cliaddr, &clilen);
      if(msglen < 0) {
        printf("Error receiveing data\n");
      }

      /* An ack message. if this is not sent we time out*/
      char ack[1024];
      sprintf(ack, "%s", "ack");
      Maxlen = strlen(msg)+1;
      msglen = lossy_sendto(loss_ratio, rseed, sock, ack, Maxlen, (struct sockaddr *)&cliaddr, clilen);
			if (msglen < 0) {
				goto RECEIVE1;
   }

      //For ending loop to Finish Recieving
     if(line[0] == 'e') {
        success = 1;
        break;
      }
       if(format == 0) {
        // No translations so just reaad and write
        fprintf(output, "%s\n\n", line);
      } else if(format == 1) {
        int type = read_type(line);
        if(type == 1) {
          // Leaving type1 as type1
          fprintf(output, "%s", line);
        }
        // Format says change all type0 to type1
          // Changing type0 to type1
          else if(type == 0) {

          // This function is used tto convert type zero units to type one
          int res = type0_to_type1(line);
          if(res == -1) {
            // This closes the file and opens it again to wipe anything that was previously saved.
            fclose(output);
            //This is to send reply from Server
            //For Errors
            char msg[1024];
            sprintf(msg, "%s", "error");
            Maxlen = strlen(msg)+1;
            msglen = lossy_sendto(loss_ratio, rseed, sock, msg, Maxlen, (struct sockaddr *)&cliaddr, clilen);
          }
        } else {
          // This closes the file and opens it again to wipe anything that was previously saved.
          fclose(output);
          //This is to send reply from Server
          //For Errors
          char msg[1024];
          sprintf(msg, "%s", "error");
          Maxlen = strlen(msg)+1;
          msglen = lossy_sendto(loss_ratio, rseed, sock, msg, Maxlen, (struct sockaddr *)&cliaddr, clilen);
        }
      }
      else if(format == 2) {
      // Format says change all type1 to type0
        int type = read_type(line);
        if(type == 0) {
          // Leave type0 as type0
          fprintf(output, "%s", line);
        } else if(type == 1) {
          // Changing type1 to type0
          int res = type1_to_type0(line);
          if(res == -1) {
            // Wrong format wrtie error message
            // This closes the file and opens it again to wipe anything that was previously saved.
            fclose(output);
            //This is to send reply from Server
            //For Errors
            char msg[1024];
            sprintf(msg, "%s", "error");
            Maxlen = strlen(msg)+1;
            msglen = lossy_sendto(loss_ratio, rseed, sock, msg, Maxlen, (struct sockaddr *)&cliaddr, clilen);
          }
        }
        else {
          // Wrong format wrtie error message
          // This closes the file and opens it again to wipe anything that was previously saved.
          fclose(output);
          //This is to send reply from Server
          //For Errors
          char msg[1024];
          sprintf(msg, "%s", "error");
          Maxlen = strlen(msg)+1;
          msglen = lossy_sendto(loss_ratio, rseed, sock, msg, Maxlen, (struct sockaddr *)&cliaddr, clilen);
        }
      }
      else if(format == 3) {
      // Format for changing both types to the other
        int type = read_type(line);
        if(type == 0) {
          // Changing type0 to type1
          int res = type0_to_type1(line);
          if(res == -1) {
            // Wrong format wrtie error message
            // This closes the file and opens it again to wipe anything that was previously saved.
            fclose(output);
            //This is to send reply from Server
            //For Errors
            char msg[1024];
            sprintf(msg, "%s", "error");
            Maxlen = strlen(msg)+1;
            msglen = lossy_sendto(loss_ratio, rseed, sock, msg, Maxlen, (struct sockaddr *)&cliaddr, clilen);
          }
        }
        else if(type == 1) {
          // Changing type1 to type0
          int res = type1_to_type0(line);
          if(res == -1) {
            // Wrong format wrtie error message
            // This closes the file and opens it again to wipe anything that was previously saved.
            fclose(output);
            //This is to send reply from Server
            //For Errors
            char msg[1024];
            sprintf(msg, "%s", "error");
            Maxlen = strlen(msg)+1;
            msglen = lossy_sendto(loss_ratio, rseed, sock, msg, Maxlen, (struct sockaddr *)&cliaddr, clilen);
          }
        }
        else {
          // Wrong format wrtie error message
          // This closes the file and opens it again to wipe anything that was previously saved.
          fclose(output);
          //This is to send reply from Server
          //For Errors
          char msg[1024];
          sprintf(msg, "%s", "error");
          Maxlen = strlen(msg)+1;
          msglen = lossy_sendto(loss_ratio, rseed, sock, msg, Maxlen, (struct sockaddr *)&cliaddr, clilen);
        }
    } else {
        // Wrong format wrtie error message
        // This closes the file and opens it again to wipe anything that was previously saved.
        fclose(output);
        //This is to send reply from Server
        //For Errors
        char msg[1024];
        sprintf(msg, "%s", "error");
        Maxlen = strlen(msg)+1;
        msglen = lossy_sendto(loss_ratio, rseed, sock, msg, Maxlen, (struct sockaddr *)&cliaddr, clilen);
      }

      //Clear the line Variable
			memset(line, 0, strlen(line));

    }
		fclose(output);

    //success Fulling Recieved and converted Data
    if(success == 1)
    {
      //This is to send reply from Server
      char msg[1024];
      sprintf(msg, "%s", "sucess");
      Maxlen = strlen(msg)+1;
      //msglen = lossy_sendto(loss_ratio, rseed, sock, msg, Maxlen, (struct sockaddr *)&cliaddr, clilen);
			msglen = sendto(sock, msg, Maxlen, 0, (struct sockaddr *)&cliaddr, clilen);
			if (msglen < 0)
			{
				perror("Failed to send");
			}
    }

    //Close socket and End
    close(sock);
    break;
  }

}



int ascii_to_int(unsigned long long num)
{
  // This function converts ascii characters to integers
  int i = 1, sum = 0;
  while(num != 0)
  {
    int temp = num%100;
    if(temp < 48)
    {
      return -1;
    }
    temp = temp - 48;
    sum = sum + (temp*i);
    i = i*10;
    num = num/100;
  }
  return sum;
}

int binaryToDecimal(long long n)
{
  // This function converts binary number to decimal
  int decimalNumber = 0, i = 0, remainder;
  while (n!=0) {
    remainder = n%10;
    if(remainder < 0 || remainder > 1)
    {
      return -1;
    }
    n /= 10;
    decimalNumber += remainder*pow(2,i);
    ++i;
  }
  return decimalNumber;
}

unsigned long long int_to_ascii(int num) {
  // This converts integers to ascii values
  unsigned long long i = 1, sum = 0;
  if(num == 0)
    return 48;
  while(num != 0) {
    // Fix the sum type to unsigned long long
    int temp = num%10;
    temp = temp + 48;
    sum = sum + (i*temp);
    i = i * 100;
    num = num/10;
  }
  return sum;
}

int amount_ascii(int n) {
  // This functions ensure that the amount variable has has 3 bytes
  if(n<100) {
    n = n + 484800;
  }
  else {
    n = n + 480000;
  }
  return n;
}

unsigned long long decimalToBinary(int n)
{
  // This function converts Decimal to binary numbers
  unsigned long long binaryNumber = 0, i = 1;
  int remainder, step = 1;
  while (n!=0)
  {
    remainder = n%2;
    n /= 2;
    binaryNumber += remainder*i;
    i *= 10;
  }
  return binaryNumber;
}

void pad_binary(unsigned long long n, bool isAmt)
{
  // This function is responsible for putting all the leading zeros in for binary numbers
  if(n < 10)
  {
    if(isAmt == true)
    {
      fprintf(output, "0000000%lld ", n);
    }
    else
    {
      fprintf(output, "000000000000000%lld ", n);
    }
  }
  else if(n < 100)
  {
    if(isAmt == true)
    {
      fprintf(output, "000000%lld ", n);
    }
    else
    {
      fprintf(output, "00000000000000%lld ", n);
    }
  }
  else if(n < 500)
  {
    if(isAmt == true)
    {
      fprintf(output, "00000%lld ", n);
    }
    else
    {
      fprintf(output, "0000000000000%lld ", n);
    }
  }
  else if(n < 5000)
  {
    if(isAmt == true)
    {
      fprintf(output, "0000%lld ", n);
    }
    else
    {
      fprintf(output, "000000000000%lld ", n);
    }
  }
  else if(n < 50000)
  {
    if(isAmt == true)
    {
      fprintf(output, "000%lld ", n);
    }
    else
    {
      fprintf(output, "00000000000%lld ", n);
    }
  }
  else if(n < 500000)
  {
    if(isAmt == true)
    {
      fprintf(output, "00%lld ", n);
    }
    else
    {
      fprintf(output, "0000000000%lld ", n);
    }
  }
  else if(n < 5000000)
  {
    if(isAmt == true)
    {
      fprintf(output, "0%lld ", n);
    }
    else
    {
      fprintf(output, "000000000%lld ", n);
    }
  }
  else if(n < 50000000)
  {
    if(isAmt == true)
    {
      fprintf(output, "%lld ", n);
    }
    else
    {
      fprintf(output, "00000000%lld ", n);
    }
  }
  else if(n < 500000000)
  {
    fprintf(output, "0000000%lld ", n);
  }
  else if(n < 5000000000)
  {
    fprintf(output, "000000%lld ", n);
  }
  else if(n < 50000000000)
  {
    fprintf(output, "00000%lld ", n);
  }
  else if(n < 500000000000)
  {
    fprintf(output, "0000%lld ", n);
  }
  else if(n < 5000000000000)
  {
    fprintf(output, "000%lld ", n);
  }
  else if(n < 50000000000000)
  {
    fprintf(output, "00%lld ", n);
  }
  else if(n < 500000000000000)
  {
    fprintf(output, "0%lld ", n);
  }
  else if(n < 5000000000000000)
  {;
    fprintf(output, "%lld ", n);
  }
}

int type0_to_type1(char *message) {
  // This function is used tto convert type zero units to type one
  unsigned long long amt, number;
  int n=0;
  char *reply;
  reply = strtok(message, " ");
  int type = atoi(reply);
  fprintf(output, "00000001 ");
  reply = strtok(NULL, " ");
  amt = atoll(reply);
  n = binaryToDecimal(amt);
  amt = int_to_ascii(n);
  amt = amount_ascii(amt);
  fprintf(output, "%d ", amt);
  int i;
  for(i=0; i<n; i++) {
    reply = strtok(NULL, " ");
    number = atoll(reply);
    number = binaryToDecimal(number);
    if(number == -1) {
      return -1;
    }
    number = int_to_ascii(number);
    if(i != n-1) {
      fprintf(output, "%lld, ", number);
    }
    else {
      fprintf(output, "%lld\r\n", number);
    }
  }
  return 0;
}

int type1_to_type0(char* message)
{
  // This function convert type one to type zero
  fprintf(output, "00000000 ");
  int amt;
  char *reply;
  reply = strtok(message, " ");
  int type = atoi(reply);
  reply = strtok(NULL, " ");
  amt = atoi(reply);
  amt = ascii_to_int(amt);
  unsigned long long bin;
  bin = decimalToBinary(amt);
  pad_binary(bin, true);
  int i;
  for(i=0; i<amt; i++)
  {
    unsigned long long number = 0;
    char comma;
    if(i == amt-1)
      reply = strtok(NULL, " ");
    else
      reply = strtok(NULL, ", ");
    number = atoll(reply);
    number = ascii_to_int(number);
    if(number == -1)
    {
      return -1;
    }
    bin = decimalToBinary(number);
    pad_binary(bin, false);
  }
  fprintf(output, "\r\n");
  return 0;
}

int read_type(char* message)
{
  // This function is used to read in the type from the units in file
  int type;
  type = message[7] - '0';
  return type;
}
