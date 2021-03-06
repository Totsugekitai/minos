#include <stdint.h>
#include <util/util.h>

extern uint64_t milli_clock;
extern char output[];

void uptime(int argc, char *argv[])
{
    if (argc == 1) {
        char clockstr[19];
        u64_to_hexstr(clockstr, milli_clock);
        sprintf(clockstr, output);
    } else {
        sprintf("uptime: bad args", output);
    }
}
