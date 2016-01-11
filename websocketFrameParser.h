#pragma once

#include <stdint.h>

#define using_more
#include <more/common.h>


typedef struct _WebSocketFrameParser WebSocketFrameParser;


typedef void (*wsframe_parser)(WebSocketFrameParser* parser, ubyte* data, ubyte* limit);

struct _WebSocketFrameParser {
  uint64_t payloadLength;
  uint32_t maskingKey;
  ubyte parseState;
  ubyte flags;
  ubyte encoded;
};

void websocket_parseframe(WebSocketFrameParser* parser, ubyte* data, ubyte* limit);

