// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

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