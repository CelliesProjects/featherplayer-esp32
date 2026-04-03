#ifndef _NORMALIZE_UTF8_
#define _NORMALIZE_UTF8_

size_t normalize_to_utf8(const char* in, size_t in_len,
                         char* out, size_t out_size)
{
    size_t i = 0;
    size_t o = 0;

    while (i < in_len)
    {
        uint8_t c = (uint8_t)in[i];

        if (c == 0)
            break;

        // ASCII
        if (c < 0x80)
        {
            if (o + 1 >= out_size) break;
            out[o++] = c;
            i++;
            continue;
        }

        // --- UTF-8 detection ---

        // 2-byte UTF-8
        if ((c & 0xE0) == 0xC0)
        {
            if (i + 1 < in_len)
            {
                uint8_t c2 = (uint8_t)in[i + 1];
                if ((c2 & 0xC0) == 0x80)
                {
                    if (o + 2 >= out_size) break;
                    out[o++] = c;
                    out[o++] = c2;
                    i += 2;
                    continue;
                }
            }
            // invalid → fallthrough
        }

        // 3-byte UTF-8
        if ((c & 0xF0) == 0xE0)
        {
            if (i + 2 < in_len)
            {
                uint8_t c2 = (uint8_t)in[i + 1];
                uint8_t c3 = (uint8_t)in[i + 2];

                // BOM check (EF BB BF)
                if (c == 0xEF && c2 == 0xBB && c3 == 0xBF)
                {
                    i += 3;
                    continue; // skip BOM
                }

                if ((c2 & 0xC0) == 0x80 &&
                    (c3 & 0xC0) == 0x80)
                {
                    if (o + 3 >= out_size) break;
                    out[o++] = c;
                    out[o++] = c2;
                    out[o++] = c3;
                    i += 3;
                    continue;
                }
            }
            // invalid → fallthrough
        }

        // 4-byte UTF-8
        if ((c & 0xF8) == 0xF0)
        {
            if (i + 3 < in_len)
            {
                uint8_t c2 = (uint8_t)in[i + 1];
                uint8_t c3 = (uint8_t)in[i + 2];
                uint8_t c4 = (uint8_t)in[i + 3];

                if ((c2 & 0xC0) == 0x80 &&
                    (c3 & 0xC0) == 0x80 &&
                    (c4 & 0xC0) == 0x80)
                {
                    if (o + 4 >= out_size) break;
                    out[o++] = c;
                    out[o++] = c2;
                    out[o++] = c3;
                    out[o++] = c4;
                    i += 4;
                    continue;
                }
            }
            // invalid → fallthrough
        }

        // --- fallback: treat as Latin-1 ---

        if (o + 2 >= out_size) break;

        out[o++] = 0xC0 | (c >> 6);
        out[o++] = 0x80 | (c & 0x3F);
        i++;
    }

    // null terminate
    if (o < out_size)
        out[o] = '\0';
    else
        out[out_size - 1] = '\0';

    return o;
}

#endif
