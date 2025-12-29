#include "util/touchHandler.h"
#include "util/touchListener.h"
#include "glm/gtx/rotate_vector.hpp"
#include <cmath>
#include <algorithm>

// Damping factor for translation; reciprocal of the decay period in seconds
#define DAMPING_PAN 4.0f

// Damping factor for zoom; reciprocal of the decay period in seconds
#define DAMPING_ZOOM 6.0f

// Minimum translation at which momentum should start (pixels per second)
#define THRESHOLD_START_PAN 350.f

// Minimum translation at which momentum should stop (pixels per second)
#define THRESHOLD_STOP_PAN 24.f

// Minimum zoom at which momentum should start (zoom levels per second)
#define THRESHOLD_START_ZOOM 1.f

// Minimum zoom at which momentum should stop (zoom levels per second)
#define THRESHOLD_STOP_ZOOM 0.3f

// No coordinate constant
#define NATIVE_NO_COORDINATE -1.0f

// Maximum pitch angle for pan limiting (degrees)
#define MAX_PITCH_FOR_PAN_LIMITING 75.0f

// Zoom sensitivity for single pointer zoom (zoom units per pixel)
#define SINGLE_POINTER_ZOOM_SENSITIVITY 0.005f

namespace Tangram {

TouchHandler::TouchHandler(View& _view)
    : m_view(_view),
      m_gestureMode(GestureMode::SINGLE_POINTER_CLICK_GUESS),
      m_pointersDown(0),
      m_noDualPointerYet(true),
      m_interactionConsumed(false),
      m_singlePointerZoomStartZoom(0.f),
      m_velocityPan(0.f, 0.f),
      m_velocityZoom(0.f),
      m_dualPointerReleaseTime(std::chrono::steady_clock::now()),
      m_firstTapTime(std::chrono::steady_clock::now()),
      m_pointer1DownTime(std::chrono::steady_clock::now()) {
}

bool TouchHandler::update(float _dt) {
    auto velocityPanPixels = m_view.pixelsPerMeter() / m_view.pixelScale() * m_velocityPan;

    bool isFlinging = glm::length(velocityPanPixels) > THRESHOLD_STOP_PAN ||
                      std::abs(m_velocityZoom) > THRESHOLD_STOP_ZOOM;

    if (isFlinging) {
        m_velocityPan -= std::min(_dt * DAMPING_PAN, 1.f) * m_velocityPan;
        m_view.translate(_dt * m_velocityPan.x, _dt * m_velocityPan.y);

        m_velocityZoom -= std::min(_dt * DAMPING_ZOOM, 1.f) * m_velocityZoom;
        m_view.zoom(m_velocityZoom * _dt);
    }

    return isFlinging;
}

void TouchHandler::cancel() {
    setVelocity(0.f, glm::vec2(0.f, 0.f));
    m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
    m_pointersDown = 0;
    m_interactionConsumed = false;
}

void TouchHandler::setMapClickListener(std::shared_ptr<MapClickListener> listener) {
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    m_mapClickListener = listener;
}

void TouchHandler::setMapInteractionListener(std::shared_ptr<MapInteractionListener> listener) {
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    m_mapInteractionListener = listener;
}

glm::vec2 TouchHandler::getTranslation(float _startX, float _startY, float _endX, float _endY) {
    float elev = 0;
    m_view.screenPositionToLngLat(_startX, _startY, &elev);

    auto start = m_view.screenToGroundPlane(_startX, _startY, elev);
    auto end = m_view.screenToGroundPlane(_endX, _endY, elev);

    glm::vec2 dr = start - end;

    // prevent extreme panning when view is nearly horizontal
    if (m_view.getPitch() > MAX_PITCH_FOR_PAN_LIMITING * M_PI / 180) {
        float dpx = glm::length(glm::vec2(_startX - _endX, _startY - _endY)) / m_view.pixelsPerMeter();
        float dd = glm::length(dr);
        if (dd > dpx) {
            dr = dr * dpx / dd;
        }
    }
    return dr;
}

void TouchHandler::setVelocity(float _zoom, glm::vec2 _translate) {
    m_velocityPan = _translate;
    m_velocityZoom = _zoom;
}

void TouchHandler::singlePointerPan(const ScreenPos& screenPos, View& viewState) {
    glm::vec2 translation = getTranslation(m_prevScreenPos1.x, m_prevScreenPos1.y, 
                                          screenPos.x, screenPos.y);
    m_view.translate(translation);
    m_prevScreenPos1 = screenPos;
}

void TouchHandler::startSinglePointerZoom(const ScreenPos& screenPos) {
    m_singlePointerZoomStartZoom = m_view.getZoom();
    m_prevScreenPos1 = screenPos;
    m_gestureMode = GestureMode::SINGLE_POINTER_ZOOM;
}

void TouchHandler::singlePointerZoom(const ScreenPos& screenPos, View& viewState) {
    // Implement single pointer zoom (double-tap and drag)
    float deltaY = screenPos.y - m_prevScreenPos1.y;
    float zoomDelta = -deltaY * SINGLE_POINTER_ZOOM_SENSITIVITY;
    
    // Get the fixed point for zooming (at the center of the screen or tap point)
    float elev;
    m_view.screenPositionToLngLat(m_prevScreenPos1.x, m_prevScreenPos1.y, &elev);
    auto start = m_view.screenToGroundPlane(m_prevScreenPos1.x, m_prevScreenPos1.y, elev);
    
    m_view.zoom(zoomDelta);
    
    auto end = m_view.screenToGroundPlane(m_prevScreenPos1.x, m_prevScreenPos1.y, elev);
    m_view.translate(start - end);
    
    m_prevScreenPos1 = screenPos;
}

bool TouchHandler::singlePointerZoomStop(const ScreenPos& screenPos, View& viewState) {
    // Return false - double-tap zoom is handled separately now
    return false;
}

void TouchHandler::handleSingleTap(const ScreenPos& screenPos) {
    // Call map click listener first
    {
        std::lock_guard<std::mutex> lock(m_listenersMutex);
        if (m_mapClickListener) {
            bool consumed = m_mapClickListener->onMapClick(ClickType::SINGLE, screenPos.x, screenPos.y);
            if (consumed) {
                return; // Listener consumed the click
            }
        }
    }
    
    // Default behavior: Pan to center the tapped location
    float viewCenterX = 0.5f * m_view.getWidth();
    float viewCenterY = 0.5f * m_view.getHeight();

    auto center = m_view.screenToGroundPlane(viewCenterX, viewCenterY);
    auto pos = m_view.screenToGroundPlane(screenPos.x, screenPos.y);

    m_view.translate(pos - center);
}

void TouchHandler::handleDoubleTap(const ScreenPos& screenPos) {
    // Call map click listener first
    {
        std::lock_guard<std::mutex> lock(m_listenersMutex);
        if (m_mapClickListener) {
            bool consumed = m_mapClickListener->onMapClick(ClickType::DOUBLE, screenPos.x, screenPos.y);
            if (consumed) {
                return; // Listener consumed the click
            }
        }
    }
    
    // Default behavior: zoom in
    doubleTapZoom(screenPos, m_view);
}

void TouchHandler::handleLongPress(const ScreenPos& screenPos) {
    // Call map click listener for long press
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    if (m_mapClickListener) {
        m_mapClickListener->onMapClick(ClickType::LONG, screenPos.x, screenPos.y);
        // Note: Long press has no default behavior, just notify listener
    }
}

void TouchHandler::handleDualTap(const ScreenPos& screenPos1, const ScreenPos& screenPos2) {
    // Call map click listener for dual tap
    // Use the midpoint of the two taps
    float x = (screenPos1.x + screenPos2.x) / 2.0f;
    float y = (screenPos1.y + screenPos2.y) / 2.0f;
    
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    if (m_mapClickListener) {
        m_mapClickListener->onMapClick(ClickType::DUAL, x, y);
        // Note: Dual tap has no default behavior, just notify listener
    }
}

void TouchHandler::doubleTapZoom(const ScreenPos& screenPos, View& viewState) {
    setVelocity(0.f, glm::vec2(0.f, 0.f));
    
    float elev;
    m_view.screenPositionToLngLat(screenPos.x, screenPos.y, &elev);
    auto start = m_view.screenToGroundPlane(screenPos.x, screenPos.y, elev);
    
    // Zoom in by 2x
    m_view.zoom(std::log2(2.f));
    
    auto end = m_view.screenToGroundPlane(screenPos.x, screenPos.y, elev);
    m_view.translate(start - end);
}

void TouchHandler::startDualPointer(const ScreenPos& screenPos1, const ScreenPos& screenPos2) {
    m_prevScreenPos1 = screenPos1;
    m_prevScreenPos2 = screenPos2;
    m_gestureMode = GestureMode::DUAL_POINTER_GUESS;
}

void TouchHandler::dualPointerGuess(const ScreenPos& screenPos1, const ScreenPos& screenPos2, View& viewState) {
    // Simple implementation: just switch to dual pointer free mode
    m_gestureMode = GestureMode::DUAL_POINTER_FREE;
    m_prevScreenPos1 = screenPos1;
    m_prevScreenPos2 = screenPos2;
}

void TouchHandler::dualPointerPan(const ScreenPos& screenPos1, const ScreenPos& screenPos2,
                                 bool rotate, bool scale, View& viewState) {
    // Calculate center points
    ScreenPos prevCenter((m_prevScreenPos1.x + m_prevScreenPos2.x) * 0.5f,
                        (m_prevScreenPos1.y + m_prevScreenPos2.y) * 0.5f);
    ScreenPos currCenter((screenPos1.x + screenPos2.x) * 0.5f,
                        (screenPos1.y + screenPos2.y) * 0.5f);
    
    // Apply pan
    glm::vec2 translation = getTranslation(prevCenter.x, prevCenter.y, currCenter.x, currCenter.y);
    m_view.translate(translation);
    
    if (scale) {
        // Calculate scale factor
        float prevDist = std::sqrt(std::pow(m_prevScreenPos2.x - m_prevScreenPos1.x, 2) +
                                  std::pow(m_prevScreenPos2.y - m_prevScreenPos1.y, 2));
        float currDist = std::sqrt(std::pow(screenPos2.x - screenPos1.x, 2) +
                                  std::pow(screenPos2.y - screenPos1.y, 2));
        
        if (prevDist > 0.f && currDist > 0.f) {
            float scaleFactor = currDist / prevDist;
            
            float elev;
            m_view.screenPositionToLngLat(currCenter.x, currCenter.y, &elev);
            auto start = m_view.screenToGroundPlane(currCenter.x, currCenter.y, elev);
            
            m_view.zoom(std::log2(scaleFactor));
            
            auto end = m_view.screenToGroundPlane(currCenter.x, currCenter.y, elev);
            m_view.translate(start - end);
        }
    }
    
    if (rotate) {
        // Calculate rotation angle
        float prevAngle = std::atan2(m_prevScreenPos2.y - m_prevScreenPos1.y,
                                    m_prevScreenPos2.x - m_prevScreenPos1.x);
        float currAngle = std::atan2(screenPos2.y - screenPos1.y,
                                    screenPos2.x - screenPos1.x);
        float rotation = currAngle - prevAngle;
        
        float elev = 0;
        m_view.screenPositionToLngLat(currCenter.x, currCenter.y, &elev);
        glm::vec2 offset = m_view.screenToGroundPlane(currCenter.x, currCenter.y, elev);
        
        glm::vec2 translation_rot = offset - glm::rotate(offset, rotation);
        m_view.translate(translation_rot);
        m_view.yaw(rotation);
    }
    
    m_prevScreenPos1 = screenPos1;
    m_prevScreenPos2 = screenPos2;
}

void TouchHandler::dualPointerTilt(const ScreenPos& screenPos1, View& viewState) {
    // Simple tilt implementation
    float deltaY = screenPos1.y - m_prevScreenPos1.y;
    float angle = -M_PI * deltaY / m_view.getHeight();
    
    float maxpitch = std::min(75.f * float(M_PI / 180.0), m_view.getMaxPitch());
    float pitch0 = glm::clamp(m_view.getPitch(), 0.f, maxpitch);
    float pitch1 = glm::clamp(m_view.getPitch() + angle, 0.f, maxpitch);
    
    m_view.pitch(pitch1 - pitch0);
    m_prevScreenPos1 = screenPos1;
}

float TouchHandler::calculateRotatingScalingFactor(const ScreenPos& screenPos1, const ScreenPos& screenPos2) {
    // Calculate the factor to determine if rotating or scaling gesture
    // Positive values indicate rotation, negative values indicate scaling
    // This is a simplified implementation - could be enhanced with more sophisticated detection
    
    float prevDist = std::sqrt(std::pow(m_prevScreenPos2.x - m_prevScreenPos1.x, 2) +
                              std::pow(m_prevScreenPos2.y - m_prevScreenPos1.y, 2));
    float currDist = std::sqrt(std::pow(screenPos2.x - screenPos1.x, 2) +
                              std::pow(screenPos2.y - screenPos1.y, 2));
    
    if (prevDist < 1.0f || currDist < 1.0f) {
        return 0.0f;
    }
    
    // Calculate angle change
    float prevAngle = std::atan2(m_prevScreenPos2.y - m_prevScreenPos1.y,
                                m_prevScreenPos2.x - m_prevScreenPos1.x);
    float currAngle = std::atan2(screenPos2.y - screenPos1.y,
                                screenPos2.x - screenPos1.x);
    float angleChange = std::abs(currAngle - prevAngle);
    
    // Calculate scale change  
    float scaleChange = std::abs(currDist - prevDist) / prevDist;
    
    // Return positive for rotation-dominant, negative for scale-dominant
    if (angleChange > scaleChange * 2.0f) {
        return angleChange;
    } else if (scaleChange > angleChange * 2.0f) {
        return -scaleChange;
    }
    
    return 0.0f;
}

void TouchHandler::onTouchEvent(TouchAction action, const ScreenPos& screenPos1, const ScreenPos& screenPos2) {
    View& viewState = m_view;
    auto now = std::chrono::steady_clock::now();
    
    switch (action) {
    case TouchAction::POINTER_1_DOWN:
        m_pointer1DownTime = now;
        m_noDualPointerYet = true;
        m_interactionConsumed = false;
        setVelocity(0.f, glm::vec2(0.f, 0.f));
        m_prevScreenPos1 = screenPos1;
        
        // Check for double-tap
        auto timeSinceFirstTap = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_firstTapTime);
        float distFromFirstTap = std::sqrt(
            std::pow(screenPos1.x - m_firstTapPos.x, 2) + 
            std::pow(screenPos1.y - m_firstTapPos.y, 2)
        );
        
        if (timeSinceFirstTap < DOUBLE_TAP_TIMEOUT && 
            distFromFirstTap < TAP_MOVEMENT_THRESHOLD &&
            m_gestureMode == GestureMode::SINGLE_POINTER_CLICK_GUESS) {
            // This is a double-tap - start zoom mode
            // Call interaction listener for zooming
            {
                std::lock_guard<std::mutex> lock(m_listenersMutex);
                if (m_mapInteractionListener) {
                    m_interactionConsumed = m_mapInteractionListener->onMapInteraction(false, true, false, false);
                }
            }
            if (!m_interactionConsumed) {
                startSinglePointerZoom(screenPos1);
            } else {
                m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
            }
        } else {
            // Start tracking for potential first tap
            m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
            m_firstTapTime = now;
            m_firstTapPos = screenPos1;
        }
        break;
        
    case TouchAction::POINTER_2_DOWN:
        m_noDualPointerYet = false;
        switch (m_gestureMode) {
        case GestureMode::SINGLE_POINTER_CLICK_GUESS:
            m_gestureMode = GestureMode::DUAL_POINTER_CLICK_GUESS;
            break;
        case GestureMode::SINGLE_POINTER_PAN:
        case GestureMode::SINGLE_POINTER_ZOOM:
            startDualPointer(screenPos1, screenPos2);
            break;
        default:
            break;
        }
        break;
        
    case TouchAction::MOVE:
        // Skip if interaction was consumed
        if (m_interactionConsumed) {
            break;
        }
        
        switch (m_gestureMode) {
        case GestureMode::SINGLE_POINTER_CLICK_GUESS:
            {
                // Check if moved too far to be a tap
                float dist = std::sqrt(
                    std::pow(screenPos1.x - m_prevScreenPos1.x, 2) + 
                    std::pow(screenPos1.y - m_prevScreenPos1.y, 2)
                );
                if (dist > TAP_MOVEMENT_THRESHOLD) {
                    // Transition to pan - call interaction listener
                    {
                        std::lock_guard<std::mutex> lock(m_listenersMutex);
                        if (m_mapInteractionListener) {
                            m_interactionConsumed = m_mapInteractionListener->onMapInteraction(true, false, false, false);
                        }
                    }
                    if (!m_interactionConsumed) {
                        m_gestureMode = GestureMode::SINGLE_POINTER_PAN;
                        m_prevScreenPos1 = screenPos1;
                    }
                }
            }
            break;
        case GestureMode::DUAL_POINTER_CLICK_GUESS:
            {
                // Transition to dual pointer - call interaction listener  
                std::lock_guard<std::mutex> lock(m_listenersMutex);
                if (m_mapInteractionListener) {
                    // We don't know yet what type of gesture it will be
                    m_interactionConsumed = m_mapInteractionListener->onMapInteraction(true, true, true, true);
                }
            }
            if (!m_interactionConsumed) {
                m_gestureMode = GestureMode::DUAL_POINTER_GUESS;
                m_prevScreenPos1 = screenPos1;
                m_prevScreenPos2 = screenPos2;
            }
            break;
        case GestureMode::SINGLE_POINTER_PAN:
            {
                auto deltaTime = std::chrono::steady_clock::now() - m_dualPointerReleaseTime;
                if (deltaTime >= DUAL_STOP_HOLD_DURATION) {
                    singlePointerPan(screenPos1, viewState);
                }
            }
            break;
        case GestureMode::SINGLE_POINTER_ZOOM:
            singlePointerZoom(screenPos1, viewState);
            break;
        case GestureMode::DUAL_POINTER_GUESS:
            dualPointerGuess(screenPos1, screenPos2, viewState);
            break;
        case GestureMode::DUAL_POINTER_TILT:
            dualPointerTilt(screenPos1, viewState);
            break;
        case GestureMode::DUAL_POINTER_ROTATE:
        case GestureMode::DUAL_POINTER_SCALE:
            dualPointerPan(screenPos1, screenPos2, 
                         m_gestureMode == GestureMode::DUAL_POINTER_ROTATE,
                         m_gestureMode == GestureMode::DUAL_POINTER_SCALE, viewState);
            break;
        case GestureMode::DUAL_POINTER_FREE:
            dualPointerPan(screenPos1, screenPos2, true, true, viewState);
            break;
        }
        break;
        
    case TouchAction::CANCEL:
        m_pointersDown = 0;
        m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
        setVelocity(0.f, glm::vec2(0.f, 0.f));
        break;
        
    case TouchAction::POINTER_1_UP:
        {
            auto tapDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_pointer1DownTime);
            float moveDist = std::sqrt(
                std::pow(screenPos1.x - m_prevScreenPos1.x, 2) + 
                std::pow(screenPos1.y - m_prevScreenPos1.y, 2)
            );
            
            switch (m_gestureMode) {
            case GestureMode::SINGLE_POINTER_CLICK_GUESS:
                // This was a tap - check if it qualifies as a click or long press
                if (moveDist < TAP_MOVEMENT_THRESHOLD) {
                    if (tapDuration >= LONG_PRESS_TIMEOUT) {
                        // Long press
                        handleLongPress(screenPos1);
                    } else if (tapDuration < DOUBLE_TAP_TIMEOUT) {
                        // Single tap
                        handleSingleTap(screenPos1);
                    }
                }
                m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
                break;
            case GestureMode::DUAL_POINTER_CLICK_GUESS:
                m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
                break;
            case GestureMode::SINGLE_POINTER_PAN:
                m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
                m_interactionConsumed = false;
                if (m_noDualPointerYet) {
                    // Start kinetic pan
                    setVelocity(0.f, m_velocityPan);
                }
                break;
            case GestureMode::SINGLE_POINTER_ZOOM:
                // Finger lifted after double-tap zoom or during drag zoom
                // If it was a quick tap without much movement, do an instant zoom
                if (tapDuration < DOUBLE_TAP_TIMEOUT && moveDist < TAP_MOVEMENT_THRESHOLD) {
                    handleDoubleTap(screenPos1);
                }
                m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
                m_interactionConsumed = false;
                if (m_noDualPointerYet) {
                    setVelocity(m_velocityZoom, glm::vec2(0.f, 0.f));
                }
                break;
            case GestureMode::DUAL_POINTER_GUESS:
            case GestureMode::DUAL_POINTER_TILT:
            case GestureMode::DUAL_POINTER_ROTATE:
            case GestureMode::DUAL_POINTER_SCALE:
            case GestureMode::DUAL_POINTER_FREE:
                m_dualPointerReleaseTime = std::chrono::steady_clock::now();
                m_prevScreenPos1 = screenPos2;
                m_gestureMode = GestureMode::SINGLE_POINTER_PAN;
                break;
            }
        }
        break;
        
    case TouchAction::POINTER_2_UP:
        switch (m_gestureMode) {
        case GestureMode::DUAL_POINTER_CLICK_GUESS:
            {
                // Check if both fingers went down and up quickly = dual tap
                auto tapDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_pointer1DownTime);
                if (tapDuration < DOUBLE_TAP_TIMEOUT) {
                    handleDualTap(m_prevScreenPos1, screenPos2);
                }
            }
            m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
            break;
        case GestureMode::DUAL_POINTER_GUESS:
        case GestureMode::DUAL_POINTER_TILT:
        case GestureMode::DUAL_POINTER_ROTATE:
        case GestureMode::DUAL_POINTER_SCALE:
        case GestureMode::DUAL_POINTER_FREE:
            m_dualPointerReleaseTime = std::chrono::steady_clock::now();
            m_prevScreenPos1 = screenPos1;
            m_gestureMode = GestureMode::SINGLE_POINTER_PAN;
            break;
        default:
            break;
        }
        break;
    }
    
    // Update pointer count
    if (action == TouchAction::POINTER_1_DOWN || action == TouchAction::POINTER_2_DOWN) {
        m_pointersDown = std::min(2, m_pointersDown + 1);
    } else if (action == TouchAction::POINTER_1_UP || action == TouchAction::POINTER_2_UP) {
        m_pointersDown = std::max(0, m_pointersDown - 1);
    } else if (action == TouchAction::CANCEL) {
        m_pointersDown = 0;
    }
}

}
