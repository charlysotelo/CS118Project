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

void error(char *msg)
{
    perror(msg);
    exit(1);
}

enum
{
  REQUEST_TYPE_UNKNOWN,
  GET,
  POST,
  HEAD,
  PUT,
  DELETE,
};

enum
{
  CONNECTION_UNKNOWN,
  PERSISTENT,
  NON_PERSISTENT,
};

struct http_request
{
  int m_enumMethod;
  char * m_szHttpVersion;
  char * m_szFileRequested;
  
  char * m_szHost;
  
  int m_enumConnectionType;
  
  char * m_szClientAgent;

  char * m_szLanguage;  
};

typedef struct http_request http_request;

char * getValueFromParameter(char * message, char * parameter)
{
  int i;
  char * value;
  char * valueLocation;
  char * parameterLocation = strstr(message,parameter);

  if(parameterLocation == NULL)
    return NULL;

  valueLocation = parameterLocation + strlen(parameter) + 1;
  for(i = 0;; i++)
  {
    if(valueLocation[i] == ' ' || valueLocation[i] == '\r')
      break;
  }
  
  value = malloc(i+1);
  strncpy(value,valueLocation,i);
  value[i] = '\0';
  
  //printf("valueLocation is %s\n",valueLocation);
  return value;
}

http_request * createRequest(char * message)
{
  int i;
  char * buffer;
  http_request * p_request;
  char messageCopy[8192];

  strcpy(messageCopy,message);
  p_request = malloc(sizeof(struct http_request));
  buffer = strtok(message," ");

  if(buffer == NULL)
    error("Request Message not formed correctly1.");

  if( !strcmp(buffer,"GET" ) )
    p_request->m_enumMethod = GET;
  else if( !strcmp(buffer,"POST") )
    p_request->m_enumMethod = POST;
  else if( !strcmp(buffer,"HEAD") )
    p_request->m_enumMethod = HEAD;
  else if( !strcmp(buffer,"PUT") )
    p_request->m_enumMethod = PUT;
  else if( !strcmp(buffer,"DELETE") )
    p_request->m_enumMethod = DELETE;
  else
    p_request->m_enumMethod = REQUEST_TYPE_UNKNOWN;

  //printf("method is %i\n",p_request->m_enumMethod);
  buffer = strtok(NULL," ");

  if(buffer == NULL)
    error("Request Message not formed correctly2.");

  //printf("Buffer length is %i\n",strlen(buffer));

  p_request->m_szFileRequested = malloc(strlen(buffer) + 1 );
  strcpy(p_request->m_szFileRequested, buffer);
  
  //printf("File Requested is %s\n",p_request->m_szFileRequested);

  buffer = strtok(NULL," \r\n");

  if(buffer == NULL)
    error("Request Message not formed correctly3.");
  
  p_request->m_szHttpVersion = malloc(strlen(buffer) + 1 );
  strcpy(p_request->m_szHttpVersion, buffer);

  //printf("Http version %s\n",p_request->m_szHttpVersion);  
  buffer = getValueFromParameter(messageCopy,"Host:"); 
 
  if(buffer == NULL)
    error("Request Message not formed correctly4."); 

  p_request->m_szHost = malloc(strlen(buffer) + 1 );
  strcpy(p_request->m_szHost,buffer);
 
  //printf("Host is %s\n",p_request->m_szHost);
   
  buffer = getValueFromParameter(messageCopy,"Connection:");

  if(buffer == NULL)
    error("Request Message not formed correctly5."); 

  if(!strcmp(buffer, "close"))
    p_request->m_enumConnectionType = NON_PERSISTENT;
  else if(!strcmp(buffer, "keep-alive"))
    p_request->m_enumConnectionType = PERSISTENT;
  else
    p_request->m_enumConnectionType = CONNECTION_UNKNOWN;

  buffer = getValueFromParameter(messageCopy,"User-Agent:");

  if(buffer == NULL)
    error("Request Message not formed correctly6");

  p_request->m_szClientAgent = malloc(strlen(buffer) + 1 );
  strcpy(p_request->m_szClientAgent,buffer);

  //printf("Client Agent is %s\n",p_request->m_szClientAgent);  

  buffer = getValueFromParameter(messageCopy,"Accept-Language:");

  if(buffer == NULL)
    error("Request Message not formed correctly7");

  p_request->m_szLanguage = malloc(strlen(buffer) + 1);
  strcpy(p_request->m_szLanguage,buffer); 
  //printf("Language is %s\n",p_request->m_szLanguage); 

  return p_request;
}

//returns http segment
char * handleRequest(http_request * p_request, int * replySize)
{
  int i;
  int statusCode;
  long int fileContentSize, headerSize;
  char   reply_header[8192] = "HTTP/1.1 ";
  char * reply_message; // size can only be determined after we get the file
  char * fileContents = NULL; 
  char lastModifiedBuffer[128] = "";
  char contentLengthBuffer[128] = "";
  char contentTypeBuffer[128] = "";
  //date
  
     time_t rawtime;
     struct tm * timeinfo;
     char dateBuffer[128];
     

     time(&rawtime);
     timeinfo = gmtime(&rawtime);

     strftime(dateBuffer,128,"Date: %a, %d %b %Y %T %Z\r\n",timeinfo); 

  if(p_request->m_enumMethod == GET)
  { 
    if( strcmp(p_request->m_szHttpVersion,"HTTP/1.1"))
    {
      //prepare a 505 error reply
      //printf("Preparing 505 reply\n");
      statusCode = 505;
    }

    p_request->m_szFileRequested;
    FILE * reqFile;
    // + 1 here removes the initial /
    reqFile = fopen(p_request->m_szFileRequested + 1, "rb");
    if(!reqFile)
    {
      //prepare a 404 reply message
      //statusCode = 404;
      strcat(reply_header,"404 Not Found\r\n");
    }
    else
    {
      statusCode = 200;
      strcat(reply_header,"200 OK\r\n");      

      fseek( reqFile,0, SEEK_END);
      fileContentSize = ftell( reqFile );
      sprintf(contentLengthBuffer,"Content-Length: %i\r\n",fileContentSize);
      rewind( reqFile );

      fileContents = malloc( fileContentSize );
      for( i = 0; i < fileContentSize; i++)
        fileContents[i] = fgetc( reqFile );
      
      //printf("fileContestsSize is %i\n",fileContentSize);
      fclose( reqFile );

      struct stat st;
      if(stat(p_request->m_szFileRequested + 1,&st))
        error("Error geting modified time\n");
      else
      {
        timeinfo = gmtime(&(st.st_mtime));
        strftime(lastModifiedBuffer,128,
        "Last-Modified: %a, %d %b %Y %T %Z\r\n",timeinfo); 
      }
      char * szExtension = NULL;
      for( i = strlen(p_request->m_szFileRequested + 1); i >= 0; i--)
      {
        if( (p_request->m_szFileRequested + 1)[i] == '.')
        {
          szExtension = p_request->m_szFileRequested + i + 2;
          break;
        }
      }
   
      //printf("Extension is %s\n",szExtension); 
      if(!strcmp(szExtension,"gif"))
        strcpy(contentTypeBuffer, "Content-Type: image/gif\r\n");
      else if(!strcmp(szExtension,"jpg"))
        strcpy(contentTypeBuffer, "Content-Type: image/jpg\r\n");
      else if(!strcmp(szExtension,"html"))
        strcpy(contentTypeBuffer, "Content-Type: text/html\r\n");
      else
        error("The content type of the requested file is unsupported");
     
      //else
      //  printf(" mtime = %11d.%.9ld\n",(long long) st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
    }
    strcat(reply_header,"Connection: close\r\n");
    strcat(reply_header,dateBuffer);
    strcat(reply_header,"Server: CS118Serv (Linux)\r\n");
    strcat(reply_header,lastModifiedBuffer);
    strcat(reply_header,contentLengthBuffer);
    strcat(reply_header,contentTypeBuffer);
    strcat(reply_header,"\r\n"); // we are done with headers 
    
    headerSize = strlen(reply_header);
    //printf("checkpoint combined %i\n",headerSize + fileContentSize);
    reply_message = malloc( headerSize + fileContentSize );
    //printf("checkpoint2. reply_header size is %i\n",strlen(reply_header));
    strcpy(reply_message,reply_header); 
    //printf("checkpoint3\n"); 
    for(i = 0 ; i < fileContentSize; i++)
    {
      reply_message[headerSize +i] = fileContents[i];
    }
    //strcat(reply_message,fileContents);
  }

  //*replySize = fileContentSize;
  //return fileContents;
  
  *replySize = headerSize + fileContentSize;
  //printf("replySize set to %i\n",*replySize);
  return reply_message;
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
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
     
     listen(sockfd,5);	//5 simultaneous connection at most
     
     //accept connections
     newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
     if (newsockfd < 0) 
       error("ERROR on accept");
         
     int n,replySize;
     char buffer[8192];
   			 
     memset(buffer, 0, 8192);	//reset memory
      
     //read client's message
     n = read(newsockfd,buffer,8192);
     if (n < 0) error("ERROR reading from socket");
   
     printf("The HTTP Request Is \n%s\n",buffer);    
 	 
     http_request * p_request = createRequest(buffer);

     char * messageReply = handleRequest(p_request,&replySize);
     
     
     printf("The HTTP Response Is \n%s\n",messageReply);
     //n = write(newsockfd, fileContents, size - 1);
     n = write(newsockfd, messageReply,replySize);
     if( n < 0 ) error("Error writing to socket");      
     
     close(newsockfd);//close connection 
     close(sockfd);
         
     return 0; 
}

