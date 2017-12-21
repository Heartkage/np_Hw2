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
#include<sys/stat.h>
#include<sys/types.h>

#define MAXLINE 1008
#define HEADER 8

char filedata[MAXLINE];

int ATOI(char *, int);
int msgdecode(char*, int);
void dg_server(int, struct sockaddr*, socklen_t);

int main(int argc, char *argv[]) {

    if(argc != 2) {
        fprintf(stderr, "[Usage] ./receiver <Port>\n");
        exit(-1);
    }

    int sockfd;
    struct sockaddr_in server, client;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&server, sizeof(server));
    bzero(&client, sizeof(client));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(atoi(argv[1]));

    if(bind(sockfd, (struct sockaddr *) &server, sizeof(server)) < 0)
	    exit(1);

    dg_server(sockfd, (struct sockaddr *) &client, sizeof(client));
	
	return 0;
}

void dg_server(int sockfd, struct sockaddr* client, socklen_t clilen){

	int n;
	socklen_t len;
	char recvline[MAXLINE];

	FILE *ofile;
	int current_ack, last_ack = -1;

	int status;
	status = mkdir("result", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	printf("[System] Server on!\n");

	while(1){
		memset(recvline, 0, sizeof(recvline));	
		len = clilen;

		if((n = recvfrom(sockfd, recvline, sizeof(recvline), 0, client, &len)) < 0){
			err(2, "[Error] Recvfrom Failed\n");
			exit(1);
		}		

		
		
		current_ack = msgdecode(recvline, n);
	
		printf("Header = %d, %d data received\n", current_ack, n);
			

		if((current_ack == 0) && ((last_ack < current_ack) || (last_ack == 99999999))){
			
			char temp[MAXLINE];
			memset(temp, 0, sizeof(temp));
			strcpy(temp, "result/");
			filedata[n-HEADER] = '\0';
			strcat(temp, filedata);

			ofile = fopen(temp, "w");
			ftruncate(fileno(ofile), 0);
			fseek(ofile, 0, SEEK_SET);
			
			char temp2[9];
			strcpy(temp2, "00000000");
			
			

			sendto(sockfd, temp2, HEADER, 0, client, len);
			printf("[New File] File name received, creating new file...\n");
			last_ack = current_ack;
			
		}
		else if((current_ack == 0) && (last_ack == 0)){
			printf("[Duplicate] File name exist, resending ack\n");

			sendto(sockfd, recvline, HEADER, 0, client, len);
		}
		else if((current_ack == 99999999) && (last_ack < current_ack)){
			printf("[Complete] EOF received, saving the file...\n");
				
			sendto(sockfd, recvline, HEADER, 0, client, len);
			fclose(ofile);
			last_ack = current_ack;
		}
		else if((current_ack == 99999999) && (last_ack == current_ack)){
			printf("[Duplicate] EOF data exist, resending EOF ack\n");
			
			sendto(sockfd, recvline, HEADER, 0, client, len);
		}
		else if(current_ack > last_ack){
			printf("[Writing...] Writing data %d to the file...\n", current_ack);
			fwrite(filedata, 1, n-HEADER, ofile);
			
			sendto(sockfd, recvline, HEADER, 0, client, len);
			last_ack = current_ack;
		}
		else if(last_ack >= current_ack){
			printf("[Duplicate] Data %d already exist, resending %d ack\n", current_ack, current_ack);
			
			sendto(sockfd, recvline, HEADER, 0, client, len);
		}
		else{
			printf("[Error] Server Shutting down\n");
			exit(-2);
		}	

	}

	return;
}

int msgdecode(char *recvline, int n){

	memset(filedata, 0, sizeof(filedata));
	char num[HEADER+1];
	int i;
	for(i = 0; i < HEADER; i++)
		num[i] = recvline[i];
	
			
	int amount = i;

	for(i = HEADER; i < n; i++)
		filedata[i-HEADER] = recvline[i];

	num[8] = '\0';
	return (atoi(num));
}

