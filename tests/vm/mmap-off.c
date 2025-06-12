/* Tries to mmap with offset > 0. */

#include <syscall.h>
#include <string.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "tests/vm/large.inc"
static char zeros[0x1000];

void
test_main (void) 
{
  int handle;
  char buf[0x1000];

  CHECK ((handle = open ("large.txt")) > 1, "open \"large.txt\"");
    // large 내용을 0x1000(4096)부터 4096바이트, 한 페이지 크기만큼 mmap
  CHECK (mmap ((void *) 0x10000000, 4096, 1, handle, 0x1000) == (void *) 0x10000000,
          "try to mmap with offset 0x1000");
  close (handle);

//   hex_dump(0x10000000, 0x10000000, 100, true);

  msg ("validate mmap.");
  if (!memcmp ((void *) 0x10000000, &large[0x1000], 0x1000))
      msg ("validated.");
  else
      fail ("validate fail.");

  msg ("write to mmap");
  memset (zeros, 0, 0x1000);
  memcpy ((void *) 0x10000000, zeros, 0x1000);
  munmap ((void *) 0x10000000);

  msg ("validate contents.");

  CHECK ((handle = open ("large.txt")) > 1, "open \"large.txt\"");

  CHECK (0x1000 == read (handle, buf, 0x1000), "read \"large.txt\" Page 0");


  msg ("validate page 0.");

  if (!memcmp ((void *) buf, large, 0x1000))
      msg ("validated.");
  else
      fail ("validate fail.");

  CHECK (0x1000 == read (handle, buf, 0x1000), "read \"large.txt\" Page 1");

  msg ("validate page 1.");
  if (!memcmp ((void *) buf, zeros, 0x1000))
      msg ("validated.");
  else
      fail ("validate fail.");
  close (handle);

  msg ("success");
}
