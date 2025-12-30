#pragma once

#include "view/view.h"
#include "util/touchListener.h"
#include "glm/vec2.hpp"

#include <memory>
#include <chrono>
#include <vector>
#include <mutex>

namespace Tangram {

// Touch action constants for internal use
enum class TouchAction {
    POINTER_1_DOWN = 0,
    POINTER_2_DOWN = 1,
    MOVE = 2,
    CANCEL = 3,
    POINTER_1_UP = 4,
    POINTER_2_UP = 5,
};

// Gesture mode states
enum class GestureMode {
    SINGLE_POINTER_CLICK_GUESS = 0,
    DUAL_POINTER_CLICK_GUESS,
    SINGLE_POINTER_PAN,
    SINGLE_POINTER_ZOOM,
    DUAL_POINTER_GUESS,
    DUAL_POINTER_TILT,
    DUAL_POINTER_ROTATE,
    DUAL_POINTER_SCALE,
    DUAL_POINTER_FREE,
};

// Forward declaration
class Map;

// Panning mode for dual pointer gestures (following Carto Mobile SDK)
enum class PanningMode {
    FREE = 0,          // Allows simultaneous rotation and scaling
    STICKY = 1,        // Separates rotate and scale gestures, but allows switching
    STICKY_FINAL = 2,  // Locks to first detected gesture until fingers lift
};

class TouchHandler {
public:
    explicit TouchHandler(View& _view, Map* _map = nullptr);

    // Main touch event handler - similar to Carto Mobile SDK
    // Returns true if the event was handled
    bool onTouchEvent(TouchAction action, const ScreenPos& screenPos1, const ScreenPos& screenPos2);

    // Update method for kinetic animations
    bool update(float _dt);

    // Cancel ongoing gestures
    void cancel();

    void setView(View& _view) { m_view = _view; }
    void setMap(Map* _map) { m_map = _map; }
    
    // Add/remove map click listeners
    void setMapClickListener(std::shared_ptr<MapClickListener> listener);
    
    // Add/remove map interaction listeners
    void setMapInteractionListener(std::shared_ptr<MapInteractionListener> listener);
    
    // Enable/disable gestures
    void setGesturesEnabled(bool zoom, bool pan, bool doubleTap, bool doubleTapDrag, bool tilt, bool rotate);
    void setZoomEnabled(bool enabled) { m_zoomEnabled = enabled; }
    void setPanEnabled(bool enabled) { m_panEnabled = enabled; }
    void setDoubleTapEnabled(bool enabled) { m_doubleTapEnabled = enabled; }
    void setDoubleTapDragEnabled(bool enabled) { m_doubleTapDragEnabled = enabled; }
    void setTiltEnabled(bool enabled) { m_tiltEnabled = enabled; }
    void setRotateEnabled(bool enabled) { m_rotateEnabled = enabled; }
    
    bool isZoomEnabled() const { return m_zoomEnabled; }
    bool isPanEnabled() const { return m_panEnabled; }
    bool isDoubleTapEnabled() const { return m_doubleTapEnabled; }
    bool isDoubleTapDragEnabled() const { return m_doubleTapDragEnabled; }
    bool isTiltEnabled() const { return m_tiltEnabled; }
    bool isRotateEnabled() const { return m_rotateEnabled; }
    
    // Get/set DPI for gesture calculations
    void setDpi(float dpi) { m_dpi = dpi; }
    float getDpi() const { return m_dpi; }
    
    // Get/set panning mode
    void setPanningMode(PanningMode mode) { m_panningMode = mode; }
    PanningMode getPanningMode() const { return m_panningMode; }

private:
    // Gesture detection and handling methods
    void singlePointerPan(const ScreenPos& screenPos, View& viewState);
    void singlePointerZoom(const ScreenPos& screenPos, View& viewState);
    void startSinglePointerZoom(const ScreenPos& screenPos);
    bool singlePointerZoomStop(const ScreenPos& screenPos, View& viewState);
    void doubleTapZoom(const ScreenPos& screenPos, View& viewState);
    void handleSingleTap(const ScreenPos& screenPos);
    void handleDoubleTap(const ScreenPos& screenPos);
    void handleLongPress(const ScreenPos& screenPos);
    void handleDualTap(const ScreenPos& screenPos1, const ScreenPos& screenPos2);
    
    void startDualPointer(const ScreenPos& screenPos1, const ScreenPos& screenPos2);
    void dualPointerGuess(const ScreenPos& screenPos1, const ScreenPos& screenPos2, View& viewState);
    void dualPointerPan(const ScreenPos& screenPos1, const ScreenPos& screenPos2, 
                       bool rotate, bool scale, View& viewState);
    void dualPointerTilt(const ScreenPos& screenPos1, View& viewState);
    
    float calculateRotatingScalingFactor(const ScreenPos& screenPos1, const ScreenPos& screenPos2);
    
    glm::vec2 getTranslation(float _startX, float _startY, float _endX, float _endY);
    void setVelocity(float _zoom, glm::vec2 _pan);

    View& m_view;
    Map* m_map;
    
    // DPI for gesture calculations (default 160)
    float m_dpi;
    
    // Panning mode for dual pointer gestures
    PanningMode m_panningMode;
    
    // Map event listeners
    std::shared_ptr<MapClickListener> m_mapClickListener;
    std::shared_ptr<MapInteractionListener> m_mapInteractionListener;
    std::mutex m_listenersMutex;
    
    // Gesture enable/disable flags
    bool m_zoomEnabled;
    bool m_panEnabled;
    bool m_doubleTapEnabled;
    bool m_doubleTapDragEnabled;
    bool m_tiltEnabled;
    bool m_rotateEnabled;
    
    // State tracking
    GestureMode m_gestureMode;
    int m_pointersDown;
    bool m_noDualPointerYet;
    bool m_interactionConsumed; // Track if listener consumed the interaction
    
    // Previous positions for gesture tracking
    ScreenPos m_prevScreenPos1;
    ScreenPos m_prevScreenPos2;
    ScreenPos m_firstTapPos;
    ScreenPos m_doubleTapStartPos; // Position where double tap started (for zooming)
    
    // Swipe tracking for dual pointer guess
    glm::vec2 m_swipe1;
    glm::vec2 m_swipe2;
    
    // Timing for gesture detection
    std::chrono::steady_clock::time_point m_dualPointerReleaseTime;
    std::chrono::steady_clock::time_point m_firstTapTime;
    std::chrono::steady_clock::time_point m_pointer1DownTime;
    
    // Zoom tracking for double-tap-and-drag
    float m_singlePointerZoomStartZoom;
    
    // Velocity for kinetic scrolling
    glm::vec2 m_velocityPan;
    float m_velocityZoom;
    
    // Constants for gesture detection
    static constexpr float ROTATION_SCALING_FACTOR_THRESHOLD_STICKY = 0.3f;
    static constexpr float ROTATION_SCALING_FACTOR_THRESHOLD_FREE = 0.05f;
    static constexpr std::chrono::milliseconds DUAL_STOP_HOLD_DURATION{500};
    static constexpr std::chrono::milliseconds DUAL_KINETIC_HOLD_DURATION{200};
    static constexpr std::chrono::milliseconds DOUBLE_TAP_TIMEOUT{300};
    static constexpr std::chrono::milliseconds LONG_PRESS_TIMEOUT{500};
    static constexpr float TAP_MOVEMENT_THRESHOLD_INCHES = 0.1f; // inches instead of pixels
    
    // Constants for dual pointer guess algorithm (following Carto)
    static constexpr float GUESS_MAX_DELTA_Y_INCHES = 1.0f;
    static constexpr float GUESS_MIN_SWIPE_LENGTH_SAME_INCHES = 0.1f;
    static constexpr float GUESS_MIN_SWIPE_LENGTH_OPPOSITE_INCHES = 0.075f;
    static constexpr float DEFAULT_DPI = 160.0f; // Fallback DPI if not available
    static constexpr float SCALING_FACTOR_THRESHOLD = 0.5f;
    static constexpr float ROTATION_FACTOR_THRESHOLD = 0.75f; // make rotation harder to trigger compared to scaling
};

}
