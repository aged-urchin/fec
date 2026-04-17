#include "test.h"
#include "test2.h"
#include "test3.h"

#include <fstream>

int main() {
    system("chcp 65001");
#if 0
    /** redirect standard outputs to null
     */
    std::ofstream nullout("/dev/null");
    std::cout.rdbuf(nullout.rdbuf());
    std::cerr.rdbuf(nullout.rdbuf());
#endif
    TEST2::test();
}
