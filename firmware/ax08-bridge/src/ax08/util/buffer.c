#include "buffer.h"

#include <string.h>

bool buffer_read(BufferReader *reader, const size_t length, void *dest) {
    if (BUFFER_READ_CHECKS) {
        if (reader->index + length > reader->size) {
            return false;
        }
    }

    memcpy(dest, reader->data + reader->index, length);
    reader->index += length;
    return true;
}

bool buffer_read_uint8(BufferReader *reader, uint8_t *value) {
    if (BUFFER_READ_CHECKS) {
        if (reader->index > (reader->size - 1)) {
            return false;
        }
    }

    *value = reader->data[reader->index];
    reader->index += 1;
    return true;
}

bool buffer_read_uint16(BufferReader *reader, uint16_t *value) {
    if (BUFFER_READ_CHECKS) {
        if (reader->index > (reader->size - 2)) {
            return false;
        }
    }

    *value = *((uint16_t*)(reader->data + reader->index));
    reader->index += 2;
    return true;
}

bool buffer_read_uint32(BufferReader *reader, uint32_t *value) {
    if (BUFFER_READ_CHECKS) {
        if (reader->index > (reader->size - 4)) {
            return false;
        }
    }

    *value = *((uint32_t*)(reader->data + reader->index));
    reader->index += 4;
    return true;
}

bool buffer_read_uint64(BufferReader *reader, uint64_t *value) {
    if (BUFFER_READ_CHECKS) {
        if (reader->index > (reader->size - 8)) {
            return false;
        }
    }

    *value = *((uint64_t*)(reader->data + reader->index));
    reader->index += 8;
    return true;
}

bool buffer_read_uint8_array(BufferReader *reader, const size_t length, uint8_t *dest) {
    return buffer_read(reader, length * 1, (void*)dest);
}

bool buffer_read_char(BufferReader *reader, char *value) {
    if (BUFFER_READ_CHECKS) {
        if (reader->index > (reader->size - 1)) {
            return false;
        }
    }

    *value = *((char*)(reader->data + reader->index));
    reader->index += 1;
    return true;
}

bool buffer_read_str8_view(BufferReader *reader, const char **value, uint8_t *length) {
    size_t start_index = reader->index;

    // Read the length of the string.
    uint8_t str_length;
    if (!buffer_read_uint8(reader, &str_length)) {
        return false;
    }

    // Check if size fits into the buffer.
    if (BUFFER_READ_CHECKS) {
        if (reader->index > (reader->size - str_length)) {
            reader->index = start_index;
            return false;
        }
    }
    if (value != NULL) {
        *value = reader->data + reader->index;
    }
    reader->index += str_length;

    // Set the length.
    if (length != NULL) {
        *length = str_length;
    }

    // Return success.
    return true;
}

bool buffer_read_str8(BufferReader *reader, char *value, uint8_t *length, const uint8_t max_size) {
    // Get the string view.
    uint8_t str_length;
    const char *str_data;
    if (!buffer_read_str8_view(reader, &str_data, &str_length)) {
        return false;
    }

    // Copy the string.
    memcpy(value, str_data, str_length);
    value[str_length] = 0;

    // Set the length.
    if (length != NULL) {
        *length = str_length;
    }

    // Return success.
    return true;
}

bool buffer_read_str16_view(BufferReader *reader, const char **value, uint16_t *length) {
    size_t start_index = reader->index;

    // Read the length of the string.
    uint16_t str_length;
    if (!buffer_read_uint16(reader, &str_length)) {
        return false;
    }

    // Check if size fits into the buffer.
    if (BUFFER_READ_CHECKS) {
        if (reader->index > (reader->size - str_length)) {
            reader->index = start_index;
            return false;
        }
    }
    if (value != NULL) {
        *value = reader->data + reader->index;
    }
    reader->index += str_length;

    // Set the length.
    if (length != NULL) {
        *length = str_length;
    }

    // Return success.
    return true;
}

bool buffer_read_str16(BufferReader *reader, char *value, uint16_t *length, const uint16_t max_size) {
    // Get the string view.
    uint16_t str_length;
    const char *str_data;
    if (!buffer_read_str16_view(reader, &str_data, &str_length)) {
        return false;
    }

    // Copy the string.
    memcpy(value, str_data, str_length);
    value[str_length] = 0;

    // Set the length.
    if (length != NULL) {
        *length = str_length;
    }

    // Return success.
    return true;
}


bool buffer_write(BufferWriter *writer, size_t length, const void *src) {
    if (BUFFER_READ_CHECKS) {
        if (writer->index + length > writer->size) {
            return false;
        }
    }

    memcpy(writer->data + writer->index, src, length);
    writer->index += length;
    return true;
}


bool buffer_write_uint8(BufferWriter *writer, const uint8_t value) {
    if (BUFFER_WRITE_CHECKS) {
        if (writer->index > (writer->size - 1)) {
            return false;
        }
    }

    writer->data[writer->index] = value;
    writer->index += 1;
    return true;
}

bool buffer_write_uint16(BufferWriter *writer, const uint16_t value) {
    if (BUFFER_WRITE_CHECKS) {
        if (writer->index > (writer->size - 2)) {
            return false;
        }
    }

    *((uint16_t*)(writer->data + writer->index)) = value;
    writer->index += 2;
    return true;
}

bool buffer_write_uint32(BufferWriter *writer, const uint32_t value) {
    if (BUFFER_WRITE_CHECKS) {
        if (writer->index > (writer->size - 4)) {
            return false;
        }
    }

    *((uint32_t*)(writer->data + writer->index)) = value;
    writer->index += 4;
    return true;
}

bool buffer_write_uint64(BufferWriter *writer, const uint64_t value) {
    if (BUFFER_WRITE_CHECKS) {
        if (writer->index > (writer->size - 8)) {
            return false;
        }
    }

    *((uint64_t*)(writer->data + writer->index)) = value;
    writer->index += 8;
    return true;
}

bool buffer_write_uint8_array(BufferWriter *writer, size_t length, const uint8_t *src) {
    return buffer_write(writer, length * 1, (const void*)src);
}

bool buffer_write_char(BufferWriter *writer, char value) {
    if (BUFFER_WRITE_CHECKS) {
        if (writer->index > (writer->size - 1)) {
            return false;
        }
    }

    *((char*)(writer->data + writer->index)) = value;
    writer->index += 1;
    return true;
}

bool buffer_write_str8(BufferWriter *writer, const char *value) {
    size_t start_index = writer->index;

    // Write the string length.
    uint8_t str_length = strlen(value);
    if (!buffer_write_uint8(writer, str_length)) {
        return false;
    }

    // Check if the string data fits into the buffer.
    if (BUFFER_READ_CHECKS) {
        if (writer->index > (writer->size - str_length)) {
            writer->index = start_index;
            return false;
        }
    }

    // Copy the data into the buffer.
    memcpy(writer->data + writer->index, value, str_length);
    writer->index += str_length;

    // Return success.
    return true;
}

bool buffer_write_str16(BufferWriter *writer, const char *value) {
    size_t start_index = writer->index;

    // Write the string length.
    uint16_t str_length = strlen(value);
    if (!buffer_write_uint16(writer, str_length)) {
        return false;
    }

    // Check if the string data fits into the buffer.
    if (BUFFER_READ_CHECKS) {
        if (writer->index > (writer->size - str_length)) {
            writer->index = start_index;
            return false;
        }
    }

    // Copy the data into the buffer.
    memcpy(writer->data + writer->index, value, str_length);
    writer->index += str_length;

    // Return success.
    return true;
}
