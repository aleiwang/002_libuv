#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ringbuf.h"

void create_ringBuffer(ringbuffer_t *ringBuf, uint8_t *buf, uint32_t buf_len)
{
    ringBuf->br         = 0;
    ringBuf->bw         = 0;
    ringBuf->btoRead    = 0;
    ringBuf->source     = buf;
    ringBuf->length     = buf_len;
    printf("create ringBuffer->length = %d\n", ringBuf->length);
}

void clear_ringBuffer(ringbuffer_t *ringBuf)
{
    ringBuf->br         = 0;
    ringBuf->bw         = 0;
    ringBuf->btoRead    = 0;
}

uint32_t write_ringBuffer(uint8_t *buffer, uint32_t size, ringbuffer_t *ringBuf)
{
    uint32_t len            = 0;
    uint32_t ringBuf_bw     = ringBuf->bw;
    uint32_t ringBuf_len    = ringBuf->length;
    uint8_t *ringBuf_source = ringBuf->source;
    
    if( (ringBuf_bw + size) <= ringBuf_len  )
    {
        memcpy(ringBuf_source + ringBuf_bw, buffer, size);
    }
    else
    {
        len = ringBuf_len - ringBuf_bw;
        memcpy(ringBuf_source + ringBuf_bw, buffer, len);
        memcpy(ringBuf_source, buffer + len, size - len);
    }

    ringBuf->bw = (ringBuf->bw + size) % ringBuf_len;
    ringBuf->btoRead += size;

    return size;
}

uint32_t peek_ringBuffer(uint8_t *buffer, uint32_t size, ringbuffer_t *ringBuf)
{
    uint32_t len            = 0;
    uint32_t ringBuf_br     = ringBuf->br;
    uint32_t ringBuf_len    = ringBuf->length;
    uint8_t *ringBuf_source = ringBuf->source;

    if( (ringBuf_br + size ) <= ringBuf_len )
    {
        memcpy(buffer, ringBuf_source + ringBuf_br, size);
    }
    else
    {
        len = ringBuf_len - ringBuf_br;
        memcpy(buffer, ringBuf_source + ringBuf_br, len);
        memcpy(buffer + len, ringBuf_source, size - len);
    }

    return size;
}


uint32_t read_ringBuffer(uint8_t *buffer, uint32_t size, ringbuffer_t *ringBuf)
{
    uint32_t sizePeek = peek_ringBuffer(buffer, size, ringBuf); 
    ringBuf->br = (ringBuf->br + sizePeek) % ringBuf->length;
    ringBuf->btoRead -= sizePeek;
    return sizePeek;
}


uint32_t get_ringBuffer_btoRead(ringbuffer_t *ringBuf)
{
    return ringBuf->btoRead;
}

uint32_t get_ringBuffer_length(ringbuffer_t *ringBuf)
{
    return ringBuf->length;
}

uint32_t get_ringBuffer_bcanWrite(ringbuffer_t *ringBuf)
{
    return ringBuf->length - ringBuf->btoRead;
}
