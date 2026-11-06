#include "Diagnostics.h"

/* Supplied by the avr-libc startup code. __brkval is 0 until the heap is
 * first used, in which case the heap start symbol is the right boundary. */
extern int  __heap_start;
extern int *__brkval;

int freeRam() {
  int stackTop;
  return (int)&stackTop -
         (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
