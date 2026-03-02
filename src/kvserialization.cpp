#include "kvserialization.h"

#include <stdlib.h>

void kv_encode_uint64(std::string & buffer, uint64_t value)
{
    char valuestr[10];
    int len = 0;
    while (1) {
        unsigned char remainder = value & 0x7f;
        value = value >> 7;
        if (value == 0) {
            // last item to write.
            valuestr[len] = remainder;
            len ++;
            break;
        }
        else {
            valuestr[len] = remainder | 0x80;
            len ++;
        }
    }
    buffer.append(valuestr, len);
}

size_t kv_decode_uint64(std::string & buffer, size_t position, uint64_t * p_value)
{
    if (position >= buffer.size()) {
        return (size_t) -1;
    }

    uint64_t value = 0;
    int s = 0;
    size_t consumed = 0;

    while (1) {
        if (position >= buffer.size()) {
            return (size_t) -1;
        }
        unsigned char remainder = buffer[position];
        position ++;
        consumed ++;
        uint64_t chunk = (uint64_t) (remainder & 0x7f);
        if (s >= 64 && chunk != 0) {
            return (size_t) -1;
        }
        if (s < 64) {
            value += chunk << s;
        }
        if ((remainder & 0x80) == 0) {
            break;
        }
        if (consumed >= 10) {
            return (size_t) -1;
        }
        s += 7;
    }

    * p_value = value;

    return position;
}
