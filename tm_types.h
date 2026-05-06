#ifndef TM_TYPES_H
#define TM_TYPES_H

#include <stdint.h>

#define TAPE_SIZE 131072            // 128 KiB tape
#define TAPE_ORIGIN (TAPE_SIZE / 2) // head starts in the middle
#define TM_STACK_SIZE 65536         // 64 KiB private ASM stack
#define DISPLAY_RADIUS 15           // cells shown around head

/*
 * Symbols
 * 1: X1 = '0'
 * 2: X2 = '1'
 * 3: X3 = blank
 * 1: D1 = L
 * 2: D2 = R
 */
#define SYM_0 1
#define SYM_1 2
#define SYM_BLANK 3
#define DIR_L 1
#define DIR_R 2

typedef struct {
  int32_t from_state;   // qi
  int32_t read_symbol;  // Xj
  int32_t to_state;     // qk
  int32_t write_symbol; // Xl
  int32_t direction;    // Dm
} Transition;

/*
 * synced with engine
 *
 *  offset  field
 *  ------  ------------------
 *    0     current_state   (int32)
 *    4     head_pos        (int32)
 *    8     step_count      (int32)
 *   12     halted          (int32)
 *   16     accepted        (int32)
 *   20     tape_len        (int32)
 *   24     tape            (ptr, 8 bytes)
 *   32     num_transitions (int32)
 *   36     _pad            (int32)
 *   40     transitions     (ptr, 8 bytes)
 *   48     saved_c_rsp     (uint64)
 *   56     tm_stack_top    (uint64)
 */
typedef struct {
  int32_t current_state;
  int32_t head_pos;
  int32_t step_count;
  int32_t halted;
  int32_t accepted;
  int32_t tape_len;
  uint8_t *tape;
  int32_t num_transitions;
  int32_t _pad;
  Transition *transitions;
  uint64_t saved_c_rsp;
  uint64_t tm_stack_top;
} TMState;

// return: 0 = still running, 1 = halted
extern int tm_step(TMState *state); // execute ONE transition
extern int tm_run(TMState *state);  // loop until halt

#endif // TM_TYPES_H */
