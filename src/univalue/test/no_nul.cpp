#include "univalue.h"

int main (int argc, char *argv[])
{
    char buf[] = "___[1,2,3]___";
    UniValue val;
    return val.read(std::string_view(buf + 3, 7)) ? 0 : 1;
}
