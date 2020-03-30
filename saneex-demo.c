#include <stdio.h>
#include "saneex.h"

int main(void) {
  sxTag = "SaneC's Exceptions Demo";

  try {
    printf("Enter a message to fail with: [] [1] [2] [!] ");

    char msg[SX_MAX_TRACE_STRING];
    thrif(!fgets(msg, sizeof(msg), stdin), "fgets() error");

    // Remove trailing line breaks and spaces.
    int i = strlen(msg) - 1;
    while (i >= 0 && msg[i] <= ' ') { msg[i--] = 0; }

    if (msg[0]) {
      errno = atoi(msg);
      struct SxTraceEntry e = newex();
      e = sxprintf(e, "Your message: %s", msg);
      e.uncatchable = msg[0] == '!';
      throw(e);
    }

    puts("End of try body");

  } catch (1) {
    puts("Caught in catch (1)");
    sxPrintTrace();

  } catch (2) {
    puts("Caught in catch (2)");
    errno = 123;
    rethrow(msgex("calling rethrow() with code 123"));

  } catchall {
    printf("Caught in catchall, message is: %s\n", curex().message);

  } finally {
    puts("Now in finally");

  } endtry

  puts("End of main()");
}
