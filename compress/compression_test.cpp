#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <iostream>
#include <time.h>

#include "Chimp/chimp.h"
#include "Gorilla/gorilla.h"
#include "Elf/elf.h"

enum ListError {
  SKIP = -2,
  EOL,
};

enum Type { Lossy, Lossless };

typedef struct {
  uint64_t ori_size;
  uint64_t cmp_size;
  uint64_t cmp_time;
  uint64_t dec_time;
} Perf;

Perf empty = {0, 0, 0, 0};

/*********************************************************************
 *                      Evaluation Settings
 *********************************************************************/
// Available compressors
struct {
  char name[16];
  Type type;
  int (*compress)(double *in, size_t len, uint8_t **out, double error);
  int (*decompress)(uint8_t *in, size_t len, double *out, double error);
  Perf perf;
} compressors[] = {
    {"Gorilla", Type::Lossless, gorilla_encode, gorilla_decode, empty},
    {"Chimp", Type::Lossless, chimp_encode, chimp_decode, empty},
    {"Elf", Type::Lossless, elf_encode, elf_decode, empty},
};

// Available datasets
struct {
  char name[32];
  const char *path;
  double error;
} datasets[] = {
    {"us-stocks", "./data/us-stocks.csv", 1E-3},
    {"air-sensor", "./data/air-sensor.csv", 1E-3},
    {"bird-migration", "./data/bird-migration.csv", 1E-3},
    {"bitcoin-historical", "./data/bitcoin-historical.csv", 1E-3},
};

// List of compressors to be evaluated
int compressor_list[] = {0, 1, 2, EOL};
// List of datasets to be evaluated
int dataset_list[] = {0, 1, 2, 3, EOL};
// List of slice lengths to be evaluated
int bsize_list[] = {5, 1000, EOL};

///////////////////////// Setting End ////////////////////////////

int check(double *d0, double *d1, size_t len, double error) {
  if (error == 0) {
    for (int i = 0; i < len; i++) {
      if (d0[i] != d1[i]) {
        printf("%d: %.16lf(%lx) vs %.16lf(%lx)\n", i, d0[i],
               ((uint64_t *)d0)[i], d1[i], ((uint64_t *)d1)[i]);
        return -1;
      }
    }
  } else {
    for (int i = 0; i < len; i++) {
      if (std::abs(d0[i] - d1[i]) > error) {
        printf("%d(%lf): %lf vs %lf\n", i, d0[i] - d1[i], d0[i], d1[i]);
        return -1;
      }
    }
  }
  return 0;
}

int read_csv_file(FILE *file, double *data, int len) {
  char line[1024];
  int i = 0;
  while (fgets(line, 1024, file)) {
    char *token = strtok(line, ",");
    token = strtok(NULL, ",");
    data[i++] = atof(token);
    if (i >= len)
      return i;
  }
  return i;
}

int test_file(FILE *file, int c, int chunk_size, double error) {
  double *d0 = (double *)malloc(chunk_size * sizeof(double));
  uint8_t *d1;
  double *d2 = (double *)malloc(chunk_size * sizeof(double));
  int64_t start;

  // compress
  FILE *fc = fopen("/tmp/tmp.cmp", "w");
  int block = 0;
  while (!feof(file)) {
    int len0 = read_csv_file(file, d0, chunk_size);
    if (len0 == 0)
      break;

    start = clock();
    int len1 = compressors[c].compress(d0, len0, &d1, error);
    compressors[c].perf.cmp_time += clock() - start;
    compressors[c].perf.cmp_size += len1;

    fwrite(&len1, sizeof(len1), 1, fc);
    fwrite(d1, 1, len1, fc);
    free(d1);
    block++;
  }
  fclose(fc);

  // decompress
  rewind(file);
  fc = fopen("/tmp/tmp.cmp", "r");
  block = 0;
  while (!feof(file)) {
    int len0 = read_csv_file(file, d0, chunk_size);
    if (len0 == 0)
      break;

    int len1;
    (void)!fread(&len1, sizeof(len1), 1, fc);
    d1 = (uint8_t *)malloc(len1);
    (void)!fread(d1, 1, len1, fc);
    start = clock();
    int len2 = compressors[c].decompress(d1, len1, d2, error);
    compressors[c].perf.dec_time += clock() - start;
    compressors[c].perf.ori_size += len2 * sizeof(double);

    double terror;
    switch (compressors[c].type) {
    case Lossy:
      terror = error;
      break;
    case Lossless:
      terror = 0;
      break;
    }

    if (len0 != len2 || check(d0, d2, len0, terror)) {
      printf("Check failed, dumping data to tmp.data\n");
      FILE *dump = fopen("tmp.data", "w");
      fwrite(d0, sizeof(double), len0, dump);
      fclose(dump);

      free(d1);
      free(d2);
      free(d0);
      fclose(file);
      return -1;
    }
    free(d1);
    block++;
  }
  fclose(fc);

  free(d0);
  free(d2);
  return 0;
}

void draw_progress(int now, int total, int len) {
  int count = now * len / total;
  int i;
  fprintf(stderr, "Progress: ");
  for (i = 0; i < count; i++) {
    fputc('#', stderr);
  }
  for (; i < len; i++) {
    fputc('-', stderr);
  }
  fputc('\r', stderr);
}

int test_dataset(int ds, int chunk_size) {
  printf("**************************************\n");
  printf("      Testing on %s(%.8lf)\n", datasets[ds].name, datasets[ds].error);
  printf("**************************************\n");
  fflush(stdout);

  const char *file_path = datasets[ds].path;

  //draw_progress(cur_file, file_cnt, 80);
  FILE *file = fopen(file_path, "rb");
  for (int i = 0; compressor_list[i] != EOL; i++) {
    if (compressor_list[i] == SKIP) {
      continue;
    }
    fseek(file, 0, SEEK_SET);
    if (test_file(file, compressor_list[i], chunk_size, datasets[ds].error)) {
      printf("Error Occurred while testing %s, skipping\n",
             compressors[compressor_list[i]].name);
      compressor_list[i] = SKIP;
    }
  }
  fclose(file);
  //draw_progress(cur_file, file_cnt, 80);
  return 0;
}

void report(int c) {
  printf("========= %s ==========\n", compressors[c].name);
  printf("Compression ratio: %lf\n",
         (double)compressors[c].perf.ori_size / compressors[c].perf.cmp_size);
  printf("Compression speed: %lf MB/s\n",
         (double)compressors[c].perf.ori_size / 1024 / 1024 /
             ((double)compressors[c].perf.cmp_time / CLOCKS_PER_SEC));
  printf("Decompression speed: %lf MB/s\n",
         (double)compressors[c].perf.ori_size / 1024 / 1024 /
             ((double)compressors[c].perf.dec_time / CLOCKS_PER_SEC));
  printf("\n");
  fflush(stdout);
}

int main() {
  double data[1000];
  for (int i = 0; i < 1000; i++) {
    data[i] = (double)rand() / RAND_MAX;
  }

  for (int i = 0; bsize_list[i] != EOL; i++) {
    printf("Current slice length: %d\n", bsize_list[i]);
    fflush(stdout);
    for (int j = 0; dataset_list[j] != EOL; j++) {
      test_dataset(dataset_list[j], bsize_list[i]);
      for (int k = 0; compressor_list[k] != EOL; k++) {
        if (compressor_list[k] != SKIP) {
          report(compressor_list[k]);
        }
        Perf &p = compressors[compressor_list[k]].perf;
        __builtin_memset(&compressors[compressor_list[k]].perf, 0,
                         sizeof(Perf));
      }
    }
  }
  printf("Test finished\n");
  return 0;
}