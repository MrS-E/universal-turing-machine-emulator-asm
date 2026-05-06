/*
 * Usage:
 *   ./utm --binary  <binstring>              binary encoding
 *   ./utm --file    <path>                   read encoding from file
 *   ./utm --decimal <number>                 Gödel number (decimal)
 *   ./utm --input   <binstring>              tape input  (0/1 chars)
 *   ./utm --input-unary <decimal>            tape input  (unary 1^n)
 *   ./utm --step                             step mode   (default: run)
 *   ./utm --run                              run  mode
 *   ./utm --max-steps <n>                    safety limit (default 100000)
 */

#define _GNU_SOURCE
#include "tm_types.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file_binary(const char *path);

static char *decimal_to_binary(const char *dec);

static int parse_encoding(const char *bin, Transition **out_trans,
                          int *out_ntrans, char **out_input);

static int count_zeros(const char *s, int *pos);

static void init_tm_state(TMState *st, Transition *trans, int ntrans,
                          const char *input);

static void print_tape(const TMState *st);

static void print_state(const TMState *st);

static void print_result(const TMState *st);

static char sym_to_char(uint8_t sym);

int main(int argc, char **argv) {
  const char *bin_arg = NULL;
  const char *file_arg = NULL;
  const char *dec_arg = NULL;
  const char *input_arg = NULL;
  const char *unary_arg = NULL;
  int step_mode = 0;
  int max_steps = 100000;

  // cli args
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--binary") && i + 1 < argc) {
      bin_arg = argv[++i];
    } else if (!strcmp(argv[i], "--file") && i + 1 < argc) {
      file_arg = argv[++i];
    } else if (!strcmp(argv[i], "--decimal") && i + 1 < argc) {
      dec_arg = argv[++i];
    } else if (!strcmp(argv[i], "--input") && i + 1 < argc) {
      input_arg = argv[++i];
    } else if (!strcmp(argv[i], "--input-unary") && i + 1 < argc) {
      unary_arg = argv[++i];
    } else if (!strcmp(argv[i], "--step")) {
      step_mode = 1;
    } else if (!strcmp(argv[i], "--run")) {
      step_mode = 0;
    } else if (!strcmp(argv[i], "--max-steps") && i + 1 < argc) {
      max_steps = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
      printf("Usage: %s [--binary <str>|--file <path>|--decimal <num>]\n"
             "          [--input <01-str>] [--input-unary <n>]\n"
             "          [--step|--run] [--max-steps <n>]\n",
             argv[0]);
      return 0;
    } else {
      // bare argument: treat as binary string
      bin_arg = argv[i];
    }
  }

  char *encoding = NULL;

  if (file_arg) {
    encoding = read_file_binary(file_arg);
    if (!encoding) {
      fprintf(stderr, "Error reading file: %s\n", file_arg);
      return 1;
    }
  } else if (dec_arg) {
    encoding = decimal_to_binary(dec_arg);
    if (!encoding) {
      fprintf(stderr, "Error converting decimal\n");
      return 1;
    }
  } else if (bin_arg) {
    encoding = strdup(bin_arg);
  } else {
    fprintf(stderr, "Error: no TM encoding provided\n");
    return 1;
  }

  printf("Binary encoding (%zu bits): %.80s%s\n", strlen(encoding), encoding,
         strlen(encoding) > 80 ? "..." : "");

  Transition *trans = NULL;
  int ntrans = 0;
  char *parsed_input = NULL;

  if (parse_encoding(encoding, &trans, &ntrans, &parsed_input) != 0) {
    fprintf(stderr, "Error: failed to parse TM encoding\n");
    free(encoding);
    return 1;
  }
  free(encoding);

  // override tape input if available
  if (input_arg) {
    free(parsed_input);
    parsed_input = strdup(input_arg);
  }
  if (unary_arg) {
    free(parsed_input);
    int n = atoi(unary_arg);
    if (n < 0)
      n = 0;
    parsed_input = malloc(n + 1);
    memset(parsed_input, '1', n);
    parsed_input[n] = '\0';
  }

  printf("\nDecoded %d transition(s):\n", ntrans);
  for (int i = 0; i < ntrans; i++) {
    Transition *t = &trans[i];
    printf("  δ(q%d, %c) = (q%d, %c, %c)\n", t->from_state,
           sym_to_char(t->read_symbol), t->to_state,
           sym_to_char(t->write_symbol), t->direction == DIR_L ? 'L' : 'R');
  }
  if (parsed_input && parsed_input[0])
    printf("Tape input: %s\n", parsed_input);
  else
    printf("Tape input: (empty)\n");

  TMState st;
  init_tm_state(&st, trans, ntrans, parsed_input);
  free(parsed_input);

  if (step_mode) {
    printf("[STEP MODE]  Press Enter to step, 'r' to run, 'q' to quit.\n\n");
    print_state(&st);
    print_tape(&st);
    printf("\n");

    while (!st.halted && st.step_count < max_steps) {
      printf("step> ");
      fflush(stdout);
      char cmd[64] = {0};
      if (!fgets(cmd, sizeof(cmd), stdin))
        break;
      if (cmd[0] == 'q' || cmd[0] == 'Q')
        break;
      if (cmd[0] == 'r' || cmd[0] == 'R') {
        /* switch to run mode for the rest */
        tm_run(&st);
        printf("\n[Switched to RUN — computation finished]\n\n");
        print_result(&st);
        goto done;
      }

      tm_step(&st);

      print_state(&st);
      print_tape(&st);
      printf("\n");
    }
    if (st.step_count >= max_steps && !st.halted)
      printf("\n[Stopped: max-steps limit (%d) reached]\n", max_steps);
    printf("\n");
    print_result(&st);
  } else {
    printf("[RUN MODE]\n\n");
    tm_run(&st);
    print_result(&st);
  }

done:
  free(st.tape);
  free(trans);
  free((void *)(uintptr_t)st.tm_stack_top - TM_STACK_SIZE +
       16); // stack buffer was allocated with extra alignment room
  return 0;
}

static char *read_file_binary(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  rewind(f);

  char *raw = malloc(len + 1);
  if (fread(raw, 1, len, f) != (size_t)len) {
    free(raw);
    fclose(f);
    return NULL;
  }
  raw[len] = '\0';
  fclose(f);

  /* strip non-binary chars */
  char *out = malloc(len + 1);
  int j = 0;
  for (int i = 0; i < len; i++) {
    if (raw[i] == '0' || raw[i] == '1')
      out[j++] = raw[i];
  }
  out[j] = '\0';
  free(raw);
  return out;
}

static char *decimal_to_binary(const char *dec) {
  // strip spaces / apostrophes / dots
  char clean[2048];
  int ci = 0;
  for (const char *p = dec; *p && ci < (int)sizeof(clean) - 1; p++) {
    if (isdigit((unsigned char)*p))
      clean[ci++] = *p;
  }
  clean[ci] = '\0';
  if (ci == 0)
    return NULL;

  int ndig = ci;
  int *digits = malloc(ndig * sizeof(int));
  for (int i = 0; i < ndig; i++)
    digits[i] = clean[i] - '0';

  char *bits = malloc(ndig * 4 + 2);
  int bpos = 0;

  while (ndig > 0) {
    int rem = 0;
    int wpos = 0;
    for (int i = 0; i < ndig; i++) {
      int cur = rem * 10 + digits[i];
      digits[wpos] = cur / 2;
      rem = cur % 2;
      if (wpos > 0 || digits[wpos] != 0)
        wpos++;
    }
    bits[bpos++] = '0' + rem;
    ndig = wpos;
  }
  bits[bpos] = '\0';
  free(digits);

  for (int i = 0, j = bpos - 1; i < j; i++, j--) {
    char tmp = bits[i];
    bits[i] = bits[j];
    bits[j] = tmp;
  }
  return bits;
}

static int count_zeros(const char *s, int *pos) {
  int cnt = 0;
  while (s[*pos] == '0') {
    cnt++;
    (*pos)++;
  }
  return cnt;
}

static int parse_transition(const char *s, int len, Transition *t) {
  int pos = 0;
  int vals[5];
  for (int v = 0; v < 5; v++) {
    vals[v] = count_zeros(s, &pos);
    if (vals[v] == 0)
      return -1;
    if (v < 4) {
      if (pos >= len || s[pos] != '1')
        return -1;
      pos++;
    }
  }
  t->from_state = vals[0];
  t->read_symbol = vals[1];
  t->to_state = vals[2];
  t->write_symbol = vals[3];
  t->direction = vals[4];
  return 0;
}

static int parse_encoding(const char *bin, Transition **out_trans,
                          int *out_ntrans, char **out_input) {
  *out_trans = NULL;
  *out_ntrans = 0;
  *out_input = NULL;

  if (!bin || !*bin)
    return -1;

  char *work = strdup(bin);
  int wlen = (int)strlen(work);

  char *code = work;
  if (code[0] == '1' && wlen > 1 && code[1] == '0') {
    code++;
    wlen--;
  }

  char *sep = strstr(code, "111");
  char *input_part = NULL;
  if (sep) {
    *sep = '\0';
    input_part = sep + 3;
  }

  int cap = 16;
  Transition *trs = malloc(cap * sizeof(Transition));
  int ntrs = 0;

  const char *p = code;
  while (*p) {
    const char *end = p;
    while (*end) {
      if (end[0] == '1' && end[1] == '1')
        break;
      end++;
    }
    int tlen = (int)(end - p);
    if (tlen > 0) {
      if (ntrs >= cap) {
        cap *= 2;
        trs = realloc(trs, cap * sizeof(Transition));
      }
      char *tseg = strndup(p, tlen);
      if (parse_transition(tseg, tlen, &trs[ntrs]) == 0)
        ntrs++;
      else
        fprintf(stderr, "Warning: skipping malformed transition segment: %s\n",
                tseg);
      free(tseg);
    }
    if (*end == '1' && *(end + 1) == '1') {
      p = end + 2;
    } else {
      break;
    }
  }

  *out_trans = trs;
  *out_ntrans = ntrs;
  *out_input = input_part ? strdup(input_part) : strdup("");

  free(work);
  return (ntrs > 0) ? 0 : -1;
}

static void init_tm_state(TMState *st, Transition *trans, int ntrans,
                          const char *input) {
  memset(st, 0, sizeof(*st));
  st->current_state = 1;
  st->tape_len = TAPE_SIZE;
  st->num_transitions = ntrans;
  st->transitions = trans;

  st->tape = malloc(TAPE_SIZE);
  memset(st->tape, SYM_BLANK, TAPE_SIZE);

  st->head_pos = TAPE_ORIGIN;
  if (input) {
    for (int i = 0; input[i]; i++) {
      if (input[i] == '0')
        st->tape[TAPE_ORIGIN + i] = SYM_0;
      else if (input[i] == '1')
        st->tape[TAPE_ORIGIN + i] = SYM_1;
    }
  }

  // allocate TM private stack (16-byte aligned)
  void *stk_buf = aligned_alloc(16, TM_STACK_SIZE);
  memset(stk_buf, 0, TM_STACK_SIZE);
  // stack grows downward
  st->tm_stack_top = (uint64_t)((char *)stk_buf + TM_STACK_SIZE - 16);
}

static char sym_to_char(uint8_t sym) {
  switch (sym) {
  case SYM_0:
    return '0';
  case SYM_1:
    return '1';
  case SYM_BLANK:
    return '_';
  default: {
    // X4, X5, ... -> 'a','b',...
    if (sym >= 4 && sym < 4 + 26)
      return 'a' + (sym - 4);
    return '?';
  }
  }
}

static void print_tape(const TMState *st) {
  int lo = st->head_pos - DISPLAY_RADIUS;
  int hi = st->head_pos + DISPLAY_RADIUS;
  if (lo < 0)
    lo = 0;
  if (hi >= st->tape_len)
    hi = st->tape_len - 1;

  printf("  Tape:  ... ");
  for (int i = lo; i <= hi; i++) {
    if (i == st->head_pos)
      printf("[%c]", sym_to_char(st->tape[i]));
    else
      printf(" %c ", sym_to_char(st->tape[i]));
  }
  printf(" ...\n");

  printf("  Head position: %d (offset from origin: %+d)\n", st->head_pos,
         st->head_pos - TAPE_ORIGIN);
}

static void print_state(const TMState *st) {
  printf("  State: q%d   |  Steps: %d", st->current_state, st->step_count);
  if (st->halted)
    printf("  |  HALTED (%s)", st->accepted ? "ACCEPTED" : "REJECTED");
  printf("\n");
}

static void print_result(const TMState *st) {
  printf("  RESULT\n");
  print_state(st);
  print_tape(st);

  printf("  Output: ");
  int first = -1, last = -1;
  for (int i = 0; i < st->tape_len; i++) {
    if (st->tape[i] != SYM_BLANK) {
      if (first < 0)
        first = i;
      last = i;
    }
  }
  if (first >= 0) {
    for (int i = first; i <= last; i++)
      putchar(sym_to_char(st->tape[i]));
  } else {
    printf("(empty)");
  }
  printf("\n");

  if (first >= 0) {
    int ones = 0;
    for (int i = first; i <= last; i++)
      if (st->tape[i] == SYM_1)
        ones++;
    printf("  Unary count (number of 1s): %d\n", ones);
  }
}
