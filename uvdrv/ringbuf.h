#ifndef RINGBUFFER_H_
#define RINGBUFFER_H_

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

typedef struct {
    uint8_t *source;
    uint32_t br;
    uint32_t bw;
    uint32_t btoRead;
    uint32_t length;
}ringbuffer_t;

#ifdef __cplusplus
extern "C" {
#endif
void create_ringBuffer(ringbuffer_t *ringBuf, uint8_t *buf, uint32_t buf_len);
void clear_ringBuffer(ringbuffer_t *ringBuf);
uint32_t write_ringBuffer(uint8_t *buffer, uint32_t size, ringbuffer_t *ringBuf);
uint32_t read_ringBuffer(uint8_t *buffer, uint32_t size, ringbuffer_t *ringBuf);
uint32_t peek_ringBuffer(uint8_t *buffer, uint32_t size, ringbuffer_t *ringBuf);
uint32_t get_ringBuffer_btoRead(ringbuffer_t *ringBuf);
uint32_t get_ringBuffer_length(ringbuffer_t *ringBuf);
uint32_t get_ringBuffer_bcanWrite(ringbuffer_t *ringBuf);
#ifdef __cplusplus
}
#endif

#endif /* RINGBUFFER_H_ */