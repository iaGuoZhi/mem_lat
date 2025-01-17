#include "AbstractCompressor.h"

class ElfXORCompressor : public AbstractCompressor {
private:
  int storedLeadingZeros = __INT32_MAX__;
  int storedTrailingZeros = __INT32_MAX__;
  uint64_t storedVal = 0;

  void writeFirst(long value);
  void compressValue(long value);

public:
  ~ElfXORCompressor() {}
};
