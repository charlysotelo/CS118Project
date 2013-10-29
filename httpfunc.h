#ifndef HTTP_FUNC_H
#define HTTP_FUNC_H

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

void error(char *msg);

char * getValueFromParameter(char * message, char * parameter);
http_request * createRequest(char * message);
char * handleRequest(http_request * p_request, int * replySize);

#endif //HTTP_FUNC_H
