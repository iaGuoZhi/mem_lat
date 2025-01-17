#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>

class CombOptions {

public:
enum XOR_type { Delta, DeltaOfDelta};
enum Compress_algo { Gorilla, Chimp, Elf, Timestamp};

private:
  XOR_type xor_type = Delta;
  Compress_algo compress_algo = Gorilla;
  bool use_eraser = false;

public:
  CombOptions(const char* options) {
    if (options == NULL) {
      return;
    }
    char *_options = new char[strlen(options) + 1];
    strcpy(_options, options);
    char* token = strtok(_options, ",");
    while (token != NULL) {
      if (strcmp(token, "delta") == 0) {
        xor_type = Delta;
      } else if (strcmp(token, "delta-of-delta") == 0) {
        xor_type = DeltaOfDelta;
      } else if (strcmp(token, "gorilla") == 0) {
        compress_algo = Gorilla;
      } else if (strcmp(token, "chimp") == 0) {
        compress_algo = Chimp;
      } else if (strcmp(token, "elf") == 0) {
        compress_algo = Elf;
      } else if (strcmp(token, "timestamp") == 0) {
        compress_algo = Timestamp;
      } else if (strcmp(token, "eraser") == 0) {
        use_eraser = true;
      } else {
        printf("Unknown option: %s\n", token);
      }
      token = strtok(NULL, ",");
    }
    delete[] _options;
  }

  CombOptions() {
    xor_type = Delta;
    compress_algo = Gorilla;
    use_eraser = false;
  }

  XOR_type getXORType() { return xor_type; }
  Compress_algo getCompressAlgo() { return compress_algo; }
  bool getUseEraser() { return use_eraser; }
};
