package com.styluslabs.tangram;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import com.styluslabs.tangram.networking.DefaultHttpHandler;
import com.styluslabs.tangram.networking.HttpHandler;
import com.styluslabs.tangram.viewholder.GLSurfaceViewHolderFactory;
import com.styluslabs.tangram.viewholder.GLViewHolder;
import com.styluslabs.tangram.viewholder.GLViewHolderFactory;

import java.lang.ref.WeakReference;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * {@code MapView} is a View for displaying a Tangram map.
 */
public class MapView extends FrameLayout {

    protected MapController mapController;
    protected GLViewHolder viewHolder;
    protected AsyncTask<Void, Void, Boolean> loadLibraryTask;
    protected static boolean libraryLoaded = false;

    /**
     * MapReadyCallback interface
     * onMapReady gets invoked on the ui thread when {@link MapController} is instantiated and ready to be used
     */
    public interface MapReadyCallback {
        void onMapReady(@Nullable MapController mapController);
    }

    public MapView(@NonNull final Context context) {
        super(context);
    }

    public MapView(@NonNull final Context context, @Nullable final AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Construct a {@code MapController} in an async thread using {@link GLSurfaceView} as GL rendering interface;
     * may only be called from the UI thread
     * Map instance uses {@link DefaultHttpHandler} for retrieving remote map resources
     * @param readyCallback {@link MapReadyCallback#onMapReady(MapController)} to be invoked when
     * {@link MapController} is instantiated and ready to be used. The callback will be made on the UI thread
     * @return false when getMapAsync was called previously
     */
    public boolean getMapAsync(@NonNull final MapReadyCallback readyCallback) {
        return getMapAsync(readyCallback, new GLSurfaceViewHolderFactory(), null);
    }

    /**
     * Construct a {@code MapController} in an async thread using {@link GLSurfaceView} as GL rendering interface;
     * may only be called from the UI thread
     * @param readyCallback {@link MapReadyCallback#onMapReady(MapController)} to be invoked when
     * {@link MapController} is instantiated and ready to be used. The callback will be made on the UI thread
     * @param handler Set the client implemented {@link HttpHandler} for retrieving remote map resources
     *                when null {@link DefaultHttpHandler} is used
     * @return false when getMapAsync was called previously
     */
    public boolean getMapAsync(@NonNull final MapReadyCallback readyCallback,
                            @Nullable final HttpHandler handler) {
        return getMapAsync(readyCallback, new GLSurfaceViewHolderFactory(), handler);
    }

    /**
     * Construct a {@code MapController} in an async thread using an instance
     * {@link GLViewHolderFactory}, like {@link GLSurfaceViewHolderFactory} or any client implementation
     * may only be called from the UI thread
     * Map instance uses {@link DefaultHttpHandler} for retrieving remote map resources
     * @param readyCallback {@link MapReadyCallback#onMapReady(MapController)} to be invoked when
     * {@link MapController} is instantiated and ready to be used. The callback will be made on the UI thread
     * @param glViewHolderFactory An instance of {@link GLViewHolderFactory} providing the GL responsibilities
     * @return false when getMapAsync was called previously
     */
    public boolean getMapAsync(@NonNull final MapReadyCallback readyCallback,
                            @NonNull final GLViewHolderFactory glViewHolderFactory) {
        return getMapAsync(readyCallback, glViewHolderFactory, null);
    }
    /**
     * Construct a {@code MapController} in an async thread using an instance
     * {@link GLViewHolderFactory}, like {@link GLSurfaceViewHolderFactory} or any client implementation
     * may only be called from the UI thread
     * @param readyCallback {@link MapReadyCallback#onMapReady(MapController)} to be invoked when
     * {@link MapController} is instantiated and ready to be used. The callback will be made on the UI thread
     * @param glViewHolderFactory An instance of {@link GLViewHolderFactory} providing the GL responsibilities
     * @param handler Set the client implemented {@link HttpHandler} for retrieving remote map resources
     *                when null {@link DefaultHttpHandler} is used
     * @return false when getMapAsync was called previously
     */
    public boolean getMapAsync(@NonNull final MapReadyCallback readyCallback,
                               final GLViewHolderFactory glViewHolderFactory,
                               @Nullable final HttpHandler handler) {
        if (viewHolder != null) {
            Log.e(BuildConfig.TAG, "MapView already initialized");
            return false;
        }

        final WeakReference<MapView> mapViewRef = new WeakReference<>(this);

        loadLibraryTask = loadNativeLibraryAsync(new NativeLibraryLoadCb() {
            @Override
            public void onLibraryReadyAsync(Boolean ok) {
                //FontConfig.init();
            }

            @Override
            public void onLibraryReady(Boolean ok) {
                MapView view = mapViewRef.get();
                if (ok && view != null) {
                    readyCallback.onMapReady(view.initMapController(glViewHolderFactory, handler));
                } else {
                    readyCallback.onMapReady(null);
                }
            }
        });

        return true;
    }

    /**
     * Construct a {@code MapController}
     * This may take some time to initialize when {@link MapView#loadNativeLibrary()} was not called previously
     * @param readyCallback {@link MapReadyCallback#onMapReady(MapController)} to be invoked when
     * {@link MapController} is instantiated and ready to be used. The callback will be made on the UI thread
     * @param handler Set the client implemented {@link HttpHandler} for retrieving remote map resources
     *                when null {@link DefaultHttpHandler} is used
     * @return MapController
     */
    public MapController initMapController(@NonNull final MapReadyCallback readyCallback,
                                           @Nullable final HttpHandler handler) {
        return initMapController(new GLSurfaceViewHolderFactory(), handler);
    }

    /**
     * Construct a {@code MapController}
     * This may take some time to initialize when {@link MapView#loadNativeLibrary()} was not called previously
     * {@link GLViewHolderFactory}, like {@link GLSurfaceViewHolderFactory} or any client implementation
     * may only be called from the UI thread
     * {@link MapController} is instantiated and ready to be used. The callback will be made on the UI thread
     * @param glViewHolderFactory An instance of {@link GLViewHolderFactory} providing the GL responsibilities
     * @param handler Set the client implemented {@link HttpHandler} for retrieving remote map resources
     *                when null {@link DefaultHttpHandler} is used
     * @return MapController
     */
    public @Nullable MapController initMapController(final GLViewHolderFactory glViewHolderFactory,
                                                     @Nullable final HttpHandler handler) {
        if (!loadNativeLibrary()) {
            return null;
        }

        if (mapController != null) {
            Log.e(BuildConfig.TAG, "MapView already initialized");
            return mapController;
        }
        viewHolder = glViewHolderFactory.build(getContext());
        if (viewHolder == null) {
            Log.e(BuildConfig.TAG, "Unable to initialize MapController: Failed to initialize OpenGL view");
            return null;
        }

        long time = System.currentTimeMillis();

        mapController = getMapInstance(getContext());

        long now = System.currentTimeMillis();
        Log.d(BuildConfig.TAG, "MapController creation took " + (now - time)+ "ms");
        time = now;

        if (mapController != null) {
            mapController.UIThreadInit(viewHolder, handler);

            now = System.currentTimeMillis();
            Log.d(BuildConfig.TAG, "MapController init took " + (now - time) + "ms");

            addView(viewHolder.getView());
        }
        return mapController;
    }

    public MapController getMapController() {
        if (mapController == null) {
            Log.e(BuildConfig.TAG, "MapView not initialized");
        }
        return mapController;
    }

    /**
     * Responsible for doing the native map library loading. Should be executed from a non-ui thread,
     * see {@link MapView#loadNativeLibraryAsync(NativeLibraryLoadCb)}.
     * @return true when everything went as expected
     */
    public static boolean loadNativeLibrary() {
        long time = System.currentTimeMillis();
        if (NativeLibraryLoader.sNativeLibraryLoaded) {
            libraryLoaded = true;
            time = System.currentTimeMillis() - time;
            Log.d(BuildConfig.TAG, "Loading native library took " + time + "ms");
            return true;
        }
        Log.e(BuildConfig.TAG, "Unable to initialize MapController: Failed to load native library");
        return false;
    }

    public interface NativeLibraryLoadCb {
        void onLibraryReadyAsync(Boolean ok);
        void onLibraryReady(Boolean ok);
    }

    /**
     * Responsible for doing the native map library loading in an AsyncTask.
     * @return AsyncTask
     */
    public static AsyncTask<Void, Void, Boolean> loadNativeLibraryAsync(final @Nullable NativeLibraryLoadCb readyCb) {
         class InitTask extends AsyncTask<Void, Void, Boolean> {
            @Override
            protected Boolean doInBackground(Void... voids) {
                boolean ok = loadNativeLibrary();
                if (readyCb != null) {
                    readyCb.onLibraryReadyAsync(ok);
                }
                return ok;
            }
            @Override
            protected void onPostExecute(Boolean ok) {
                if (readyCb != null) {
                    readyCb.onLibraryReady(ok);
                }
            }
            @Override
            protected void onCancelled(Boolean ok) {
                if (readyCb != null) {
                    readyCb.onLibraryReady(false);
                }
            }
        }
        return new InitTask().execute();
    }

    protected void disposeMap() {

        if (loadLibraryTask != null) {
            loadLibraryTask.cancel(true);
            loadLibraryTask = null;
        }

        if (viewHolder != null) {
            viewHolder.onPause();
            viewHolder = null;
        }

        if (mapController != null) {
            mapController.dispose();
            mapController = null;
        }
    }

    // New-style touch handling (Carto-like)
    private static final int NATIVE_ACTION_POINTER_1_DOWN = 0;
    private static final int NATIVE_ACTION_POINTER_2_DOWN = 1;
    private static final int NATIVE_ACTION_MOVE = 2;
    private static final int NATIVE_ACTION_CANCEL = 3;
    private static final int NATIVE_ACTION_POINTER_1_UP = 4;
    private static final int NATIVE_ACTION_POINTER_2_UP = 5;
    private static final float NATIVE_NO_COORDINATE = -1.0f;
    private static final int INVALID_POINTER_ID = -1;

    private int pointer1Id = INVALID_POINTER_ID;
    private int pointer2Id = INVALID_POINTER_ID;
    private boolean useNewTouchHandling = false;

    /**
     * Enable new touch handling system (similar to Carto Mobile SDK)
     * This provides more consistent cross-platform touch handling
     * @param enabled true to use new system, false for legacy gesture-based system
     */
    public void setNewTouchHandlingEnabled(boolean enabled) {
        useNewTouchHandling = enabled;
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        if (mapController != null) {
            if (useNewTouchHandling) {
                return handleTouchEventNew(ev);
            } else {
                return mapController.handleGesture(this, ev);
            }
        }
        return false;
    }

    private synchronized boolean handleTouchEventNew(MotionEvent event) {
        if (mapController == null || mapController.nativeMap == null) {
            return false;
        }

        boolean clickable = isClickable() || isLongClickable();
        if (!isEnabled() || !clickable) {
            return clickable;
        }

        try {
            int pointer1Index;
            int pointer2Index;
            switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                pointer1Index = event.getActionIndex();
                pointer1Id = event.getPointerId(pointer1Index);
                mapController.nativeMap.handleTouchEvent(NATIVE_ACTION_POINTER_1_DOWN,
                        event.getX(pointer1Index), event.getY(pointer1Index),
                        NATIVE_NO_COORDINATE, NATIVE_NO_COORDINATE);
                break;
            case MotionEvent.ACTION_POINTER_DOWN:
                if (event.getPointerCount() == 2) {
                    // Check which pointer to use
                    if (pointer1Id != INVALID_POINTER_ID) {
                        pointer1Index = event.findPointerIndex(pointer1Id);
                        pointer2Index = event.getActionIndex();
                        pointer2Id = event.getPointerId(event.getActionIndex());
                    } else if (pointer2Id != INVALID_POINTER_ID) {
                        pointer2Index = event.findPointerIndex(pointer2Id);
                        pointer1Index = event.getActionIndex();
                        pointer1Id = event.getPointerId(event.getActionIndex());
                    } else {
                        break;
                    }
                    mapController.nativeMap.handleTouchEvent(NATIVE_ACTION_POINTER_2_DOWN,
                            event.getX(pointer1Index), event.getY(pointer1Index),
                            event.getX(pointer2Index), event.getY(pointer2Index));
                }
                break;
            case MotionEvent.ACTION_MOVE:
                if (pointer1Id != INVALID_POINTER_ID && pointer2Id == INVALID_POINTER_ID) {
                    pointer1Index = event.findPointerIndex(pointer1Id);
                    mapController.nativeMap.handleTouchEvent(NATIVE_ACTION_MOVE,
                            event.getX(pointer1Index), event.getY(pointer1Index),
                            NATIVE_NO_COORDINATE, NATIVE_NO_COORDINATE);
                } else if (pointer1Id != INVALID_POINTER_ID && pointer2Id != INVALID_POINTER_ID) {
                    pointer1Index = event.findPointerIndex(pointer1Id);
                    pointer2Index = event.findPointerIndex(pointer2Id);
                    mapController.nativeMap.handleTouchEvent(NATIVE_ACTION_MOVE,
                            event.getX(pointer1Index), event.getY(pointer1Index),
                            event.getX(pointer2Index), event.getY(pointer2Index));
                }
                break;
            case MotionEvent.ACTION_CANCEL:
                mapController.nativeMap.handleTouchEvent(NATIVE_ACTION_CANCEL,
                        NATIVE_NO_COORDINATE, NATIVE_NO_COORDINATE,
                        NATIVE_NO_COORDINATE, NATIVE_NO_COORDINATE);
                pointer1Id = INVALID_POINTER_ID;
                pointer2Id = INVALID_POINTER_ID;
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                int pointerIndex = event.getActionIndex();
                int pointerId = event.getPointerId(pointerIndex);
                // Single pointer
                if (pointer1Id == pointerId && pointer2Id == INVALID_POINTER_ID) {
                    pointer1Index = event.findPointerIndex(pointer1Id);
                    mapController.nativeMap.handleTouchEvent(NATIVE_ACTION_POINTER_1_UP,
                            event.getX(pointer1Index), event.getY(pointer1Index),
                            NATIVE_NO_COORDINATE, NATIVE_NO_COORDINATE);
                    pointer1Id = INVALID_POINTER_ID;
                    // Dual pointer, first pointer up
                } else if (pointer1Id == pointerId) {
                    pointer1Index = event.findPointerIndex(pointer1Id);
                    pointer2Index = event.findPointerIndex(pointer2Id);
                    mapController.nativeMap.handleTouchEvent(NATIVE_ACTION_POINTER_1_UP,
                            event.getX(pointer1Index), event.getY(pointer1Index),
                            event.getX(pointer2Index), event.getY(pointer2Index));
                    pointer1Id = pointer2Id;
                    pointer2Id = INVALID_POINTER_ID;
                    // Dual pointer, second finger up
                } else if (pointer2Id == pointerId) {
                    pointer1Index = event.findPointerIndex(pointer1Id);
                    pointer2Index = event.findPointerIndex(pointer2Id);
                    mapController.nativeMap.handleTouchEvent(NATIVE_ACTION_POINTER_2_UP,
                            event.getX(pointer1Index), event.getY(pointer1Index),
                            event.getX(pointer2Index), event.getY(pointer2Index));
                    pointer2Id = INVALID_POINTER_ID;
                }
                break;
            }
        }
        catch (IllegalArgumentException e) {
            Log.e(BuildConfig.TAG, "MapView.onTouchEvent: " + e);
            return false;
        }
        return true;
    }

    protected MapController getMapInstance(Context context) {
        return new MapController(context);
    }

    /**
     * You must call this method from the parent Activity/Fragment's corresponding method.
     */
    public void onCreate(final Bundle savedInstanceState) {

    }

    /**
     * You must call this method from the parent Activity/Fragment's corresponding method.
     */
    public void onResume() {
        if (viewHolder != null) {
            viewHolder.onResume();
        }
    }

    /**
     * You must call this method from the parent Activity/Fragment's corresponding method.
     */
    public void onPause() {
        if (viewHolder != null) {
            viewHolder.onPause();
        }
    }

    /**
     * You must call this method from the parent Activity/Fragment's corresponding method.
     * Any access to MapView.mapController is illegal after this call.
     */
    public void onDestroy() {
        if (viewHolder != null) {
            viewHolder.onDestroy();
        }
        disposeMap();
    }

    /**
     * You must call this method from the parent Activity/Fragment's corresponding method.
     */
    public void onLowMemory() {
        mapController.onLowMemory();
    }

}
