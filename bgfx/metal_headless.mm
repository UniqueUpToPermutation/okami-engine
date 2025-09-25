#ifdef __APPLE__

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Foundation/Foundation.h>

extern "C" void* createMetalLayerForHeadless(int width, int height) {
    @autoreleasepool {
        CAMetalLayer* metalLayer = [[CAMetalLayer alloc] init];
        
        // Set up the Metal device
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            return nullptr;
        }
        
        metalLayer.device = device;
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.drawableSize = CGSizeMake(width, height);
        
        // Keep the layer alive by retaining it
        return (__bridge_retained void*)metalLayer;
    }
}

extern "C" void releaseMetalLayer(void* layer) {
    if (layer) {
        @autoreleasepool {
            CAMetalLayer* metalLayer = (__bridge_transfer CAMetalLayer*)layer;
            // metalLayer will be released automatically when it goes out of scope
        }
    }
}

#else

extern "C" void* createMetalLayerForHeadless(int width, int height) {
    return nullptr;
}

extern "C" void releaseMetalLayer(void* layer) {
    // No-op on non-Apple platforms
}

#endif