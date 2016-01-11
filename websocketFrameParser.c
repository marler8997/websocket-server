
#include "websocketFrameParser.h"

/*
#define WSF_PARSE_STATE_INITIAL 0
#define WSF_PARSE_STATE_PAYLOAD0 1
#define WSF_PARSE_STATE_PAYLOAD1 1
#define WSF_PARSE_STATE_PAYLOAD2 1
#define WSF_PARSE_STATE_PAYLOAD3 1
#define WSF_PARSE_STATE_PAYLOAD4 1
#define WSF_PARSE_STATE_PAYLOAD5 1
#define WSF_PARSE_STATE_PAYLOAD6 1
#define WSF_PARSE_STATE_PAYLOAD7 1
#define WSF_PARSE_STATE_PAYLOAD7 1
#define WSF_PARSE_STATE_PAYLOAD7 1
*/



/*

// Assumption: limit > data AND (data points to valid data)
void payload_len_parser(WebSocketFrameParser* parser, ubyte* data, ubyte* limit)
{
}


// Assumption: limit > data AND (data points to valid data)
void initial_parser(WebSocketFrameParser* parser, ubyte* data, ubyte* limit)
{
  parser->flags = *data;
  data++;
  if(data >= limit) {
    //parser->parse = payload_len_parser;
    return;
  }

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  //
  // need some way to call payload_len_parser without having
  // to call another function!
  //
  // To solve this problem, I may need to have just one parse
  // function.  This way, the function arguments will be placed
  // on the function stack one time.  Then I can use some type of state
  // to jump to the right part of the function.
  // The question is, how do you jump to the right part of the function
  // without a bunch of comparisons?
  // A switch statement may be able to optimize it into a jump table, but
  // it would be nice if I could just explicitly use some sort of jump table.
  // If there was a way to assign a label to a variable, that could work! :)
}
*/


void websocket_frameparser_init(WebSocketFrameParser* parser)
{
  //parser->parse = &initial_parser;
}


#include <stdio.h>

// Assumption: limit > data
void websocket_parseframe(WebSocketFrameParser* parser, ubyte* data, ubyte* limit)
{
  switch(parser->parseState) {
  case 0:
    parser->flags = *data;
    printf("[ws-parser] OP=%d FIN=%d RSV1=%d RSV2=%d RSV3=%d\n",
	   parser->flags & 0x07,
	   parser->flags >> 7 & 1,
	   parser->flags >> 6 & 1,
	   parser->flags >> 5 & 1,
	   parser->flags >> 4 & 1);
    data++;
    if(data >= limit) {
      parser->parseState = 1;
      return;
    }
  case 1: {
    ubyte len;
    parser->encoded = (*data & 0x80) ? 1 : 0;
    printf("[ws-parser] ENCODED=%d\n", parser->encoded);
    len = *data & 0x7F;
    data++;

    if(len <= 125) {
      parser->payloadLength = len;
      printf("[ws-parser] Payload Length is %d (1 byte)\n", len);
      if(data >= limit) {
	parser->parseState = 12;
	return;
      }
      goto MASKING_KEY;
    }

    parser->payloadLength = 0;
    if(len == 126) {
      printf("[ws-parser] Payload Length will be 2 bytes\n");
      if(data >= limit) {
	parser->parseState = 2;
	return;
      }
      goto PAYLOAD_LEN_16_BITS;
    }

    // len == 127
    printf("[ws-parser] Payload Length will be 8 bytes\n");
    if(data >= limit) {
      parser->parseState = 4;
      return;
    }
    goto PAYLOAD_LEN_64_BITS;
  }

  PAYLOAD_LEN_16_BITS:
  case 2: // first byte of 16 bit payload len
    printf("[ws-parser] case 2 (PAYLOAD_LEN_16_BITS)\n");

  case 3:

    goto MASKING_KEY;
    
  PAYLOAD_LEN_64_BITS:
  case 4:
    printf("[ws-parser] case 4 (PAYLOAD_LEN_64_BITS)\n");

  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
  case 10:
  case 11:

  MASKING_KEY:
  case 12:
    printf("[ws-parser] case 12 (MASKING_KEY)\n");
    parser->maskingKey = *data << 24;
    data++;
    if(data >= limit) {
      parser->parseState = 13;
      return;
    }
  case 13:
    parser->maskingKey |= *data << 16;
    data++;
    if(data >= limit) {
      parser->parseState = 14;
      return;
    }
  case 14:
    parser->maskingKey |= *data << 8;
    data++;
    if(data >= limit) {
      parser->parseState = 15;
      return;
    }
  case 15:
    parser->maskingKey |= *data;
    data++;
    if(data >= limit) {
      parser->parseState = 16;
      return;
    }

  PAYLOAD_DATA:
  case 16:
    printf("[ws-parser] case 16 (PAYLOAD_DATA) %d byte(s)\n", limit-data);
    

  }
}
