#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "BitStream/BitReader.h"
#include "ChimpDef.h"

static const int16_t leadingRep[] = {0, 8, 12, 16, 18, 20, 22, 24};

int chimp_decode(uint8_t *in, size_t len, double *out, double error, const char *options) {
  assert((len - 10) % 4 == 0);

  int32_t storedLeadingZeros = INT32_MAX;
  int32_t storedTrailingZeros = 0;

  int data_len = *(uint16_t *)in;
  out[0] = *(double *)(in + 2);
  int64_t *data = (int64_t *)out;
  BitReader reader;
  initBitReader(&reader, (uint32_t *)(in + 2 + 8), (len - 10) / 4);

  int32_t previousValues = PREVIOUS_VALUES;
  int32_t previousValuesLog2 = 31 - __builtin_clz(PREVIOUS_VALUES);
  int32_t initialFill = previousValuesLog2 + 9;
  int64_t *storedValues = (int64_t *)calloc(sizeof(int64_t), previousValues);

  int64_t delta;
  storedValues[0] = data[0];

  for (int i = 1; i < data_len; i++) {
    int32_t flag = read(&reader, 2);
    uint32_t tmp, fill, index, significantBits;
    switch (flag) {
    case 3:
      tmp = read(&reader, 3);
      storedLeadingZeros = leadingRep[tmp];
      delta = readLong(&reader, 64 - storedLeadingZeros);
      data[i] = data[i - 1] ^ delta;
      storedValues[i % previousValues] = data[i];
      break;
    case 2:
      delta = readLong(&reader, 64 - storedLeadingZeros);
      data[i] = data[i - 1] ^ delta;
      storedValues[i % previousValues] = data[i];
      break;
    case 1:
      fill = initialFill;
      tmp = read(&reader, fill);
      fill -= previousValuesLog2;
      index = (tmp >> fill) & ((1 << previousValuesLog2) - 1);
      fill -= 3;
      storedLeadingZeros = leadingRep[(tmp >> fill) & 0x7];
      fill -= 6;
      significantBits = (tmp >> fill) & 0x3f;
      if (significantBits == 0) {
        significantBits = 64;
      }
      storedTrailingZeros = 64 - significantBits - storedLeadingZeros;
      delta = readLong(&reader, significantBits);
      data[i] = storedValues[index] ^ (delta << storedTrailingZeros);
      storedValues[i % previousValues] = data[i];
      break;
    default:
      data[i] = storedValues[readLong(&reader, previousValuesLog2)];
      storedValues[i % previousValues] = data[i];
      break;
    }
  }
  free(storedValues);
  return data_len;
}