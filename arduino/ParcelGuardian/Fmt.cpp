#include "Fmt.h"

char *fmtFixed(char *out, float v, uint8_t dp) {
  char *d = out;

  /* NaN and infinity would otherwise print as garbage digits. */
  if (isnan(v))  { strcpy_P(d, PSTR("nan")); return d + 3; }
  if (isinf(v))  { strcpy_P(d, PSTR("inf")); return d + 3; }

  if (v < 0.0f) { *d++ = '-'; v = -v; }

  uint32_t scale = 1;
  for (uint8_t i = 0; i < dp; i++) scale *= 10UL;

  /* Guard against values too large for the integer split. Anything this
   * big is a sensor fault, not a reading. */
  if (v >= 4294967.0f) { strcpy_P(d, PSTR("ovf")); return d + 3; }

  uint32_t whole = (uint32_t)v;
  uint32_t frac  = (uint32_t)((v - (float)whole) * (float)scale + 0.5f);

  if (frac >= scale) { frac -= scale; whole++; }   /* rounding carried */

  ultoa(whole, d, 10);
  while (*d) d++;

  if (dp) {
    *d++ = '.';
    for (uint32_t div = scale / 10; div; div /= 10) {
      *d++ = (char)('0' + (frac / div) % 10);
    }
    *d = '\0';
  }
  return d;
}

void fmtPrint(Print &p, float v, uint8_t dp) {
  char buf[20];
  fmtFixed(buf, v, dp);
  p.print(buf);
}
