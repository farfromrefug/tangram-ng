package com.styluslabs.tangram;

import androidx.annotation.Keep;

/**
 * Listener interface for map click events.
 * The listener can be set with {@link MapController#setMapClickListener(MapClickListener)}.
 * The callback will be run on the main (UI) thread.
 */
@Keep
public interface MapClickListener {
    /**
     * Called when the user taps on the map.
     * 
     * @param x The x screen coordinate where the tap occurred
     * @param y The y screen coordinate where the tap occurred
     * @return true to consume the event and prevent default behavior (centering), false to allow default handling
     */
    boolean onMapClick(float x, float y);
}
