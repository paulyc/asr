#include "ui.h"

#include <AppKit/NSOpenPanel.h>

#include <cstdio>

bool FileOpenDialog::OpenSingleFile(std::string &filename)
{
    NSOpenPanel* openDlg = [NSOpenPanel openPanel];
    
    [openDlg setCanChooseFiles:YES];
    [openDlg setCanChooseDirectories:NO];
    [openDlg setAllowsMultipleSelection:NO];
    
    if ([openDlg runModal] == NSOKButton)
    {
        NSArray* files = [openDlg URLs];
        
        for( int i = 0; i < [files count]; i++ )
        {
            NSURL* nsFileUrl = [files objectAtIndex:i];
            NSLog(@"%@", nsFileUrl.path);
            filename = std::string([nsFileUrl.path UTF8String]);
            return true;
        }
    }
    return false;
}
