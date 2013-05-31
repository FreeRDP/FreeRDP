//
//  main.m
//  MacClient2
//
//  Created by Benoît et Kathy on 2013-05-08.
//
//

#import <Cocoa/Cocoa.h>
#import <MacFreeRDP-library/MRDPView.h>

int main(int argc, char *argv[])
{
    [MRDPView class];
    return NSApplicationMain(argc, (const char **)argv);
}
