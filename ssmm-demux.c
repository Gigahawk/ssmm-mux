#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef DEBUG
#define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DBG_PRINTF(...)
#endif


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

void parse_pack_header(uint8_t * buf) {
    uint32_t scr = 0;
    uint16_t scr_ext = 0;
    uint32_t mux_rate = 0;

    // SCR bits [32,30]
    scr |= ((buf[0] & 0b00111000) >> 3) << 30;
    // SCR bits [29,28]
    scr |= (buf[0] & 0b00000011) << 28;
    // SCR bits [27,20]
    scr |= buf[1] << 20;
    // SCR bits [19,15]
    scr |= ((buf[2] & 0b11111000) >> 3) << 15;
    // SCR bits [14,13]
    scr |= (buf[2] & 0b00000011) << 13;
    // SCR bits [12,5]
    scr |= buf[3] << 5;
    // SCR bits [4,0]
    scr |= (buf[4] & 0b11111000) >> 3;

    // SCR_ext bits [8,7]
    scr_ext |= (buf[4] & 0b00000011) << 7;
    // SCR_ext bits [6,0]
    scr_ext |= (buf[5] & 0b11111110) >> 1;

    // Program Mux Rate bits [21,14]
    mux_rate |= buf[6] << 14;
    // Program Mux Rate bits [13,6]
    mux_rate |= buf[7] << 6;
    // Program Mux Rate bits [5,0]
    mux_rate |= (buf[8] & 0b11111100) >> 2;
    DBG_PRINTF(
        "SCR: %u, SCR_ext: %u, Mux rate: %u\n",
        scr, scr_ext, mux_rate
    );
}

void parse_system_header(uint8_t * buf, uint16_t header_len) {
    uint32_t rate_bound = 0;
    uint8_t audio_bound = 0;
    uint8_t fixed_flag;
    uint8_t csps_flag;
    uint8_t system_audio_lock_flag;
    uint8_t system_video_lock_flag;
    uint8_t video_bound = 0;
    uint8_t packet_rate_restriction_flag;

    uint16_t streams_start = 6;
    uint16_t streams;
    uint8_t idx;
    uint8_t p_std_buffer_bound_scale;
    uint16_t p_std_buffer_size_bound;



    // Rate bound bits [21,15]
    rate_bound |= (buf[0] & 0b01111111) << 15;
    // Rate bound bits [14,7]
    rate_bound |= buf[1] << 7;
    // Rate bound bits [6,0]
    rate_bound |= (buf[2] & 0b11111110) >> 1;

    audio_bound |= (buf[3] & 0b11111100) >> 2;

    fixed_flag = (buf[3] & 0b00000010) ? 1 : 0;
    csps_flag = (buf[3] & 0b00000001) ? 1 : 0;
    system_audio_lock_flag = (buf[4] & 0b10000000) ? 1 : 0;
    system_video_lock_flag = (buf[4] & 0b01000000) ? 1 : 0;

    video_bound |= buf[4] & 0b00011111;

    packet_rate_restriction_flag = (buf[5] & 0b10000000) ? 1 : 0;

    // Seems like there's only 1 stream id 0xE0 (0b11100000)
    // No support for 24bit long structure (id 0b10110111)
    streams = (header_len - streams_start) / 2;



    DBG_PRINTF(
        "rate_bound: %u, audio_bound: %u, fixed_flag: %u, csps_flag: %d, "
        "system_audio_lock_flag: %d, system_video_lock_flag: %d, video_bound: %d, "
        "packet_rate_restriction_flag: %d, streams: %d\n",
        rate_bound, audio_bound, fixed_flag, csps_flag, system_audio_lock_flag,
        system_video_lock_flag, video_bound, packet_rate_restriction_flag,
        streams
    );
    for (uint8_t stream = 0; stream < streams; stream++) {
        idx = streams_start + (stream*2);
        p_std_buffer_bound_scale = (buf[idx] & 0b00100000) ? 1 : 0;

        p_std_buffer_size_bound = 0;
        // P-STD_buffer_bound_scale bits [12,8]
        p_std_buffer_size_bound |= (buf[idx] & 0b00011111) << 8;
        // P-STD_buffer_bound_scale bits [7,0]
        p_std_buffer_size_bound |= buf[idx + 1];
        DBG_PRINTF(
            "Stream: %d, P-STD_buffer_bound_scale: %d, P-STD_buffer_size_bound: %d\n",
            stream, p_std_buffer_bound_scale, p_std_buffer_size_bound
        );
    }
}

void parse_pes_ext_header(uint8_t * buf) {
    uint8_t pes_scrambling_control = 0;
    uint8_t pes_priority;
    uint8_t data_alignment_indicator;
    uint8_t copyright;
    uint8_t original;
    uint8_t pts_dts_flags = 0;
    uint8_t escr_flag;
    uint8_t es_rate_flag;
    uint8_t dsm_trick_mode_flag;
    uint8_t additional_copy_info_flag;
    uint8_t pes_crc_flag;
    uint8_t pes_extension_flag;
    uint8_t pes_header_length;
    uint8_t idx = 3;

    uint8_t pts_magic = 0;
    uint8_t dts_magic = 0;
    uint64_t pts = 0;
    uint64_t dts = 0;

    uint8_t pes_private_data_flag = 0;
    uint8_t pack_header_field_flag = 0;
    uint8_t program_packet_sequence_counter_flag = 0;
    uint8_t p_std_buffer_flag = 0;
    // Note PS2 doesn't support PES extension flag 2

    uint16_t pes_private_data;

    uint8_t pack_field_length;

    uint8_t packet_sequence_counter = 0;
    uint8_t is_mpeg2;
    uint8_t original_stuffing_length;


    uint8_t p_std_buffer_scale;
    uint8_t p_std_buffer_size;

    pes_scrambling_control = (buf[0] & 0b00110000) >> 4;
    pes_priority = (buf[0] & 0b00001000) ? 1 : 0;
    data_alignment_indicator = (buf[0] & 0b00000100) ? 1 : 0;
    copyright = (buf[0] & 0b00000010) ? 1 : 0;
    original = (buf[0] & 0b00000001) ? 1 : 0;

    pts_dts_flags = (buf[1] & 0b11000000) >> 6;
    escr_flag = (buf[1] & 0b00100000) ? 1 : 0;
    es_rate_flag = (buf[1] & 0b00010000) ? 1 : 0;
    dsm_trick_mode_flag = (buf[1] & 0b00001000) ? 1 : 0;
    additional_copy_info_flag = (buf[1] & 0b00000100) ? 1 : 0;
    pes_crc_flag = (buf[1] & 0b00000010) ? 1 : 0;
    pes_extension_flag = (buf[1] & 0b00000001) ? 1 : 0;

    pes_header_length = buf[2];

    DBG_PRINTF(
        "PES scrambling control: %d, PES priority: %d, data alignment indicator: %d, "
        "copyright: %d, original: %d, PTS DTS flags: %d, ESCR flag: %d, "
        "ES rate flag: %d, DSM trick mode flag: %d, additional copy info flag: %d, "
        "PES CRC flag: %d, PES extension flag: %d, PES header length: %d\n",
        pes_scrambling_control, pes_priority, data_alignment_indicator, copyright,
        original, pts_dts_flags, escr_flag, es_rate_flag, dsm_trick_mode_flag,
        additional_copy_info_flag, pes_crc_flag, pes_extension_flag, pes_header_length
    );

    if (pts_dts_flags & 0b10) {
        pts_magic |= (buf[idx] & 0b11110000) >> 4;
        // PTS bits [32,30]
        pts |= ((buf[idx++] & 0b00001110) >> 1) << 30;
        // PTS bits [29,22]
        pts |= buf[idx++] << 22;
        // PTS bits [21,15]
        pts |= ((buf[idx++] & 0b11111110) >> 1) << 15;
        // PTS bits [14,7]
        pts |= buf[idx++] << 7;
        // PTS bits [6,0]
        pts |= (buf[idx++] & 0b11111110) >> 1;
        DBG_PRINTF("PTS: %lu, ", pts);
    }
    if (pts_dts_flags & 0b01) {
        dts_magic |= (buf[idx] & 0b11110000) >> 4;
        // DTS bits [32,30]
        dts |= ((buf[idx++] & 0b00001110) >> 1) << 30;
        // DTS bits [29,22]
        dts |= buf[idx++] << 22;
        // DTS bits [21,15]
        dts |= ((buf[idx++] & 0b11111110) >> 1) << 15;
        // DTS bits [14,7]
        dts |= buf[idx++] << 7;
        // DTS bits [6,0]
        dts |= (buf[idx++] & 0b11111110) >> 1;
        DBG_PRINTF("DTS: %lu, ", dts);
    }
    if (pts_dts_flags == 0b10) {
        if (pts_magic != 0b0010) {
            DBG_PRINTF("Error: invalid pts magic %d\n", pts_magic);
            exit(-1);
        }
    } else if (pts_dts_flags == 0b11) {
        if (pts_magic != 0b0011) {
            DBG_PRINTF("error: invalid pts magic %d\n", pts_magic);
            exit(-1);
        }
        if (dts_magic != 0b0001) {
            DBG_PRINTF("error: invalid dts  magic %d\n", dts_magic);
            exit(-1);
        }
    } else if (pts_dts_flags == 0b01) {
        DBG_PRINTF("Invalid PTS DTS flags\n");
        exit(-1);
    }
    DBG_PRINTF("\n");

    // TODO: parse these
    if (escr_flag) {
        DBG_PRINTF("ESCR FLAG ON\n");
        exit(-1);
        idx += 6;
    }
    if (es_rate_flag) {
        DBG_PRINTF("ES RATE FLAG ON\n");
        exit(-1);
        idx += 3;
    }
    if (additional_copy_info_flag) {
        DBG_PRINTF("ADDITIONAL COPY INFO FLAG ON\n");
        exit(-1);
        idx += 1;
    }
    if (pes_crc_flag) {
        DBG_PRINTF("CRC FLAG ON\n");
        exit(-1);
        idx += 2;
    }

    if (pes_extension_flag) {
        pes_private_data_flag = (buf[idx] & 0b10000000) ? 1 : 0;
        pack_header_field_flag = (buf[idx] & 0b01000000) ? 1 : 0;
        program_packet_sequence_counter_flag = (buf[idx] & 0b00100000) ? 1 : 0;
        p_std_buffer_flag = (buf[idx] & 0b00010000) ? 1 : 0;
        idx++;
        DBG_PRINTF(
            "PES EXTENSTION: 1, PES private data flag: %d, "
            "pack header field flag: %d, program packet sequence counter flag: %d, "
            "p_std_buffer_flag: %d\n",
            pes_private_data_flag, pack_header_field_flag,
            program_packet_sequence_counter_flag, p_std_buffer_flag
        );
    }
    if (pes_private_data_flag) {
        pes_private_data |= buf[idx++] << 8;
        pes_private_data |= buf[idx++];
        DBG_PRINTF("PES Private data: %04X\n", pes_private_data);
    }
    if (pack_header_field_flag) {
        pack_field_length = buf[idx++];
        DBG_PRINTF("Pack field length: %d\n", pes_private_data);
    }
    if (program_packet_sequence_counter_flag) {
        packet_sequence_counter |= (buf[idx++] & 0b01111111) << 1;
        packet_sequence_counter |= (buf[idx] & 0b10000000) >> 7;
        is_mpeg2 = (buf[idx] & 0b01000000) ? 1 : 0;
        original_stuffing_length = buf[idx++] & 0b00111111;
        DBG_PRINTF(
            "packet sequence counter: %d, is mpeg2: %d, "
            "original_stuffing_length: %d\n",
            packet_sequence_counter, is_mpeg2, original_stuffing_length
        );
    }

    if (p_std_buffer_flag) {
        p_std_buffer_scale = (buf[idx] & 0b00100000) ? 1 : 0;
        // P-STD buffer size bits [12, 8]
        p_std_buffer_size |= (buf[idx] & 0b00011111) << 8;
        // P-STD buffer size bits [7, 0]
        p_std_buffer_size |= buf[idx];
        DBG_PRINTF(
            "P-STD buffer scale: %d, P-STD buffer size: %d\n",
            p_std_buffer_scale, p_std_buffer_size
        );
    }

}

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
            DBG_PRINTF("Found program end\n");
            break;
        }
        efread(streambuf, 2, pss, pss_path);
        packet_size = get_u16_be(streambuf);
        if (id == 0x000001BA) {
            DBG_PRINTF("Found pack header at index %08X\n", file_idx);
            // First two bytes of pack header isn't packet size,
            // don't overwrite before passing to parser
            efread(streambuf + 2, 7, pss, pss_path);
#ifdef DEBUG
            parse_pack_header(streambuf);
#endif
            efread(streambuf, 1, pss, pss_path);
            pack_stuff_len = streambuf[0] & 0b111;
            efseek(pss, pack_stuff_len, SEEK_CUR, pss_path);
        } else if (id == 0x000001BB) {
            DBG_PRINTF("Found system header at index %08X\n", file_idx);
            efread(streambuf, packet_size, pss, pss_path);
#ifdef DEBUG
            parse_system_header(streambuf, packet_size);
#endif
        } else if (id == 0x000001BE) {
            DBG_PRINTF("Found padding header at index %08X\n", file_idx);
            efseek(pss, packet_size, SEEK_CUR, pss_path);
        } else {
            if (0x000001E0 <= id && id <= 0x000001EF) {
                DBG_PRINTF("Found PES video header at index %08X\n", file_idx);
                stream = STREAM_VIDEO;
            } else if (id == 0x000001BD) {
                DBG_PRINTF("Found PES private stream header at index %08X\n", file_idx);
                stream = STREAM_PRIVATE;
            } else {
                printf(
                    "Found unknown PES header id %08X at offset %08x\n",
                    id, file_idx
                );
                exit(-1);
            }
            efread(streambuf, 3, pss, pss_path);
            pes_head_len = streambuf[2];
            efread(streambuf + 3, pes_head_len, pss, pss_path);
#ifdef DEBUG
            parse_pes_ext_header(streambuf);
#endif
            data_len = packet_size - pes_head_len - 3;
            efread(streambuf, data_len, pss, pss_path);
            payload_offset = 0;
            if (stream == STREAM_PRIVATE) {
                ssid = get_u32_be(streambuf);
                if (ssid == 0xFFA00000) {
                    DBG_PRINTF("Private stream contains SS2 audio\n");
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
            data_len -= payload_offset;
            if (streams[stream].f == NULL) {
                streams[stream].path = subext(pss_path, stream_names[stream]);
                efopen(&streams[stream].f, streams[stream].path, "w+b");
            }
            efwrite(
                streambuf + payload_offset, data_len,
                streams[stream].f, streams[stream].path
            );
            DBG_PRINTF("Wrote %d (%04X) bytes\n", data_len, data_len);
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
