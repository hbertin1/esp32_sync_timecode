// #include <time.h>
// #include <string.h>
// #include <ltc.h>
// #include <sys/time.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"
#include "esp_log.h"


// #define SAMPLE_RATE 48000
// #define FPS 25
// #define BT_ENC_TAG "ENC"
// #define BT_DEC_TAG "DEC"
// static unsigned char bufferTest[3*1920];

// int decode(ltcsnd_sample_t *buffer, int len_buffer)
// {
//   int apv = 1920;
//   ltcsnd_sample_t sound[1024];
//   size_t n;
//   long int total = 0; // not sure to start at 0 here

//   LTCDecoder *decoder;
//   LTCFrameExt frame;

//   decoder = ltc_decoder_create(apv, 32);
//   int index = 0;

//   do
//   {
//     ESP_LOGI(BT_DEC_TAG, "index = %d", index);
//     // n = fread(sound, sizeof(ltcsnd_sample_t), BUFFER_SIZE, f);
//     if(index+1024<len_buffer) n = 1024;
//     else n = len_buffer-index;
//     memcpy(sound, bufferTest+index, n);
//     index += n;
//     ESP_LOGI(BT_DEC_TAG, "n = %zu", n);
//     ltc_decoder_write(decoder, sound, n, total);
//     while (ltc_decoder_read(decoder, &frame))
//     {
//       ESP_LOGI(BT_DEC_TAG,"On rentre ici");
//       SMPTETimecode stime;
//       ltc_frame_to_time(&stime, &frame.ltc, 1);

//       ESP_LOGI(BT_DEC_TAG,"%04d-%02d-%02d %s ",
//                     ((stime.years < 67) ? 2000 + stime.years : 1900 + stime.years),
//                     stime.months,
//                     stime.days,
//                     stime.timezone);

//       ESP_LOGI(BT_DEC_TAG,"%02d:%02d:%02d%c%02d | %8lld %8lld%s\n",
//                     stime.hours,
//                     stime.mins,
//                     stime.secs,
//                     (frame.ltc.dfbit) ? '.' : ':',
//                     stime.frame,
//                     frame.off_start,
//                     frame.off_end,
//                     frame.reverse ? "  R" : "");
//     }
//     total += n;
//   } while (n);
//   ltc_decoder_free(decoder);

//   return 0;
// }

// int encode(SMPTETimecode st, esp_spp_cb_param_t *param, uint8_t *data, int *len_data)
// {
//   double length = 2;
//   double fps = 25;

//   LTCEncoder *encoder;

//   int vframe_cnt;
//   int vframe_last;

//   int total = 0;
//   ltcsnd_sample_t *buf;

//   /* LTC encoder */
//   encoder = ltc_encoder_create(SAMPLE_RATE, FPS, LTC_TV_625_50, LTC_USE_DATE);
//   ltc_encoder_set_timecode(encoder, &st);   
//   // Probleme ici : l encodage n est pas realise de la meme maniere que mon laptop

//   vframe_cnt = 0;
//   vframe_last = length * fps;

//   ESP_LOGI(BT_ENC_TAG, "vframe last : %d\n", vframe_last);

//   int length_buffer_data = *len_data;

//   while (vframe_cnt++ < 3)
//   {
//     ESP_LOGI(BT_ENC_TAG, "length_buffer_data : %d\n", length_buffer_data);

//     /* encode a complete LTC frame in one step */
//     ltc_encoder_encode_frame(encoder);
//     int len;
//     buf = ltc_encoder_get_bufptr(encoder, &len, 1);

//     if (len > 0)
//     {
//       memcpy(bufferTest + length_buffer_data, buf, len);
//       length_buffer_data += len;
//       total += len;
//     }
//     *len_data = length_buffer_data;
//     ltc_encoder_inc_timecode(encoder);
//   }
//   ltc_encoder_free(encoder);
//   ESP_LOGI(BT_ENC_TAG, "Done: wrote %d samples\n", total);

//   return 0;
// }

// char genTimeCode(esp_spp_cb_param_t *param, uint8_t *data, int *len_data)
// {
//   SMPTETimecode st;

//   int year, month, day, hour, min, sec;
//   time_t now;

//   // time(&now);

//   struct timeval tv;
//   gettimeofday(&tv, NULL);
//   // struct tm *time = localtime(&now);
//   struct tm *time = localtime(&(tv.tv_sec));

//   year = 1900;
//   month = time->tm_mon + 1;
//   day = time->tm_mday;
//   hour = time->tm_hour;
//   min = time->tm_min;
//   sec = time->tm_sec;

//   const char timezone[6] = "+0100";
//   strcpy(st.timezone, timezone);
//   st.years = time->tm_year + 1900 + '0';
//   st.months = time->tm_mon + 1;
//   st.days = time->tm_mday;

//   st.hours = time->tm_hour;
//   st.mins = time->tm_min;
//   st.secs = time->tm_sec;
//   st.frame = 0;

//   if (!encode(st, param, data, len_data))
//   {
//     // verify that the encoding is correct
//     decode(data, *len_data);
//     return 0;
//   }
//   return 1; // failure
// }

/*
 * compile with
 *  gcc -o decode test.c -lltc -lm
 *
 * from the source-folder this can be compiled after building libltc:
 *  gcc -o ltc-encoder example_encode.c ../src/.libs/libltc.a -lm -I../src
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include "ltc.h"

#define BUFFER_SIZE (1024)

static unsigned char buffer[3 * 1920];
static int len_buffer = 0;

int encode()
{
  FILE *file;
  double length = 2; // in seconds
  double fps = 25;
  double sample_rate = 48000;
  char *filename = "test";

  int vframe_cnt;
  int vframe_last;

  int total = 0;
  ltcsnd_sample_t *buf;

  LTCEncoder *encoder;
  SMPTETimecode st;

  clock_t clock();

  /* get Time */
  time_t now = time(NULL);

  /* start encoding */
  const char timezone[6] = "+0100";
  strcpy(st.timezone, timezone);
  st.years = 22;
  st.months = 12;
  st.days = 25;

  st.hours = 22;
  st.mins = 30;
  st.secs = 12;
  st.frame = 0;

  // /* open output file */
  // file = fopen(filename, "wb");
  // if (!file)
  // {
  //     fprintf(stderr, "Error: can not open file '%s' for writing.\n", filename);
  //     return 0;
  // }

  /* prepare encoder */
  encoder = ltc_encoder_create(sample_rate, fps,
                               fps == 25 ? LTC_TV_625_50 : LTC_TV_525_60, LTC_USE_DATE);

  buf = calloc(ltc_encoder_get_buffersize(encoder), sizeof(ltcsnd_sample_t));
  if (!buf)
    return;

  ltc_encoder_set_timecode(encoder, &st);

  vframe_cnt = 0;
  vframe_last = length * fps;

  printf("vframe last : %d\n", vframe_last);

  while (vframe_cnt++ < 3)
  {
    /* encode and write each of the 80 LTC frame bits (10 bytes) */
    // int byte_cnt;
    // for (byte_cnt = 0; byte_cnt < 10; byte_cnt++)
    // {
    //     ltc_encoder_encode_byte(encoder, byte_cnt, 1.0);

    //     int len;
    //     buf = ltc_encoder_get_bufptr(encoder, &len, 1);

    //     if (len > 0)
    //     {
    //         fwrite(buf, sizeof(ltcsnd_sample_t), len, file);
    //         total += len;
    //     }
    // }

    /* encode a complete LTC frame in one step */
    ltc_encoder_encode_frame(encoder);

    // int len;
    // printf("size char : %u", sizeof(ltcsnd_sample_t));
    // buf = ltc_encoder_get_bufptr(encoder, &len, 1);
    // // esp_log_buffer_hex("ENC", buf, len);
    // printf("buf[0] encoder: %u\n", buf[len_buffer+145]);

    size_t len = ltc_encoder_get_buffer(encoder, buf);

    for (int i = 0; i < len; i++)
    {
      if (i == 0 % 16)
        printf("\n");
      printf("%u ", buf[i]);
    }
    printf("len %d\n", len);

    if (len > 0)
    {
      // fwrite(buf, sizeof(ltcsnd_sample_t), len, file);
      memcpy(buffer + len_buffer, buf, len);
      len_buffer += len;
      total += len;
    }

    ltc_encoder_inc_timecode(encoder);
  }

  // fclose(file);
  ltc_encoder_free(encoder);
  free(buf);
  printf("Done: wrote %d samples to '%s'\n", total, filename);

  return 0;
}

int decode()
{
  int apv = 1920;
  ltcsnd_sample_t sound[BUFFER_SIZE];
  size_t n;
  long int total = 0;
  FILE *f;
  char *filename = "test";
  LTCDecoder *decoder;
  LTCFrameExt frame;

  decoder = ltc_decoder_create(apv, 32);
  int index = 0;

  do
  {
    printf("total = %ld\n", total);
    // n = fread(sound, sizeof(ltcsnd_sample_t), BUFFER_SIZE, f);
    if (index + 1024 < len_buffer)
      n = 1024;
    else
      n = len_buffer - index;
    memcpy(sound, buffer + index, n); // revoir ici comment donner les bonnes informations
    printf("buffer[0] = %u\n", buffer[index]);
    printf("n : %zu\n", n);
    index += n;
    ltc_decoder_write(decoder, sound, n, total);
    // int ret;
    // int ret = ltc_decoder_read(decoder, &frame);
    // printf("ret = %d\n", ret);
    while (ltc_decoder_read(decoder, &frame))
    {
      ESP_LOGD("DEC", "test");
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
    // printf("ret = %d\n", ret);
    total += n;
  } while (n);
  // fclose(f);
  ltc_decoder_free(decoder);

  return 0;
}

char genTimeCode(esp_spp_cb_param_t *param, uint8_t *data, int *len_data)
{
  /* this is based on the assumption that with every compiler
   * ((unsigned char) 128)<<1 == ((unsigned char 1)>>1) == 0
   */
  unsigned char test1 = 128;
  test1 = test1 << 1;
  unsigned char test2 = 1;
  test2 = test2 >> 1;
  printf("(unsigned char) 128)<<1 ==%u\n", test1);
  printf("((unsigned char 1)>>1) ==%u\n", test2);

#if BIG_ENDIAN
  printf("big\n");
#endif

  if (encode())
    return 1;
  if (decode())
    return 1;

  return 0;
}