/*
 * compile with
 *  gcc -o test-encoder decode.c -lltc -lm
 *
 * from the source-folder this can be compiled after building libltc:
 *  gcc -o ltc-encoder example_encode.c ../src/.libs/libltc.a -lm -I../src
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <ltc.h>

#define BUFFER_SIZE (1024)

int main(int argc, char **argv)
{
    int apv = 1920;
    ltcsnd_sample_t sound[BUFFER_SIZE];
    size_t n;
    long int total;
    FILE *f;
    char *filename = "gen_timecode";
    LTCDecoder *decoder;
    LTCFrameExt frame;

    f = fopen(filename, "r");
    if (!f)
    {
        fprintf(stderr, "error opening '%s'\n", filename);
        return -1;
    }
    fprintf(stderr, "* reading from: %s\n", filename);

    decoder = ltc_decoder_create(apv, 32);

    do
    {
        n = fread(sound, sizeof(ltcsnd_sample_t), BUFFER_SIZE, f);
        ltc_decoder_write(decoder, sound, n, total);
        while (ltc_decoder_read(decoder, &frame))
        {
            SMPTETimecode stime;
            ltc_frame_to_time(&stime, &frame.ltc, 1);
            printf("%04d-%02d-%02d %s ",
                   ((stime.years < 67) ? 2000 + stime.years : 1900 + stime.years),
                   stime.months,
                   stime.days,
                   stime.timezone);
            printf("%02d:%02d:%02d%c%02d | %8lld %8lld%s\n",
                   stime.hours,
                   stime.mins,
                   stime.secs,
                   (frame.ltc.dfbit) ? '.' : ':',
                   stime.frame,
                   frame.off_start,
                   frame.off_end,
                   frame.reverse ? "  R" : "");
        }
        total += n;
    } while (n);
    fclose(f);
    ltc_decoder_free(decoder);

    return 0;
}