#include "util/touchHandler.h"
#include "glm/gtx/rotate_vector.hpp"
#include <cmath>

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

namespace Tangram {

TouchHandler::TouchHandler(View& _view)
    : m_view(_view),
      m_gestureMode(GestureMode::SINGLE_POINTER_CLICK_GUESS),
      m_pointersDown(0),
      m_noDualPointerYet(true),
      m_velocityPan(0.f, 0.f),
      m_velocityZoom(0.f),
      m_dualPointerReleaseTime(std::chrono::steady_clock::now()) {
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
}

glm::vec2 TouchHandler::getTranslation(float _startX, float _startY, float _endX, float _endY) {
    float elev = 0;
    m_view.screenPositionToLngLat(_startX, _startY, &elev);

    auto start = m_view.screenToGroundPlane(_startX, _startY, elev);
    auto end = m_view.screenToGroundPlane(_endX, _endY, elev);

    glm::vec2 dr = start - end;

    // prevent extreme panning when view is nearly horizontal
    if (m_view.getPitch() > 75.0 * M_PI / 180) {
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

void TouchHandler::singlePointerZoom(const ScreenPos& screenPos, View& viewState) {
    // Implement single pointer zoom (e.g., double-tap and drag)
    float deltaY = screenPos.y - m_prevScreenPos1.y;
    float zoomDelta = -deltaY * 0.01f; // Adjust sensitivity as needed
    
    // Get the fixed point for zooming
    float elev;
    m_view.screenPositionToLngLat(screenPos.x, screenPos.y, &elev);
    auto start = m_view.screenToGroundPlane(screenPos.x, screenPos.y, elev);
    
    m_view.zoom(zoomDelta);
    
    auto end = m_view.screenToGroundPlane(screenPos.x, screenPos.y, elev);
    m_view.translate(start - end);
    
    m_prevScreenPos1 = screenPos;
}

bool TouchHandler::singlePointerZoomStop(const ScreenPos& screenPos, View& viewState) {
    // Return true if we should perform a double-tap zoom
    return false; // Simplified for now
}

void TouchHandler::doubleTapZoom(const ScreenPos& screenPos, View& viewState) {
    setVelocity(0.f, glm::vec2(0.f, 0.f));
    
    float elev;
    m_view.screenPositionToLngLat(screenPos.x, screenPos.y, &elev);
    auto start = m_view.screenToGroundPlane(screenPos.x, screenPos.y, elev);
    
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
    // Simplified implementation - returns 0 for now
    return 0.0f;
}

void TouchHandler::onTouchEvent(TouchAction action, const ScreenPos& screenPos1, const ScreenPos& screenPos2) {
    View& viewState = m_view;
    
    switch (action) {
    case TouchAction::POINTER_1_DOWN:
        m_noDualPointerYet = true;
        setVelocity(0.f, glm::vec2(0.f, 0.f));
        m_prevScreenPos1 = screenPos1;
        m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
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
        switch (m_gestureMode) {
        case GestureMode::SINGLE_POINTER_CLICK_GUESS:
            // Transition to pan if moved enough
            m_gestureMode = GestureMode::SINGLE_POINTER_PAN;
            m_prevScreenPos1 = screenPos1;
            break;
        case GestureMode::DUAL_POINTER_CLICK_GUESS:
            m_gestureMode = GestureMode::DUAL_POINTER_GUESS;
            m_prevScreenPos1 = screenPos1;
            m_prevScreenPos2 = screenPos2;
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
        switch (m_gestureMode) {
        case GestureMode::SINGLE_POINTER_CLICK_GUESS:
            // Handle single tap
            break;
        case GestureMode::DUAL_POINTER_CLICK_GUESS:
            m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
            break;
        case GestureMode::SINGLE_POINTER_PAN:
            m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
            if (m_noDualPointerYet) {
                // Start kinetic pan
                setVelocity(0.f, m_velocityPan);
            }
            break;
        case GestureMode::SINGLE_POINTER_ZOOM:
            if (singlePointerZoomStop(screenPos1, viewState)) {
                doubleTapZoom(screenPos1, viewState);
            }
            m_gestureMode = GestureMode::SINGLE_POINTER_CLICK_GUESS;
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
        break;
        
    case TouchAction::POINTER_2_UP:
        switch (m_gestureMode) {
        case GestureMode::DUAL_POINTER_CLICK_GUESS:
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
    }
}

}
