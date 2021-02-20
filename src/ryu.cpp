#include <iostream>

#include "ryu/app.h"
#include <uv.h>

int main(int argc, char* argv[]) {
    ryu::App app(uv_default_loop());
    return app.Run().Expect("Application failure");
}