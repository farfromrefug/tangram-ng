#pragma once

namespace Tangram {

// Touch action constants
enum class TouchAction {
    POINTER_1_DOWN = 0,
    POINTER_2_DOWN = 1,
    MOVE = 2,
    CANCEL = 3,
    POINTER_1_UP = 4,
    POINTER_2_UP = 5,
};

// Screen position for touch coordinates
struct ScreenPos {
    float x;
    float y;
    
    ScreenPos() : x(0.f), y(0.f) {}
    ScreenPos(float _x, float _y) : x(_x), y(_y) {}
};

// Touch event listener interface
// Listeners can intercept touch events before they are processed by the default handler
class OnTouchListener {
public:
    virtual ~OnTouchListener() = default;
    
    // Called when a touch event occurs
    // Return true to consume the event and prevent default handling
    // Return false to allow default handling to proceed
    virtual bool onTouchEvent(TouchAction action, const ScreenPos& screenPos1, const ScreenPos& screenPos2) = 0;
};

}
