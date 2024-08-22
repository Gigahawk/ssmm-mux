#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

uint32_t get_u32_be(uint8_t *buf)
{
    return (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];
}

uint16_t get_u16_be(uint8_t *buf)
{
    return (buf[0]<<8) | buf[1];
}

void put_u32_le(uint8_t *buf, uint32_t n)
{
    buf[0] = (uint8_t)(n & 0xFF);
    buf[1] = (uint8_t)((n >> 8) & 0xFF);
    buf[2] = (uint8_t)((n >> 16) & 0xFF);
    buf[3] = (uint8_t)((n >> 24) & 0xFF);
}

void efopen(FILE **stream, const char *filename, const char *mode)
{
    *stream = fopen(filename, mode);
    if (*stream == NULL) {
        printf(
            "%s: could not open file for %s (%s)\n",
            filename, mode[0]=='r'?"reading":"writing", strerror(errno)
        );
        exit(EXIT_FAILURE);
    }
}

void efseek(FILE *stream, long int offset, int whence, char *name)
{
    if (0 != fseek(stream, offset, whence)) {
        printf(
            "%s: 0x%08lX: seek error (%ld, mode=%d) (%s)\n",
            name, ftell(stream), offset, whence, strerror(errno)
        );
        exit(EXIT_FAILURE);
    }
}

void efread(void *ptr, size_t bytes, FILE *stream, char *name)
{
    if (bytes != fread(ptr, 1, bytes, stream)) {
        printf(
            "%s: 0x%08lX: read error ",
            name, ftell(stream)
        );
        if (feof(stream)) {
            printf("(unexpected end of file) ");
        }
        printf("(%s)\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void efwrite(const void *ptr, size_t bytes, FILE *stream, char *name)
{
    if (bytes != fwrite(ptr, 1, bytes, stream)) {
        printf(
            "%s: 0x%08lX: write error: %s\n",
            name, ftell(stream), strerror(errno)
        );
        exit(EXIT_FAILURE);
    }
}

char *subext(char *path, const char *rep)
{
    //
    // substitutes extension of path with another string
    //
    int i, s=0;
    char *out;

    // scan from start up to basename
    for (i=0; path[i]; ++i)
    {
        if (path[i] == '/' || path[i] == '\\') {
            s = i + 1;
        }
    }

    // scan from basename up to extension
    for (i=s; path[i]; ++i)
    {
        if (path[i] == '.') {
            s = i;
        }
    }

    // no extension; get full path
    if (path[s] != '.') {
        s = i;
    }

    out = malloc(s + strlen(rep) + 1);

    if (out == NULL) {
        printf(
            "could not allocate memory for subext()\n"
            "arguments:\n0: %s\n1: %s\n", path, rep
        );
        exit(EXIT_FAILURE);
    }

    memcpy(out, path, s);

    strcpy(out + s, rep);

    return out;
}

const char *stream_names[] = {
    ".bin",
    ".m2v",
    ".ss2"
};

enum {
    STREAM_PRIVATE = 0,
    STREAM_VIDEO   = 1,
    STREAM_AUDIO   = 2
};

typedef struct {
    FILE *f;
    char *path;
} stream_t;

#define STREAMS_MAX ((sizeof stream_names) / (sizeof *stream_names))

//#define ZOE_SSID_SUBS_JP 0x00
//#define ZOE_SSID_ADPCM   0x01
//#define ZOE_SSID_BIN     0x05
//#define ZOE_SSID_SUBS_EN 0x07
//#define ZOE_SSID_SUBS_FR 0x08
//#define ZOE_SSID_SUBS_DE 0x09
//#define ZOE_SSID_SUBS_IT 0x0A

int main(int argc, char *argv[])
{
    FILE     *pss                 = NULL;
    char     *pss_path            = NULL;
    long int pss_size             = 0;
    stream_t streams[STREAMS_MAX] = {0};
    uint8_t  stream               = 0;
    uint16_t streambufsize        = 8192;
    uint8_t  *streambuf           = NULL;
    uint32_t id                   = 0;
    uint8_t pack_stuff_len        = 0;
    uint8_t pes_head_len          = 0;
    uint32_t data_len             = 0;
    uint32_t ssid                 = 0;
    uint16_t packet_size          = 0;
    uint16_t payload_offset       = 0;

    uint32_t file_idx             = 0;

    if (argc != 2) {
        printf("usage: %s <pss>\n", argv[0]);
        return 1;
    }

    pss_path = argv[1];

    efopen(&pss, pss_path, "rb");

    efseek(pss, 0, SEEK_END, pss_path);

    pss_size = ftell(pss);

    if (pss_size == EOF) {
        printf("%s: cannot determine file size (too large?)\n", pss_path);
        exit(EXIT_FAILURE);
    }

    efseek(pss, 0, SEEK_SET, pss_path);

    streambuf = malloc(streambufsize);

    if (streambuf == NULL) {
        printf("%s: could not allocate stream buffer\n", pss_path);
        exit(EXIT_FAILURE);
    }

    while (ftell(pss) < pss_size) {
        file_idx = ftell(pss);
        efread(streambuf, 4, pss, pss_path);
        id = get_u32_be(streambuf);
        if (id == 0x000001B9) {
            printf("Found program end\n");
            break;
        }
        efread(streambuf, 2, pss, pss_path);
        packet_size = get_u16_be(streambuf);
        if (id == 0x000001BA) {
            printf("Found pack header\n");
            // Seek past all the fixed length info
            efseek(pss, 7, SEEK_CUR, pss_path);
            efread(streambuf, 1, pss, pss_path);
            pack_stuff_len = streambuf[0] & 0b111;
            efseek(pss, pack_stuff_len, SEEK_CUR, pss_path);
        } else if (id == 0x000001BB) {
            printf("Found system header\n");
            efseek(pss, packet_size, SEEK_CUR, pss_path);
        } else if (id == 0x000001BE) {
            printf("Found padding header\n");
            efseek(pss, packet_size, SEEK_CUR, pss_path);
        } else {
            if (0x000001E0 <= id && id <= 0x000001EF) {
                printf("Found PES video header\n");
                stream = STREAM_VIDEO;
            } else if (id == 0x000001BD) {
                printf("Found PES private stream header\n");
                stream = STREAM_PRIVATE;
            } else {
                printf(
                    "Found unknown PES header id %08X at offset %08x\n",
                    id, file_idx
                );
                exit(-1);
            }
            efseek(pss, 2, SEEK_CUR, pss_path);
            efread(streambuf, 1, pss, pss_path);
            pes_head_len = streambuf[0];
            efseek(pss, pes_head_len, SEEK_CUR, pss_path);
            data_len = packet_size - pes_head_len - 3;
            efread(streambuf, data_len, pss, pss_path);
            payload_offset = 0;
            if (stream == STREAM_PRIVATE) {
                ssid = get_u32_be(streambuf);
                if (ssid == 0xFFA00000) {
                    printf("Private stream contains SS2 audio\n");
                    stream = STREAM_AUDIO;
                    payload_offset = 4;
                } else {
                    printf(
                        "Unknown SSID %08X on packet starting at offset %08X\n",
                        ssid, file_idx
                    );
                    exit(-1);
                }
            }
            if (streams[stream].f == NULL) {
                streams[stream].path = subext(pss_path, stream_names[stream]);
                efopen(&streams[stream].f, streams[stream].path, "w+b");
            }
            efwrite(
                streambuf + payload_offset, data_len - payload_offset,
                streams[stream].f, streams[stream].path
            );
        }
    }

    // clean up
    if (0 != fclose(pss)) {
        printf("%s: could not close PSS file\n", pss_path);
        exit(EXIT_FAILURE);
    }
    free(streambuf);
    for (stream=0; stream<STREAMS_MAX; ++stream) {
        if (streams[stream].f) {
            if (0 != fclose(streams[stream].f)) {
                printf(
                    "%s: could not close output file: %s\n",
                    pss_path, streams[stream].path
                );
                exit(EXIT_FAILURE);
            }
            free(streams[stream].path);
        }
    }

    return 0;
}
