#pragma once

#include <memory>
#include <functional>

namespace Tangram {

// Screen position for touch coordinates
struct ScreenPos {
    float x;
    float y;
    
    ScreenPos() : x(0.f), y(0.f) {}
    ScreenPos(float _x, float _y) : x(_x), y(_y) {}
};

// Map click listener interface
// Called when the user performs a single tap on the map
class MapClickListener {
public:
    virtual ~MapClickListener() = default;
    
    // Called when a single tap occurs
    // Return true to consume the event and prevent default behavior (centering)
    // Return false to allow default handling
    virtual bool onMapClick(float x, float y) = 0;
};

// Map interaction listener interface  
// Called when the user is interacting with the map (panning, zooming, rotating, tilting)
class MapInteractionListener {
public:
    virtual ~MapInteractionListener() = default;
    
    // Called when map interaction starts
    // Return true to consume all interaction events and prevent default behavior
    // Return false to allow default handling
    virtual bool onMapInteraction(bool isPanning, bool isZooming, bool isRotating, bool isTilting) = 0;
};

}
