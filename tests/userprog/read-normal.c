/* Try reading a file in the most normal way. */

#include "tests/userprog/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
 
  //msg("sample addr: %p\n", sample);
  check_file ("sample.txt", sample, sizeof sample - 1);
}
