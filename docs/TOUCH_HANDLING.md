# Touch Handling Migration Guide

This document describes the new unified touch handling system available in tangram-ng and how to migrate from the old gesture-based system.

## Overview

The new touch handling system provides a unified, cross-platform approach to handling touch input that works consistently across iOS and Android. It is modeled after the Carto Mobile SDK's approach and replaces the platform-specific gesture recognizer systems.

### Key Benefits

1. **Cross-platform consistency**: iOS and Android now handle touches identically
2. **Easier maintenance**: Touch logic is centralized in C++ rather than duplicated across platforms
3. **Better control**: Direct access to raw touch events allows for more sophisticated gesture detection
4. **Backward compatible**: Existing apps continue to work without changes

## Architecture

The new system consists of three main components:

1. **TouchHandler (C++)**: Core gesture state machine that processes touch events
2. **Platform bridges**: Android (Java) and iOS (Objective-C) code that captures touch events and forwards them to C++
3. **Toggle mechanism**: Allows applications to opt-in to the new system

### Touch Actions

The system defines 6 touch action types:

- `POINTER_1_DOWN` (0): First finger touches the screen
- `POINTER_2_DOWN` (1): Second finger touches the screen
- `MOVE` (2): One or both fingers move
- `CANCEL` (3): Touch sequence is canceled
- `POINTER_1_UP` (4): First finger is lifted
- `POINTER_2_UP` (5): Second finger is lifted

### Gesture Modes

The TouchHandler implements a state machine with these gesture modes:

- `SINGLE_POINTER_CLICK_GUESS`: Waiting to determine if single tap
- `DUAL_POINTER_CLICK_GUESS`: Waiting to determine if double tap
- `SINGLE_POINTER_PAN`: Panning with one finger
- `SINGLE_POINTER_ZOOM`: Zooming with double-tap-drag
- `DUAL_POINTER_GUESS`: Determining dual-pointer gesture type
- `DUAL_POINTER_TILT`: Tilting the map
- `DUAL_POINTER_ROTATE`: Rotating the map
- `DUAL_POINTER_SCALE`: Scaling/pinching the map
- `DUAL_POINTER_FREE`: Free rotation and scaling simultaneously

## Migration Guide

### Android

#### Before (Old System)
```java
MapView mapView = findViewById(R.id.map);
mapView.getMapAsync(new MapView.MapReadyCallback() {
    @Override
    public void onMapReady(@Nullable MapController mapController) {
        // Map uses gesture recognizers automatically
    }
});
```

#### After (New System)
```java
MapView mapView = findViewById(R.id.map);
// Enable new touch handling
mapView.setNewTouchHandlingEnabled(true);

mapView.getMapAsync(new MapView.MapReadyCallback() {
    @Override
    public void onMapReady(@Nullable MapController mapController) {
        // Map now uses unified touch handling
    }
});
```

### iOS

#### Before (Old System)
```objective-c
TGMapView *mapView = [[TGMapView alloc] initWithFrame:self.view.bounds];
// Map uses gesture recognizers automatically
[self.view addSubview:mapView];
```

#### After (New System)
```objective-c
TGMapView *mapView = [[TGMapView alloc] initWithFrame:self.view.bounds];
// Enable new touch handling
mapView.useNewTouchHandling = YES;
[self.view addSubview:mapView];
```

Or in Swift:
```swift
let mapView = TGMapView(frame: view.bounds)
// Enable new touch handling
mapView.useNewTouchHandling = true
view.addSubview(mapView)
```

### C++ Applications

C++ applications can access the TouchHandler directly if needed:

```cpp
// The Map class now has both InputHandler and TouchHandler
Tangram::Map map(std::make_unique<MyPlatform>());

// Use the new touch handling system
map.handleTouchEvent(
    0,  // POINTER_1_DOWN
    x, y,  // First pointer position
    -1, -1  // No second pointer (-1 = NATIVE_NO_COORDINATE)
);
```

However, for C++ applications, the existing gesture methods remain available and are recommended:

```cpp
map.handleTapGesture(x, y);
map.handlePanGesture(startX, startY, endX, endY);
map.handlePinchGesture(x, y, scale, velocity);
// etc.
```

## Implementation Details

### Android Pointer Tracking

The Android implementation tracks pointer IDs to handle multi-touch correctly:

```java
private int pointer1Id = INVALID_POINTER_ID;
private int pointer2Id = INVALID_POINTER_ID;
```

When a pointer goes down, its ID is tracked. When it moves or goes up, the implementation uses `findPointerIndex()` to locate it in the current event.

### iOS Touch Tracking

The iOS implementation tracks UITouch objects directly:

```objective-c
UITouch *_pointer1;
UITouch *_pointer2;
```

Touch events are matched by comparing touch objects to determine which pointer changed.

## Backward Compatibility

The new system is designed to be 100% backward compatible:

1. **Default behavior unchanged**: By default, both platforms continue using gesture recognizers
2. **Opt-in migration**: Applications explicitly enable the new system via API calls
3. **C++ API unchanged**: Existing C++ applications work without modifications
4. **Gradual migration**: Apps can migrate platform-by-platform or all at once

## Testing Recommendations

When migrating to the new touch handling:

1. **Test all gestures**: Verify tap, double-tap, pan, pinch, rotate, tilt work as expected
2. **Test edge cases**: Multi-finger transitions, rapid taps, simultaneous gestures
3. **Performance testing**: Ensure smooth 60fps during pan and zoom
4. **Cross-platform comparison**: Verify iOS and Android behave identically

## Known Limitations

1. **Double-tap zoom**: Some advanced double-tap zoom features are not yet fully implemented in TouchHandler
2. **Custom gestures**: Applications using custom gesture recognizers may need additional work to migrate

## Future Enhancements

Potential future improvements to the touch handling system:

1. Configurable gesture thresholds (pan distance, tap timeout, etc.)
2. Gesture prioritization and conflict resolution options
3. Velocity tracking for more accurate kinetic scrolling
4. Support for 3+ simultaneous touch points

## Support

For questions or issues related to touch handling:

1. Check the [tangram-ng GitHub repository](https://github.com/farfromrefug/tangram-ng)
2. Review existing InputHandler code for reference
3. Consult Carto Mobile SDK documentation for design rationale
