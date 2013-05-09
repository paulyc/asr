#include "ui.h"

#include <string>
#include <iostream>
#include <cstdio>

#include "track.h"

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
        else if (line == std::string("start"))
        {
            _io->Start();
        }
        else if (line == std::string("stop"))
        {
            _io->Stop();
        }
        else if (line == std::string("p1"))
        {
            _io->GetTrack(1)->play_pause(false);
        }
        else if (line == std::string("p2"))
        {
            _io->GetTrack(2)->play_pause(false);
        }
    }
}

void CommandlineUI::set_text_field(int id, const char *txt, bool del)
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