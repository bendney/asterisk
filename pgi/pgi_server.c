#include <stdio.h>  
#include <stdlib.h>  
#include <stdbool.h>
#include <string.h>  

#include <unistd.h>
#include <errno.h>  

#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  



#define EVENT_ACCOUTS_LOAD					1
#define EVENT_ACCOUTS_UPDATE				2
#define EVENT_CONFERENCE_CREATE			3

#define DEFAULT_PORT								8700

#define MAXLINE											10240

#define SIP_ADDITIONAL_CONF					"sip_additional.conf"

typedef struct shardPacket {
  int message;
  char buffer[MAXLINE];
} SHAREDPACKET;


bool loadAccounts(const char * accountsFile, char * buffer, int length)
{
  FILE *file = NULL;
  int readBytes = 0;

  file = fopen(accountsFile, "r");
  if (file == NULL) {
    return false;
  }

  readBytes = fread(buffer, length, 1, file);

  fclose(file);

  return true;
}

bool updateAccounts(const char * accountsFile, char * buffer, int length)
{
  FILE *file = NULL;
  int writeBytes = 0;

  file = fopen(accountsFile, "w+");
  if (file == NULL) {
    return false;
  }

  writeBytes = fwrite(buffer, length, 1, file);
  if (writeBytes == 0) {
    return false;
  }

  fclose(file);

  system("asterisk -rx 'core reload'");

  return true;
}

bool createConference(const char *conference)
{
	char  comman1[64] =  {'\0'};
	char  comman2[64] =  {'\0'};

	snprintf(comman1, 64, "asterisk -rx 'dialplan add extension %s,1,Answer() into app-confbridge'", conference);
	snprintf(comman2, 64, "asterisk -rx 'dialplan add extension %s,1,ConfBridge(%s) into app-confbridge'", conference);

	system(comman1);
	system(comman2);

	return true;
}

static void event_control(int handle, char * buffer, int bufferSize, int event)
{
	bool status = false;
	SHAREDPACKET outPackets;

	switch (event) {
		case EVENT_ACCOUTS_LOAD:
		{
			printf("Event accounts load\n");
			status = loadAccounts(SIP_ADDITIONAL_CONF, outPackets.buffer, MAXLINE);
			if (status == false) {
				outPackets.message = 21;
				memset(outPackets.buffer, 0, MAXLINE);
				send(handle, &outPackets,
						sizeof(outPackets.message) + strlen(outPackets.buffer), 0);
				break;
			}

			outPackets.message = 11;
			send(handle, &outPackets,
					sizeof(outPackets.message) + strlen(outPackets.buffer), 0);

			break;
		}

		case EVENT_ACCOUTS_UPDATE:
		{
			printf("Event accounts update\n");
			status = updateAccounts(SIP_ADDITIONAL_CONF, buffer, bufferSize);
			if (status == false) {
				outPackets.message = 22;
				memset(outPackets.buffer, 0, MAXLINE);
				send(handle, &outPackets,
						sizeof(outPackets.message) + strlen(outPackets.buffer), 0);
				break;
			}

			outPackets.message = 12;
			memset(outPackets.buffer, 0, MAXLINE);
			send(handle, &outPackets,
					sizeof(outPackets.message) + strlen(outPackets.buffer), 0);
			break;
		}
		case EVENT_CONFERENCE_CREATE:
		{
			printf("Event conference create\n");
			status = createConference(buffer);
			if (status == false) {
				outPackets.message = 23;
				memset(outPackets.buffer, 0, MAXLINE);
				send(handle, &outPackets,
						sizeof(outPackets.message) + strlen(outPackets.buffer), 0);
				break;
			}

			outPackets.message = 13;
			memset(outPackets.buffer, 0, MAXLINE);
			send(handle, &outPackets,
					sizeof(outPackets.message) + strlen(outPackets.buffer), 0);
			break;
		}
		default:
			break;
	}
}


int main(int argc, char** argv)  
{  
	int socket_fd;
	int handle;  
	struct sockaddr_in servaddr;  
	int n ;  
	int readBytes = 0;
	bool status = false;

	FILE * file = NULL;

	SHAREDPACKET inPackets;

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1 ) {  
		printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);  
		exit(0);  
	} 

	memset(&servaddr, 0, sizeof(servaddr));  
	servaddr.sin_family = AF_INET;  
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(DEFAULT_PORT);

	if (bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){  
		printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);  
		exit(0);  
	}  

	if (listen(socket_fd, 10) == -1){  
		printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);  
		exit(0);  
	}  

	printf("Event server socket listening...\n");

	while(true){
		if ((handle = accept(socket_fd, (struct sockaddr*)NULL, NULL)) == -1){  
			printf("Accept socket error: %s(errno: %d)", strerror(errno), errno);  
      continue;  
    }

    if (!fork()) {
      while (true) {
        memset(&inPackets, 0, sizeof(inPackets));

        n = recv(handle, &inPackets, MAXLINE, 0);
        if (n == -1) {
          printf("Listen socket error: %s(errno: %d)", strerror(errno), errno);  
          break;
        }

				event_control(handle, inPackets.buffer, n - 4, inPackets.message);
      }
    }
  }
  close(socket_fd);
  close(handle);
}

