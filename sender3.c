#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<stdbool.h>
#include<err.h>
#include<errno.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<signal.h>

#define MAXLINE 1008
#define HEADER 8

char filename[20];
char *filedata;

int readfile(void);
void dg_cli(int, struct sockaddr*, socklen_t);

int main(int argc, char *argv[]){

	if(argc != 4)
		err(1, "[Usage] sender1 <server IP> <Server Port> <File name>\n");
	
	int sockfd;
	struct sockaddr_in server;
	
	strcpy(filename, argv[3]);
	
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err(2, "[Error] Failed in creating socket\n");

	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(argv[1]);
	server.sin_port = htons(atoi(argv[2]));

	dg_cli(sockfd, (struct sockaddr *)&server, sizeof(server));


	return 0;
}

void dg_cli(int sockfd, struct sockaddr* server, socklen_t len){
	
	int current_ack, n;
	int usecond = 10000;
	int filelen = readfile();
	char sendline[MAXLINE];
	char recvline[HEADER+1];
	bool ok_to_send;

	struct timeval tv;
	tv.tv_sec = 0;

	//Step 1, sending file name
	current_ack = 0;
	memset(sendline, 0, sizeof(sendline));
	strcpy(sendline, "00000000");
	strcat(sendline, filename);
	
	ok_to_send = true;
	while(1){

		tv.tv_usec = usecond;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	
		if(ok_to_send){
			int temp = sendto(sockfd, sendline, strlen(sendline), 0, server, len);
			
			printf("%d\n", temp);
		}
			
		if((n = recvfrom(sockfd, recvline, sizeof(recvline), 0, NULL, NULL)) < 0){
			if(errno == EWOULDBLOCK){
				printf("[Timeout] Resending filename datagram\n");
				
				if(usecond < 500000)
					usecond += 10000;
				ok_to_send = true;
			}
			else
				err(4, "[Error] Recvfrom has error\n");
		}
		else{
			recvline[8] = '\n';
			if(current_ack == atoi(recvline)){
				printf("[Success] Filename Ack received\n");

				if(usecond >= 20000)
					usecond -= 10000;

				current_ack++;

				break;
			}
			else
				ok_to_send = false;
		}
	}

	//step 2, sending file data
	bool end = false;
	int i = 0;
	int current_read;
	
	while(!end){
		memset(sendline, 0, sizeof(sendline));
		memset(recvline, 0, sizeof(recvline));
		
		tv.tv_usec = usecond;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

		char num[HEADER];
		sprintf(num, "%d", current_ack);
		strcpy(sendline, num);
		
		current_read = i;
		for(; i < filelen; i++){
			if((i-current_read) >= (MAXLINE-HEADER))
				break;

			sendline[i-current_read+HEADER] = filedata[i];
		}

		if(i == filelen)
			end = true;

		ok_to_send = true;
		while(1){
			sendto(sockfd, sendline, (i-current_read+HEADER), 0, server, len);

			if((n = recvfrom(sockfd, recvline, sizeof(recvline), 0, NULL, NULL)) < 0){
				if(errno == EWOULDBLOCK){
					printf("[Timeout] Resending Packet %d\n", current_ack);
				
					if(usecond < 500000)
						usecond += 10000;
					ok_to_send = true;
				}
				else
					err(4, "[Error] Recvfrom has error\n");
			}
			else{
				recvline[8] = '\n';
				if(current_ack == atoi(recvline)){
					printf("[Success] Packet %d ack received\n", current_ack);

					if(usecond >= 20000)
						usecond -= 10000;
					break;
					current_ack++;
				}
				else
					ok_to_send = false;
			}	

		}

		current_ack++;
	}
	
	//step 3, sending end packet
	current_ack = 99999999;
	strcpy(sendline, "99999999");
	
	ok_to_send = true;
	while(1){

		sendto(sockfd, sendline, strlen(sendline), 0, server, len);
		
		tv.tv_usec = usecond;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		
		if((n = recvfrom(sockfd, recvline, sizeof(recvline), 0, NULL, NULL)) < 0){
			if(errno == EWOULDBLOCK){
				printf("[Timeout] Resending end packet\n");
				
				if(usecond < 500000)
					usecond += 10000;
				ok_to_send = true;
			}
			else
				err(4, "[Error] Recvfrom has error\n");
		}
		else{
			recvline[8] = '\n';
			if(current_ack == atoi(recvline)){
				printf("[Success] End packet ack received\n");

				if(usecond >= 20000)
					usecond -= 10000;
				break;
			}
			else
				ok_to_send = false;
		}

	}
	
	printf("[Complete] All data has sent\n");

	return;
}

int readfile(void){
	
	FILE *ifile;

	if((ifile = fopen(filename, "r")) == NULL)
		err(3, "[Error] Failed in opening the file\n");

	int n;
	fseek(ifile, 0, SEEK_END);
	n = ftell(ifile);
	fseek(ifile, 0, SEEK_SET);

	filedata = malloc(n * sizeof(char));

	fread(filedata, n, 1, ifile);

	return n;
}
