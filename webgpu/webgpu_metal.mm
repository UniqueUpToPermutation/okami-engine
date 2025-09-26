#ifdef __APPLE__

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Cocoa/Cocoa.h>

extern "C" void* createMetalLayerForWebGPU(void* nsWindow) {
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)nsWindow;
        NSView* contentView = [window contentView];
        
        // Create a CAMetalLayer
        CAMetalLayer* metalLayer = [[CAMetalLayer alloc] init];
        
        // Set up basic Metal device
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device) {
            metalLayer.device = device;
            metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        }
        
        // Set the layer as the content view's layer
        [contentView setWantsLayer:YES];
        [contentView setLayer:metalLayer];
        
        // Return retained reference
        return (__bridge_retained void*)metalLayer;
    }
}

extern "C" void releaseWebGPUMetalLayer(void* layer) {
    if (layer) {
        @autoreleasepool {
            CAMetalLayer* metalLayer = (__bridge_transfer CAMetalLayer*)layer;
            // metalLayer will be released automatically
        }
    }
}

#else

extern "C" void* createMetalLayerForWebGPU(void* nsWindow) {
    return nullptr;
}

extern "C" void releaseWebGPUMetalLayer(void* layer) {
    // No-op on non-Apple platforms
}

#endif