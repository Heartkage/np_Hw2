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

char filedata[MAXLINE];

int msgdecode(char*, int);
void dg_server(int, struct sockaddr*, socklen_t);

int main(int argc, char *argv[]) {

    if(argc != 2) {
        fprintf(stderr, "[Usage] ./server <SERVER_IP>\n");
        exit(-1);
    }

    int sockfd;
    struct sockaddr_in server, client;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(atoi(argv[1]));

    bind(sockfd, (struct sockaddr *) &server, sizeof(server));

    dg_server(sockfd, (struct sockaddr *) &client, sizeof(client));

}

void dg_server(int sockfd, struct sockaddr* client, socklen_t clilen){

	int n;
	socklen_t len;
	char recvline[MAXLINE];
	char sendline[HEADER];

	FILE *ofile;
	int current_ack, last_ack = -1;

	while(1){
		
		len = clilen;
		
		if((n = recvfrom(sockfd, recvline, sizeof(recvline), 0, client, &len)) < 0)
			err(2, "[Error] Recvfrom Failed\n");
		
		printf("%d data received\n", n);
		
		current_ack = msgdecode(recvline, n);
		


		if((current_ack == 0) && ((last_ack < current_ack) || (last_ack == 99999999))){
			
			char temp[20];
			strcpy(temp, "result/");
			strcat(temp, filedata);

			ofile = fopen(temp, "w");
			ftruncate(fileno(ofile), 0);
			fseek(ofile, 0, SEEK_SET);
			
			strcpy(sendline, "00000000");
			sendto(sockfd, sendline, strlen(sendline), 0, client, len);
			printf("[New File] File name received, creating new file...\n");
			last_ack = current_ack;
			
		}
		else if((current_ack == 0) && (last_ack == 0)){
			printf("[Duplicate] File name exist, resending ack...\n");
			strcpy(sendline, "00000000");
			
			sendto(sockfd, sendline, strlen(sendline), 0, client, len);
		}
		else if((current_ack == 99999999) && (last_ack < current_ack)){
			printf("[Complete] EOF received, saving the file\n");
			strcpy(sendline, "99999999");
			sendto(sockfd, sendline, strlen(sendline), 0, client, len);
			fclose(ofile);
			last_ack = current_ack;
		}
		else if((current_ack == 99999999) && (last_ack == current_ack)){
			printf("[Duplicate] EOF data exist, resending EOF ack\n");
			strcpy(sendline, "99999999");
			sendto(sockfd, sendline, strlen(sendline), 0, client, len);
		}
		else if(current_ack > last_ack){
			
			fwrite(filedata, 1, n-HEADER, ofile);
			printf("[Writing...] Writing data %d to the file\n", current_ack);
			
			sendto(sockfd, recvline, HEADER, 0, client, len);
			last_ack = current_ack;
		}
		else if(last_ack >= current_ack){
			printf("[Duplicate] Data %d already exist, resending %d ack\n", current_ack, current_ack);
			
			sendto(sockfd, recvline, HEADER, 0, client, len);
		}
		else{
			printf("[Error] Server Shutting down\n");
			exit(1);
		}	

	}

	return;
}

int msgdecode(char *recvline, int n){

	char num[HEADER];

	for(int i = 0; i < HEADER; i++)
		num[i] = recvline[i];	
	
	for(int i = HEADER; i < n; i++)
		filedata[i-HEADER] = recvline[i];

	return (atoi(num));
}
