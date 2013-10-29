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

void error(char *msg)
{
    perror(msg);
    exit(1);
}
 
void freeClientRequest( http_request * p_request)
{
  free(p_request->m_szHttpVersion);
  free(p_request->m_szFileRequested);
  free(p_request->m_szHost);
  free(p_request->m_szClientAgent);
  free(p_request->m_szLanguage);

  free(p_request);
}
//----------------------------------------------------------------
//Purpose:
//-Used to modularily extract a parameter value from a request.
//
//Parameters:
//-message is the http request message sent by the client.
//-parameter is the Header name for the value that you request.
//
//Returns:
//-The value, as a string, of the given parameter in message.
//----------------------------------------------------------------
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
    if(valueLocation[i] == ' ' || valueLocation[i] == '\r')
      break;
  
  value = malloc(i+1);
  strncpy(value,valueLocation,i);
  value[i] = '\0';
  
  return value;
}

//----------------------------------------------------------------
//Purpose:
//-Creates a nice struct http_request defined defined in httpfunc.h
// from the passed request message
//
//Parameters:
//-message is the http request message sent by the client.
//
//Returns:
//-A pointer to a "nice" struct defined in httpfunc.h
//----------------------------------------------------------------
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

  buffer = strtok(NULL," ");

  if(buffer == NULL)
    error("Request Message not formed correctly2.");

  p_request->m_szFileRequested = malloc(strlen(buffer) + 1 );
  strcpy(p_request->m_szFileRequested, buffer);
  
  buffer = strtok(NULL," \r\n");

  if(buffer == NULL)
    error("Request Message not formed correctly3.");
  
  p_request->m_szHttpVersion = malloc(strlen(buffer) + 1 );
  strcpy(p_request->m_szHttpVersion, buffer);

  buffer = getValueFromParameter(messageCopy,"Host:"); 
 
  if(buffer == NULL)
    error("Request Message not formed correctly4."); 

  p_request->m_szHost = malloc(strlen(buffer) + 1 );
  strcpy(p_request->m_szHost,buffer);
  
  free(buffer);
  buffer = getValueFromParameter(messageCopy,"Connection:");

  if(buffer == NULL)
    error("Request Message not formed correctly5."); 

  if(!strcmp(buffer, "close"))
    p_request->m_enumConnectionType = NON_PERSISTENT;
  else if(!strcmp(buffer, "keep-alive"))
    p_request->m_enumConnectionType = PERSISTENT;
  else
    p_request->m_enumConnectionType = CONNECTION_UNKNOWN;

  free(buffer);
  buffer = getValueFromParameter(messageCopy,"User-Agent:");

  if(buffer == NULL)
    error("Request Message not formed correctly6");

  p_request->m_szClientAgent = malloc(strlen(buffer) + 1 );
  strcpy(p_request->m_szClientAgent,buffer);

  free(buffer);
  buffer = getValueFromParameter(messageCopy,"Accept-Language:");

  if(buffer == NULL)
    error("Request Message not formed correctly7");

  p_request->m_szLanguage = malloc(strlen(buffer) + 1);
  strcpy(p_request->m_szLanguage,buffer); 

  return p_request;
}

//----------------------------------------------------------------
//Purpose:
//-Creates a nice struct http_request defined defined in httpfunc.h
// from the passed request message
//
//Parameters:
//-http_request is defined in httpfunc.h - it basically is just a 
// nicely structured,parsed, and easily accesible version of a
// client http request.
//-replySize is a pointer to an unused int. It is set to the total size
// of the http response message if any.
//
//Returns:
//-Formatted request which is made up of both http headers and the 
// requested data (if requested and found)
//----------------------------------------------------------------
char * handleRequest(http_request * p_request, int * replySize)
{
  int i;
  int statusCode;
  long int fileContentSize = 0, headerSize;
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
      statusCode = 505;

    p_request->m_szFileRequested;
    FILE * reqFile;
    
    // + 1 here removes the initial '/'
    reqFile = fopen(p_request->m_szFileRequested + 1, "rb");
    if(!reqFile)
      strcat(reply_header,"404 Not Found\r\n");
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
   
      if(!strcmp(szExtension,"gif"))
        strcpy(contentTypeBuffer, "Content-Type: image/gif\r\n");
      else if(!strcmp(szExtension,"jpg"))
        strcpy(contentTypeBuffer, "Content-Type: image/jpg\r\n");
      else if(!strcmp(szExtension,"html"))
        strcpy(contentTypeBuffer, "Content-Type: text/html\r\n");
      else if(!strcmp(szExtension,"txt"))
        strcpy(contentTypeBuffer, "Content-Type: text/plain\r\n");
      else//unknown type
        strcpy(contentTypeBuffer, "Content-Type: application/octet-stream\r\n");
    }
    strcat(reply_header,"Connection: close\r\n");
    strcat(reply_header,dateBuffer);
    strcat(reply_header,"Server: CS118Serv (Linux)\r\n");
    strcat(reply_header,lastModifiedBuffer);
    strcat(reply_header,contentLengthBuffer);
    strcat(reply_header,contentTypeBuffer);
    strcat(reply_header,"\r\n"); // we are done with headers 
    
    headerSize = strlen(reply_header);
    reply_message = malloc( headerSize + fileContentSize );
    strcpy(reply_message,reply_header); 
    for(i = 0 ; i < fileContentSize; i++)
      reply_message[headerSize +i] = fileContents[i];
  }
  free(fileContents);
  *replySize = headerSize + fileContentSize;
  return reply_message;
}
