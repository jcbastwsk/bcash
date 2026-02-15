// macOS Cocoa helper for activating the application
#import <Cocoa/Cocoa.h>

extern "C" void OSXActivateApp()
{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
}
