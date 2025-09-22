#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BUFFER_READ_CHECKS 1
#define BUFFER_WRITE_CHECKS 1

typedef struct BufferReader {
    size_t size;
    size_t index;
    const uint8_t *data;
} BufferReader;

typedef struct BufferWriter {
    size_t size;
    size_t index;
    uint8_t *data;
} BufferWriter;

bool buffer_read(BufferReader *reader, size_t length, void *dest);

bool buffer_read_uint8(BufferReader *reader, uint8_t *value);
bool buffer_read_uint16(BufferReader *reader, uint16_t *value);
bool buffer_read_uint32(BufferReader *reader, uint32_t *value);
bool buffer_read_uint64(BufferReader *reader, uint64_t *value);

bool buffer_read_uint8_array(BufferReader *reader, size_t length, uint8_t *dest);

bool buffer_read_char(BufferReader *reader, char *value);
bool buffer_read_str8_view(BufferReader *reader, const char **value, uint8_t *length);
bool buffer_read_str8(BufferReader *reader, char *value, uint8_t *length, uint8_t max_size);
bool buffer_read_str16_view(BufferReader *reader, const char **value, uint16_t *length);
bool buffer_read_str16(BufferReader *reader, char *value, uint16_t *length, uint16_t max_size);

bool buffer_write(BufferWriter *writer, size_t length, const void *src);

bool buffer_write_uint8(BufferWriter *writer, uint8_t value);
bool buffer_write_uint16(BufferWriter *writer, uint16_t value);
bool buffer_write_uint32(BufferWriter *writer, uint32_t value);
bool buffer_write_uint64(BufferWriter *writer, uint64_t value);

bool buffer_write_uint8_array(BufferWriter *writer, size_t length, const uint8_t *src);

bool buffer_write_char(BufferWriter *writer, char value);
bool buffer_write_str8(BufferWriter *writer, const char *value);
bool buffer_write_str16(BufferWriter *writer, const char *value);
