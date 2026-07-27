#include "App.h"

#include <exception>
#include <iostream>

int main(int, char **)
{
    try {
        kaya::designer::App app;
        if (!app.initialize()) {
            return 1;
        }
        app.run();
        app.shutdown();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Kaya Designer crashed: " << error.what() << "\n";
        return 1;
    }
}

