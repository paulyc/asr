#include "ui.h"

#include <string>
#include <iostream>
#include <cstdio>

void CommandlineUI::main_loop()
{
    std::string line;
    while (true)
    {
        std::cin >> line;
        if (line == std::string("q") || line == std::string("quit"))
        {
            break;
        }
    }
}

void CommandlineUI::set_text_field(int id, const wchar_t *txt, bool del)
{
    std::cout << "set text " << id << " " << txt << std::endl;
}

void CommandlineUI::set_clip(int t)
{
    printf("clip %d\n", t);
}

void CommandlineUI::set_position(void *t, double tm, bool invalidate)
{
    
}