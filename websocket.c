#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <errno.h>

#include <http_parser.h>
#include <net.h>

#define using_more
#include <sha1.h>

#define LOG_FATAL(fmt, ...) do {printf("FATAL: " fmt "\n",##__VA_ARGS__); fflush(stdout); }while(0)
#define LOG_ERROR(fmt, ...) do {printf("ERROR: " fmt "\n",##__VA_ARGS__); fflush(stdout); }while(0)
#define LOG_INFO(fmt, ...)  do {printf("INFO : " fmt "\n",##__VA_ARGS__); fflush(stdout); }while(0)
#define LOG_DEBUG(fmt, ...) do {printf("DEBUG: " fmt "\n",##__VA_ARGS__); fflush(stdout); }while(0)

//
// Settings
//
//#define DATA_BUFFER_SIZE 1024
#define DATA_BUFFER_SIZE 1 // Use 1 for development to expose corner cases and bugs

#define EVENT_BUFFER_COUNT 32
#define HTTP_LISTEN_PORT 81
#define HTTP_BACKLOG 32
//#define HTTP_PROCESS_BUFFER_SIZE 32 // Note: must be big enough to hold the biggest header that we are interested in
#define HTTP_PROCESS_BUFFER_SIZE 24 // Note: must be big enough to hold the biggest header that we are interested in
#define WEBSOCKET_URL "/MyWebSocket"

//const char WebSocketResponseFormat[] =
//  "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\nSec-WebSocket-Protocol: %s\r\n\r\n";
const char WebSocketResponsePartA[] =
  "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
const char WebSocketResponsePartB[] = "\r\nSec-WebSocket-Protocol: ";
const char WebSocketResponsePartC[] = "\r\n\r\n";

const char WebSocketKeyPostfix[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

typedef void (*EpollHandler)(void* ptr);

typedef struct  {
  EpollHandler handler; // must be the first field
  int sock;
  int state;
} SockEpollHandler;

typedef struct {
  char array[HTTP_PROCESS_BUFFER_SIZE];
  unsigned savedContentLength;
  unsigned droppedContentLength;
} HttpProcessBuffer;

typedef struct {
  EpollHandler handler; // must be the first field
  int sock;
  http_parser httpParser;


  #define HTTP_PROCESS_STATE_URL           0
  #define HTTP_PROCESS_STATE_HEADER_FIELD  1

  #define HTTP_PROCESS_STATE_HEADER_VALUE_FIRST         2
  #define HTTP_PROCESS_STATE_HEADER_VALUE_GENERIC       2
  #define HTTP_PROCESS_STATE_HEADER_VALUE_WEBSOCKET_KEY 3
  #define HTTP_PROCESS_STATE_HEADER_VALUE_LAST          3

  #define HTTP_PROCESS_STATE_BODY          4
  byte state;

  HttpProcessBuffer buf;
  
  Sha1 sha;

} HttpEpollHandler;

void HttpEpollHandlerAdd(HttpEpollHandler* handler, const char* data, size_t len)
{
  size_t copySize = sizeof(handler->buf.array) - handler->buf.savedContentLength;
  if(len <= copySize) {
    copySize = len;
  } else {
    handler->buf.droppedContentLength += (len - copySize);
  }
  
  if(copySize > 0) {
    memcpy(handler->buf.array + handler->buf.savedContentLength, data, copySize);
    handler->buf.savedContentLength += copySize;
    //LOG_DEBUG("HttpEpollHandlerAdd: %.*s", handler->contentLen, handler->buf);
  }
}
#define HttpEpollHandlerBufEquals(handler, literal)			\
  (handler->buf.savedContentLength == sizeof(literal)-1 &&		\
   memcmp(handler->buf.array, literal, sizeof(literal)-1) == 0)

int epollfd;
int httpListenSock;
http_parser_settings httpParserSettings;
char buf[DATA_BUFFER_SIZE];


void EncodeSha(ubyte* buffer, HttpEpollHandler* handler)
{
  ubyte sha1HashBytes[SHA1_HASH_BYTE_LENGTH];
  int i;
  int sha1Off = 0;
  for(i = 0; i < 5; i++) {
    sha1HashBytes[sha1Off++] = handler->sha.hash[i] >> 24;
    sha1HashBytes[sha1Off++] = handler->sha.hash[i] >> 16;
    sha1HashBytes[sha1Off++] = handler->sha.hash[i] >>  8;
    sha1HashBytes[sha1Off++] = handler->sha.hash[i]      ;
  }
  base64Encode(sha1HashBytes, SHA1_HASH_BYTE_LENGTH, buffer);
}


void WebSocketHandler(void* ptr)
{
  ssize_t received;
  int sock = ((HttpEpollHandler*)ptr)->sock;

  received = recv(sock, buf, sizeof(buf), 0);
  if(received <= 0) {
    if(received == 0) {
      LOG_INFO("(s=%d) connection closed", sock);
    } else {
      LOG_ERROR("(s=%d) recv failed (e=%d)", sock, errno);
    }
    close(sock); // should be removed from epollfd automatically
    free(ptr);
  } else {
    LOG_DEBUG("(s=%d) WebSocket got %d bytes", sock, received);
  }
}

void HttpHeaderHandler(void* ptr)
{
  ssize_t received;
  int sock = ((HttpEpollHandler*)ptr)->sock;

  received = recv(sock, buf, sizeof(buf), 0);
  if(received <= 0) {
    if(received == 0) {
      LOG_INFO("(s=%d) connection closed", sock);
    } else {
      LOG_ERROR("(s=%d) recv failed (e=%d)", sock, errno);
    }
    close(sock); // should be removed from epollfd automatically
    free(ptr);
  } else {

    int parseLength;

    //LOG_DEBUG("Got %lu bytes from (s=%d)", received, sock);
    parseLength = http_parser_execute(&((HttpEpollHandler*)ptr)->httpParser,
				      &httpParserSettings, buf, received);

    if(((HttpEpollHandler*)ptr)->state == HTTP_PROCESS_STATE_BODY) {
      ssize_t sent;
      size_t toSend;
      ubyte response[1024];

      LOG_DEBUG("Done with receiving initial HTTP headers");

      toSend = 0;
      memcpy(response + toSend, WebSocketResponsePartA, sizeof(WebSocketResponsePartA)-1);
      toSend += sizeof(WebSocketResponsePartA)-1;

      EncodeSha(response + toSend, ((HttpEpollHandler*)ptr));
      toSend += 27;
      response[toSend++] = '=';

      memcpy(response + toSend, WebSocketResponsePartB, sizeof(WebSocketResponsePartB)-1);
      toSend += sizeof(WebSocketResponsePartB)-1;
      memcpy(response + toSend, "custom_websocket_proto", sizeof("custom_websocket_proto") - 1);
      toSend += sizeof("custom_websocket_proto") - 1;
      memcpy(response + toSend, WebSocketResponsePartC, sizeof(WebSocketResponsePartC)-1);
      toSend += sizeof(WebSocketResponsePartC)-1;

      sent = send(sock, response, toSend, 0);
      if(sent != toSend) {
	LOG_ERROR("(s=%d) tried to send %d but only sent %d", toSend, sent);
	close(sock);
	free(ptr);
	return;
      }

      ((HttpEpollHandler*)ptr)->handler = &WebSocketHandler;
      
    }

    if(parseLength != received) {
      LOG_DEBUG("only parsed %d bytes of %ld, closing socket...", parseLength, received);
      shutdown(sock, SHUT_RDWR);
      close(sock);
      free(ptr);
    }
  }
}

void HttpAcceptHandler(void* ptr)
{
  struct sockaddr from;
  socklen_t fromlen = sizeof(from);
  char fromString[MAX_ADDR_CHARS];
  struct epoll_event epollEvent;

  int newSock = accept(httpListenSock, &from, &fromlen);
  if(newSock == -1) {
    LOG_ERROR("accept on http listener failed (e=%d)", errno);
    // TODO: handle the error somehow
    return;
  }

  addrToString(fromString, &from);
  LOG_INFO("Accepted new connection (s=%d) from '%s'", newSock, fromString);

  epollEvent.data.ptr = malloc(sizeof(HttpEpollHandler));
  if(epollEvent.data.ptr == NULL) {
    LOG_FATAL("HttpAcceptHandler: malloc failed (e=%d)", errno);
    // TODO: signal exit somehow
    return;
  }
  //LOG_DEBUG("HttpEpollHandler* = %p", epollEvent.data.ptr);

  ((HttpEpollHandler*)epollEvent.data.ptr)->handler = &HttpHeaderHandler;
  ((HttpEpollHandler*)epollEvent.data.ptr)->sock    = newSock;
  http_parser_init(&((HttpEpollHandler*)epollEvent.data.ptr)->httpParser, HTTP_REQUEST);
  ((HttpEpollHandler*)epollEvent.data.ptr)->state   = HTTP_PROCESS_STATE_URL;
  //sha1Init(&((HttpEpollHandler*)epollEvent.data.ptr)->sha);
  
  epollEvent.events   = EPOLLIN;
  if(epoll_ctl(epollfd, EPOLL_CTL_ADD, newSock, &epollEvent) == -1) {
    LOG_FATAL("epoll_ctl failed for http listener (e=%d)", errno);
    // TODO: signal exit somehow
    return;
  }
}
static const EpollHandler httpAcceptHandlerPtr = &HttpAcceptHandler;


#define PARSER_PTR_TO_HANDLER_PTR(parser) (HttpEpollHandler*)((char*)(parser) - offsetof(HttpEpollHandler, httpParser))


int httpParserOnMessageBegin(http_parser* parser)
{
  LOG_DEBUG("http-parser: message-begin");
}
int httpParserOnMessageComplete(http_parser* parser)
{
  LOG_DEBUG("http-parser: message-complete");
  
}
int httpParserOnUrl(http_parser* parser, const char* at, size_t length)
{
  HttpEpollHandler* handler = PARSER_PTR_TO_HANDLER_PTR(parser);

  //LOG_DEBUG("http-parser: url '%.*s'", (int)length, at);

  if(handler->state != HTTP_PROCESS_STATE_URL) {
    LOG_ERROR("at url callback but current state is %d", handler->state);
    return 1;
  }

  HttpEpollHandlerAdd(handler, at, length);
  return 0;
}

int handleState0(HttpEpollHandler* handler, int newState)
{
  if(handler->state == HTTP_PROCESS_STATE_URL) {
    if(!HttpEpollHandlerBufEquals(handler, WEBSOCKET_URL)) {
      LOG_ERROR("Unknown URL '%.*s'", (int)handler->buf.savedContentLength, handler->buf.array);
      return 1;
    }

    LOG_DEBUG("Got valid url %s", WEBSOCKET_URL);
    handler->buf.savedContentLength = 0; // Remove the url
    handler->buf.droppedContentLength = 0;
    handler->state = newState;
  } else if(handler->state >= HTTP_PROCESS_STATE_HEADER_VALUE_FIRST &&
	    handler->state <= HTTP_PROCESS_STATE_HEADER_VALUE_LAST) {

    LOG_DEBUG("HttpHeaderValue '%.*s%s'", (int)handler->buf.savedContentLength, handler->buf.array,
	      handler->buf.droppedContentLength ? "..." : "");
    handler->buf.savedContentLength = 0; // Remove the url
    handler->buf.droppedContentLength = 0;
    
    if(handler->state == HTTP_PROCESS_STATE_HEADER_VALUE_WEBSOCKET_KEY) {
      // Finish websocket key
      Sha1_AddString(&handler->sha, WebSocketKeyPostfix);
      Sha1_Finish(&handler->sha);
      handler->state = newState;
    }

    handler->state = newState;
  }

  return 0;
}


int httpParserOnHeadersComplete(http_parser* parser)
{
  HttpEpollHandler* handler = PARSER_PTR_TO_HANDLER_PTR(parser);

  //LOG_DEBUG("http-parser: headers-complete");
  
  {
    int result = handleState0(handler, HTTP_PROCESS_STATE_BODY);
    if(result) {
      return result;
    }
  }

  return 0;
}
int httpParserOnHeaderField(http_parser* parser, const char* at, size_t length)
{
  HttpEpollHandler* handler = PARSER_PTR_TO_HANDLER_PTR(parser);

  //LOG_DEBUG("http-parser: header-field '%.*s'", (int)length, at);
  {
    int result = handleState0(handler, HTTP_PROCESS_STATE_HEADER_FIELD);
    if(result) {
      return result;
    }
  }

  if(handler->state != HTTP_PROCESS_STATE_HEADER_FIELD) {
    LOG_ERROR("at header field callback but current state is %d", handler->state);
    return 1;
  }

  HttpEpollHandlerAdd(handler, at, length);
  return 0;
}


int httpParserOnHeaderValue(http_parser* parser, const char* at, size_t length)
{
  HttpEpollHandler* handler = PARSER_PTR_TO_HANDLER_PTR(parser);

  //LOG_DEBUG("http-parser: header-value '%.*s'", (int)length, at);

  if(handler->state == HTTP_PROCESS_STATE_HEADER_FIELD) {
    LOG_DEBUG("HttpHeaderField '%.*s%s'", (int)handler->buf.savedContentLength, handler->buf.array,
	      handler->buf.droppedContentLength ? "..." : "");
    if(HttpEpollHandlerBufEquals(handler, "Sec-WebSocket-Key")) {
      LOG_DEBUG("Found WebSocket Key!");
      handler->state = HTTP_PROCESS_STATE_HEADER_VALUE_WEBSOCKET_KEY;
      more_Sha1_Init(&handler->sha);
    } else {
      handler->state = HTTP_PROCESS_STATE_HEADER_VALUE_GENERIC;
    }
    handler->buf.savedContentLength = 0; // Remove the header
    handler->buf.droppedContentLength = 0;
  }

  if(handler->state == HTTP_PROCESS_STATE_HEADER_VALUE_GENERIC) {
    HttpEpollHandlerAdd(handler, at, length);
    return 0;
  } else if(handler->state == HTTP_PROCESS_STATE_HEADER_VALUE_WEBSOCKET_KEY) {
    HttpEpollHandlerAdd(handler, at, length);
    Sha1_AddBinary(&handler->sha, at, length);
    return 0;
  } else {
    LOG_ERROR("at header value callback but current state is %d", handler->state);
    return 1;
  }
}
int httpParserOnBody(http_parser* parser, const char* at, size_t length)
{
  LOG_DEBUG("http-parser: body '%.*s'", (int)length, at);
  return 0;
}

int main(int argc, char* argv[])
{
  struct sockaddr_in addr;
  struct epoll_event epollEvent;
  struct epoll_event eventBuffer[EVENT_BUFFER_COUNT];

  epollfd = epoll_create1(0);
  if(epollfd == -1) {
    LOG_FATAL("epoll_create1 failed (e=%d)", errno);
    return 1;
  }

  {
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(HTTP_LISTEN_PORT);

    httpListenSock = socket(addr.sin_family, SOCK_STREAM, IPPROTO_TCP);
    if(httpListenSock == -1) {
      LOG_FATAL("socket function failed for http listener (e=%d)", errno);
      return 1;
    }
    {
      int enable = 1;
      if(setsockopt(httpListenSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
	LOG_FATAL("setsockopt for REUSEADDR failed (e=%d)", errno);
	return 1;
      }
    }
    if(bind(httpListenSock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      LOG_FATAL("bind failed for http listener (e=%d)", errno);
      return 1;
    }
    if(listen(httpListenSock, HTTP_BACKLOG) < 0) {
      LOG_FATAL("listen failed for http listener (e=%d)", errno);
      return 1;
    }
    
    // Initialize Http Parser Settings
    httpParserSettings.on_message_begin    = httpParserOnMessageBegin;
    httpParserSettings.on_headers_complete = httpParserOnHeadersComplete;
    httpParserSettings.on_message_complete = httpParserOnMessageComplete;
    httpParserSettings.on_url              = httpParserOnUrl;
    httpParserSettings.on_header_field     = httpParserOnHeaderField;
    httpParserSettings.on_header_value     = httpParserOnHeaderValue;
    httpParserSettings.on_body             = httpParserOnBody;

    // Add the socket to epoll
    epollEvent.events   = EPOLLIN;
    epollEvent.data.ptr = (void*)&httpAcceptHandlerPtr;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, httpListenSock, &epollEvent) == -1) {
      LOG_FATAL("epoll_ctl failed for http listener (e=%d)", errno);
      return 1;
    }
  }

  while(1) {
    int eventIndex;
    int eventCount = epoll_wait(epollfd, eventBuffer, EVENT_BUFFER_COUNT, -1);
    if(eventCount == -1) {
      LOG_FATAL("epoll_wait failed (e=%d)", errno);
      break;
    }

    for(eventIndex = 0; eventIndex < eventCount; eventIndex++) {
      EpollHandler* handlerPtr = (EpollHandler*)eventBuffer[eventIndex].data.ptr;
      // We assume the pointer is pointing to a structure
      // where the first field is EpollHandler
      (*handlerPtr)(handlerPtr);
    }
  }

  return 0;
}
