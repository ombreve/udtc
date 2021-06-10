/* Code and decode utf8 buffers.
 * Decoding code is from Bjoern Hoehrmann.
 * (see http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details)
 */

#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>

#ifndef UTF8_API
#define UTF8_API
#endif

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

/* Decode a UTF8 sequence BYTE to a codepoint pointed by CODEP.
 * Return UTF8_ACCEPT if the sequence is fully decoded in codepoint,
 * or UTF8_REJECT on error (invalid byte sequence, non-canonical encoding,
 * or a surrogate half. */
UTF8_API
uint32_t utf8_decode(uint32_t *state, uint32_t* codep, uint32_t byte);

/* Code a codepoint CODE as UTF8 sequence in SEQ.
 * Return a pointer to the next unused slot in SEQ.
 * Set STATE to UTF8_ACCEPT or UTF8_REJECT on error.
 * WARNNING: the caller must ensure that SEQ table has at least 4 bytes free!
 */
UTF8_API
uint8_t *utf8_code(uint32_t *state, uint8_t *seq, uint32_t code);

#ifdef UTF8_IMPLEMENTATION

static const uint8_t utf8d[] = {
    /* The first part of the table maps bytes to character classes
     * to reduce the size of the transition table and create bitmasks. */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
   10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

    /* The second part is a transition table that maps a combination
     * of a state of the automaton and a character class to a state. */
    0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
   12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
   12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
   12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
   12,36,12,12,12,12,12,12,12,12,12,12,
};

UTF8_API uint32_t
utf8_decode(uint32_t *state, uint32_t *codep, uint32_t byte)
{
    uint32_t type = utf8d[byte];

    *codep = (*state != UTF8_ACCEPT) ?
        (byte & 0x3fu) | (*codep << 6) :
        (0xff >> type) & (byte);
    *state = utf8d[256 + *state + type];
    return *state;
}

UTF8_API uint8_t *
utf8_code(uint32_t *state, uint8_t *seq, uint32_t code)
{
    if (code >= 0X110000U
        || (code >= 0xD800U && code <= 0xDFFFU)) {
        *state = UTF8_REJECT;
        return seq;
    }

    *state = UTF8_ACCEPT;
    if (code >= (1L << 16)) {
        seq[0] = 0xf0 |  (code >> 18);
        seq[1] = 0x80 | ((code >> 12) & 0x3f);
        seq[2] = 0x80 | ((code >>  6) & 0x3f);
        seq[3] = 0x80 | ((code >>  0) & 0x3f);
        return seq + 4;
    } else if (code >= (1L << 11)) {
        seq[0] = 0xe0 |  (code >> 12);
        seq[1] = 0x80 | ((code >>  6) & 0x3f);
        seq[2] = 0x80 | ((code >>  0) & 0x3f);
        return seq + 3;
    } else if (code >= (1L << 7)) {
        seq[0] = 0xc0 |  (code >>  6);
        seq[1] = 0x80 | ((code >>  0) & 0x3f);
        return seq + 2;
    } else {
        seq[0] = code;
        return seq + 1;
    }
    /* not reached */
}

#endif /* UTF8_IMPLEMENTATION */
#endif /* UTF8_H */
