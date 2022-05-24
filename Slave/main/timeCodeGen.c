#include "timeCodeGen.h"

int decode(ltcsnd_sample_t *buffer, int len_buffer)
{
  int apv = 1920;
  ltcsnd_sample_t sound[1024];
  size_t n;
  long int total = 0;

  LTCDecoder *decoder;
  LTCFrameExt frame;

  decoder = ltc_decoder_create(apv, 32);
  int index = 0;

  do
  {
    if(index+1024<len_buffer) n = 1024;
    else n = len_buffer-index;
    memcpy(sound, buffer+index, n);
    index += n;
    ltc_decoder_write(decoder, sound, n, total);
    while (ltc_decoder_read(decoder, &frame))
    {
      SMPTETimecode stime;
      ltc_frame_to_time(&stime, &frame.ltc, 1);

      ESP_LOGI(BT_DEC_TAG,"%04d-%02d-%02d %s ",
                    ((stime.years < 67) ? 2000 + stime.years : 1900 + stime.years),
                    stime.months,
                    stime.days,
                    stime.timezone);

      ESP_LOGI(BT_DEC_TAG,"%02d:%02d:%02d%c%02d | %8lld %8lld%s\n",
                    stime.hours,
                    stime.mins,
                    stime.secs,
                    (frame.ltc.dfbit) ? '.' : ':',
                    stime.frame,
                    frame.off_start,
                    frame.off_end,
                    frame.reverse ? "  R" : "");
      printf("mins: %02d, secs: %02d, frame: %02d\n", stime.mins, stime.secs, stime.frame);
    }
    total += n;
  } while (n);
  ltc_decoder_free(decoder);

  return 0;
}

int encode(SMPTETimecode st, uint8_t *buffer, int *len_buffer)
{
  /* encoding the SMPTE timecode as a LTC timecode, see libLTC: https://github.com/x42/libltc */
  double length = 2;
  double fps = 25;

  LTCEncoder *encoder;

  int vframe_cnt;
  int vframe_last;

  int total = 0;
  ltcsnd_sample_t *buf;

  /* LTC encoder */
  encoder = ltc_encoder_create(SAMPLE_RATE, FPS, LTC_TV_625_50, LTC_USE_DATE);
  ltc_encoder_set_timecode(encoder, &st);

  vframe_cnt = 0;
  vframe_last = length * fps;

  ESP_LOGI(BT_ENC_TAG, "vframe last : %d", vframe_last);

  while (vframe_cnt++ < 2)
  {
    /* encode a complete LTC frame in one step */
    ltc_encoder_encode_frame(encoder);
    int len;
    buf = ltc_encoder_get_bufptr(encoder, &len, 1);

    if (len > 0)
    {
      memcpy(&(buffer[total]), buf, len);
      total += len;
      *len_buffer = total;
    }
    
    ltc_encoder_inc_timecode(encoder);
  }
  ltc_encoder_free(encoder);
  ESP_LOGI(BT_ENC_TAG, "Done: wrote %d samples\n", total);

  return 0;
}

int genTimeCode(uint8_t *data, int *len_data)
{
  /* Generate the SMPTE TimeCode with the internal clock timestamp */
  SMPTETimecode st;

  int year, month, day, hour, min, sec;

  gettimeofday(&latency.time_gen_LTC, NULL);
  struct tm *time = localtime(&latency.time_gen_LTC.tv_sec);

  year = 1900;
  month = time->tm_mon + 1;
  day = time->tm_mday;
  hour = time->tm_hour;
  min = time->tm_min;
  sec = time->tm_sec;

  const char timezone[6] = "+0100";
  strcpy(st.timezone, timezone);
  st.years = time->tm_year + 1900 + '0';
  st.months = time->tm_mon + 1;
  st.days = time->tm_mday;

  st.hours = time->tm_hour;
  st.mins = time->tm_min;
  st.secs = time->tm_sec;

  /* converting the subseconds value to a frame value, SMPTE TimeCode are precise at the frame level */
  double msecs = latency.time_gen_LTC.tv_usec/1000;
  double frame = msecs * FPS/1000;
  st.frame = (unsigned char) frame;

  if (!encode(st, data, len_data))
  {
    /* use the following line to debug, to verify that the encoding is correct */
    // decode(data, *len_data);
    return 0; // success
  }
  return 1; // failure
}