/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <time.h>
#include "httpfunc.h"

#define MAX_CONNECTIONS 5

int main(int argc, char *argv[])
{
     int i,sockfd, portno, pid, clientsockets[MAX_CONNECTIONS];
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;

     if (argc < 2) 
     {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
    
     for( i = 0; i < MAX_CONNECTIONS; i++)
       clientsockets[i] = 0;
     
     sockfd = socket(AF_INET, SOCK_STREAM, 0);	//create socket
     if (sockfd < 0) 
        error("ERROR opening socket");
     memset((char *) &serv_addr, 0, sizeof(serv_addr));	//reset memory
     //fill in address info
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,MAX_CONNECTIONS);	//MAX_CONNECTIONS simultaneous connection at most
     fd_set active_fd_set;
     
     int max_fd;
     for(;;)
     {
       FD_ZERO(&active_fd_set);
       FD_SET(sockfd,&active_fd_set);
       max_fd = sockfd;
       for(i = 0 ; i < MAX_CONNECTIONS; i++)
       {
         int fd = clientsockets[i];

         if( fd > 0)
           FD_SET( fd, &active_fd_set );
    
         if( fd > max_fd)
           max_fd = fd; 
       }

       int socketReady = select(max_fd+1,&active_fd_set,NULL,NULL,NULL); 
       if( socketReady < 0)
         error("Select returned a negative number");
 
       if(FD_ISSET(sockfd,&active_fd_set)) //if the server socket is active
       {
         //accept a new connection
         int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
         if (newsockfd < 0) 
           error("ERROR on accept");
        
         for( i = 0; i < MAX_CONNECTIONS ; i++)
         {
           if(clientsockets[i] == 0)
           {
             clientsockets[i] = newsockfd;
             break;
           }
         } 
       }
       for(i = 0 ; i < MAX_CONNECTIONS; i++)
       {
         int fd = clientsockets[i];

         if( fd > 0)//active client. serve it
         {
           int n,replySize;
           char buffer[8192];
   			 
           memset(buffer, 0, 8192);	//reset memory
      
           //read client's message
           n = read(fd,buffer,8192);
           if (n < 0) error("ERROR reading from socket");
           printf("Client request: \n%s",buffer); 
           http_request * p_request = createRequest(buffer);
           char * messageReply = handleRequest(p_request,&replySize);
           printf("Response: \n%s",messageReply);
           n = write(fd, messageReply,replySize);
           if( n < 0 ) error("Error writing to socket");
           freeClientRequest(p_request);
           free(messageReply);      
           FD_CLR(fd,&active_fd_set);
           close(fd);//close connection 
           clientsockets[i] = 0; //invalidate socket;
         }    
       }
     }
     close(sockfd);
         
     return 0; 
}

