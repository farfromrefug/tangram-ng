package com.styluslabs.tangram.android;

import android.graphics.PointF;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.widget.Button;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.styluslabs.tangram.CameraPosition;
import com.styluslabs.tangram.CameraUpdateFactory;
import com.styluslabs.tangram.FeaturePickListener;
import com.styluslabs.tangram.FeaturePickResult;
import com.styluslabs.tangram.LabelPickListener;
import com.styluslabs.tangram.LabelPickResult;
import com.styluslabs.tangram.LngLat;
import com.styluslabs.tangram.MapChangeListener;
import com.styluslabs.tangram.MapClickListener;
import com.styluslabs.tangram.MapController;
import com.styluslabs.tangram.MapData;
import com.styluslabs.tangram.MapInteractionListener;
import com.styluslabs.tangram.MapView;
import com.styluslabs.tangram.Marker;
import com.styluslabs.tangram.MarkerPickListener;
import com.styluslabs.tangram.MarkerPickResult;
import com.styluslabs.tangram.SceneError;
import com.styluslabs.tangram.SceneUpdate;
import com.styluslabs.tangram.networking.DefaultHttpHandler;
import com.styluslabs.tangram.networking.HttpHandler;

import java.io.File;
import java.util.*;
import java.util.concurrent.TimeUnit;

import okhttp3.Cache;
import okhttp3.CacheControl;
import okhttp3.HttpUrl;
import okhttp3.OkHttpClient;
import okhttp3.Request;

/**
 * Hillshade feature demo.
 *
 * <p>Demonstrates use of scene files from
 * <a href="https://github.com/farfromrefug/ascendmaps/tree/abd695a8f13e7c0cfffc96b4daf4f19a336cceda/assets">
 * AscendMaps</a>:
 * <ul>
 *   <li><b>Hillshading</b> – scenes/hillshade.yaml (elevation from ArcGIS WorldElevation3D)</li>
 *   <li><b>Slope toggle</b> – slope-angle and custom-slope-shading overlays via
 *       {@link MapController#updateGlobals} (no scene reload required)</li>
 *   <li><b>Custom layer</b> – adds ArcGIS satellite source defined in mapsources.default.yaml
 *       at runtime using {@link MapController#loadSceneFileAsync} with SceneUpdates</li>
 *   <li><b>Config usage</b> – reads metric_units and view.zoom from config.default.yaml;
 *       sets contours on/off via {@link MapController#updateGlobals}</li>
 * </ul>
 *
 * <p>Initial camera: Austrian Alps (Hohe Tauern), lng 12.80 / lat 47.32 / zoom 10.
 */
public class MainActivity extends AppCompatActivity implements MapController.SceneLoadListener, FeaturePickListener, LabelPickListener, MarkerPickListener,
        MapView.MapReadyCallback {

    private static final String TAG = "TangramDemo";

    // Hillshade demo scene (imports hillshade.yaml, slope-angle.yaml, custom-slope-shading.yaml)
    private static final String SCENE_HILLSHADE_DEMO = "asset:///scene-hillshade-demo.yaml";

    private static final String[] SCENE_PRESETS = {
            SCENE_HILLSHADE_DEMO,
            "asset:///scene.yaml",
            "asset:///scene3d.yaml",
            "asset:///scene-pmtiles.yaml"
    };

    // --- config.default.yaml values used in this demo ---
    // In a full app these would be parsed from the YAML file (e.g. with SnakeYAML).
    // Here they are read directly so the demo remains dependency-free.
    private static final boolean CONFIG_METRIC_UNITS = true;  // config.default.yaml: metric_units
    private static final int     CONFIG_VIEW_ZOOM    = 10;    // config.default.yaml: view.zoom (adjusted for Alps)

    // --- state ---
    // 0 = OFF, 1 = Slope Angle, 2 = Custom Slope Shading
    private int slopeMode = 0;
    private boolean satelliteLayerEnabled = false;
    private boolean contoursEnabled = true;

    private ArrayList<SceneUpdate> sceneUpdates = new ArrayList<>();

    MapController map;
    MapView view;
    MapData mapData;

    Button btnSlope;
    Button btnLayer;
    Button btnContours;

    PresetSelectionTextView sceneSelector;

    boolean showTileInfo = false;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.main);

        sceneSelector = (PresetSelectionTextView)findViewById(R.id.sceneSelector);
        sceneSelector.setText(SCENE_PRESETS[0]);
        sceneSelector.setPresetStrings(Arrays.asList(SCENE_PRESETS));
        sceneSelector.setOnSelectionListener(new PresetSelectionTextView.OnSelectionListener() {
            @Override
            public void onSelection(String selection) {
                // Reset demo state when scene is manually switched
                slopeMode = 0;
                satelliteLayerEnabled = false;
                contoursEnabled = true;
                updateButtonLabels();
                map.loadSceneFile(selection, sceneUpdates);
            }
        });

        btnSlope = (Button)findViewById(R.id.btnSlope);
        btnLayer = (Button)findViewById(R.id.btnLayer);
        btnContours = (Button)findViewById(R.id.btnContours);

        btnSlope.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { toggleSlope(); }
        });
        btnLayer.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { toggleSatelliteLayer(); }
        });
        btnContours.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { toggleContours(); }
        });

        view = (MapView)findViewById(R.id.map);
        view.getMapAsync(this, getHttpHandler());
    }

    // -------------------------------------------------------------------------
    // Slope overlay toggle
    // -------------------------------------------------------------------------

    /**
     * Cycles slope mode: OFF → Slope Angle → Custom Slope Shading → OFF.
     *
     * <p>Uses {@link MapController#updateGlobals} so no scene reload is required.
     * The demo scene binds {@code u_slope_angle_opacity} and {@code u_custom_slope_opacity}
     * to globals {@code global.slope_angle_opacity} / {@code global.custom_slope_opacity}.
     */
    private void toggleSlope() {
        slopeMode = (slopeMode + 1) % 3;

        // Opacity 0.75 = visible, 0.0 = hidden
        String slopeAngleOpacity  = (slopeMode == 1) ? "0.75" : "0.0";
        String customSlopeOpacity = (slopeMode == 2) ? "0.75" : "0.0";

        map.updateGlobals(Arrays.asList(
                new SceneUpdate("global.slope_angle_opacity",  slopeAngleOpacity),
                new SceneUpdate("global.custom_slope_opacity", customSlopeOpacity)
        ), /* rebuildTiles= */ true);

        updateButtonLabels();
        Log.d(TAG, "Slope mode: " + getSlopeModeLabel());
    }

    // -------------------------------------------------------------------------
    // Satellite layer toggle (custom source from mapsources.default.yaml)
    // -------------------------------------------------------------------------

    /**
     * Adds or removes the ArcGIS Satellite raster source.
     *
     * <p>The source URL is taken from {@code mapsources.default.yaml} (arcgis-satellite entry).
     * Adding a source requires a scene reload with SceneUpdates; removal also reloads
     * without those updates.
     */
    private void toggleSatelliteLayer() {
        satelliteLayerEnabled = !satelliteLayerEnabled;

        List<SceneUpdate> updates = buildSceneUpdates();
        map.loadSceneFileAsync(getCurrentSceneUrl(), updates);

        updateButtonLabels();
        Log.d(TAG, "Satellite layer: " + satelliteLayerEnabled);
    }

    /**
     * Builds the SceneUpdate list for the current demo state.
     * Slope opacity is handled separately via updateGlobals, so the only reload-time
     * updates needed are for the satellite layer source/layer definition.
     */
    private List<SceneUpdate> buildSceneUpdates() {
        List<SceneUpdate> updates = new ArrayList<>();

        if (satelliteLayerEnabled) {
            // arcgis-satellite source from mapsources.default.yaml
            updates.add(new SceneUpdate("sources.arcgis-satellite.type", "Raster"));
            updates.add(new SceneUpdate("sources.arcgis-satellite.url",
                    "https://server.arcgisonline.com/arcgis/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"));
            updates.add(new SceneUpdate("sources.arcgis-satellite.max_zoom", "19"));
            // Layer that renders the satellite tiles using the satellite-overlay style
            // defined in scene-hillshade-demo.yaml
            updates.add(new SceneUpdate("layers.arcgis-satellite-layer.data.source", "arcgis-satellite"));
            updates.add(new SceneUpdate("layers.arcgis-satellite-layer.draw.satellite-overlay.order", "500"));
        }

        return updates;
    }

    // -------------------------------------------------------------------------
    // Contour lines toggle (uses config.default.yaml metric_units setting)
    // -------------------------------------------------------------------------

    /**
     * Toggles contour lines on/off.
     *
     * <p>Reads {@code CONFIG_METRIC_UNITS} from config.default.yaml to pass the correct
     * unit system when enabling contours.
     */
    private void toggleContours() {
        contoursEnabled = !contoursEnabled;

        map.updateGlobals(Arrays.asList(
                // global.show_contours drives #ifdef SHOW_CONTOURS in the hillshade shader
                new SceneUpdate("global.show_contours", String.valueOf(contoursEnabled)),
                // global.metric_units drives #ifdef METRIC_UNITS (25m/50m/100m vs ft intervals)
                // sourced from config.default.yaml: metric_units
                new SceneUpdate("global.metric_units", String.valueOf(CONFIG_METRIC_UNITS))
        ), /* rebuildTiles= */ true);

        updateButtonLabels();
        Log.d(TAG, "Contours: " + contoursEnabled + " (metric=" + CONFIG_METRIC_UNITS + ")");
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    private String getCurrentSceneUrl() {
        String url = sceneSelector.getCurrentString();
        return (url != null && !url.isEmpty()) ? url : SCENE_HILLSHADE_DEMO;
    }

    private String getSlopeModeLabel() {
        switch (slopeMode) {
            case 1:  return "Slope Angle";
            case 2:  return "Custom Slope";
            default: return "OFF";
        }
    }

    private void updateButtonLabels() {
        btnSlope.setText("Slope: " + getSlopeModeLabel());
        btnLayer.setText("Satellite: " + (satelliteLayerEnabled ? "ON" : "OFF"));
        btnContours.setText("Contours: " + (contoursEnabled ? "ON" : "OFF"));
    }

    // -------------------------------------------------------------------------
    // MapReadyCallback
    // -------------------------------------------------------------------------

    @Override
    public void onMapReady(@Nullable MapController mapController) {
        if (mapController == null) { return; }
        map = mapController;
        map.setSceneLoadListener(this);

        mapController.setMapInteractionListener(new MapInteractionListener() {
            public boolean onMapInteraction(boolean isPanning, boolean isZooming,
                                            boolean isRotating, boolean isTilting) {
                return false;
            }
        });

        mapController.setMapClickListener(new MapClickListener() {
            @Override
            public boolean onMapClick(ClickType clickType, float x, float y) {
                switch (clickType) {
                    case CLICK_TYPE_SINGLE: {
                        LngLat tappedPoint = map.screenPositionToLngLat(new PointF(x, y));
                        map.pickFeature(x, y);
                        map.pickLabel(x, y);
                        map.pickMarker(x, y);
                        map.updateCameraPosition(CameraUpdateFactory.setPosition(tappedPoint), 1000, null);
                        return true;
                    }
                    case CLICK_TYPE_LONG: {
                        showTileInfo = !showTileInfo;
                        map.setDebugFlag(MapController.DebugFlag.TILE_INFOS, showTileInfo);
                        return true;
                    }
                }
                return false;
            }
        });

        // Initial camera: Austrian Alps (config.default.yaml view.zoom = CONFIG_VIEW_ZOOM)
        LngLat alps = new LngLat(12.80, 47.32);
        map.updateCameraPosition(CameraUpdateFactory.newLngLatZoom(alps, CONFIG_VIEW_ZOOM));

        Log.d(TAG, "Loading hillshade demo scene");
        map.loadSceneFileAsync(SCENE_HILLSHADE_DEMO, sceneUpdates);

        map.setFeaturePickListener(this);
        map.setLabelPickListener(this);
        map.setMarkerPickListener(this);

        map.setMapChangeListener(new MapChangeListener() {
            @Override public void onViewComplete() {}
            @Override public void onRegionWillChange(boolean animated) {}
            @Override public void onRegionIsChanging() {}
            @Override public void onRegionDidChange(boolean animated) {}
        });

        mapData = map.addDataLayer("touch");
        map.setPickRadius(10);
    }

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    @Override
    public void onResume() {
        super.onResume();
        view.onResume();
    }

    @Override
    public void onPause() {
        super.onPause();
        view.onPause();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        view.onDestroy();
    }

    @Override
    public void onLowMemory() {
        super.onLowMemory();
        view.onLowMemory();
    }

    // -------------------------------------------------------------------------
    // Scene load callback
    // -------------------------------------------------------------------------

    @Override
    public void onSceneReady(int sceneId, SceneError sceneError) {
        Log.d(TAG, "onSceneReady: " + sceneId);
        if (sceneError != null) {
            Log.e(TAG, "Scene error: " + sceneError.getSceneUpdate() + " " + sceneError.getError());
            Toast.makeText(this, "Scene error: " + sceneError.getError(), Toast.LENGTH_SHORT).show();
            return;
        }
        // After reload, re-apply the current slope opacity state (satellite state is
        // already embedded in the SceneUpdates; slope state uses globals).
        if (slopeMode != 0) {
            map.updateGlobals(Arrays.asList(
                    new SceneUpdate("global.slope_angle_opacity",  (slopeMode == 1) ? "0.75" : "0.0"),
                    new SceneUpdate("global.custom_slope_opacity", (slopeMode == 2) ? "0.75" : "0.0")
            ), true);
        }
    }

    // -------------------------------------------------------------------------
    // HTTP handler with tile cache
    // -------------------------------------------------------------------------

    HttpHandler getHttpHandler() {
        OkHttpClient.Builder builder = DefaultHttpHandler.getClientBuilder();
        File cacheDir = getExternalCacheDir();
        if (cacheDir != null && cacheDir.exists()) {
            builder.cache(new Cache(cacheDir, 16 * 1024 * 1024));
        }
        return new DefaultHttpHandler(builder) {
            CacheControl tileCacheControl = new CacheControl.Builder().maxStale(7, TimeUnit.DAYS).build();
            @Override
            protected void configureRequest(HttpUrl url, Request.Builder builder) {
                if (url.host().contains("arcgisonline.com") ||
                        url.host().contains("arcgis.com")) {
                    builder.cacheControl(tileCacheControl);
                }
            }
        };
    }

    // -------------------------------------------------------------------------
    // Pick listeners
    // -------------------------------------------------------------------------

    @Override
    public void onFeaturePickComplete(@Nullable final FeaturePickResult result) {
        if (result == null) { return; }
        String name = result.getProperties().get("name");
        if (name == null || name.isEmpty()) { name = "unnamed"; }
        final String msg = name;
        Toast.makeText(getApplicationContext(), "Feature: " + msg, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onLabelPickComplete(@Nullable final LabelPickResult result) {
        if (result == null) { return; }
        String name = result.getProperties().get("name");
        if (name == null || name.isEmpty()) { name = "unnamed"; }
        Toast.makeText(getApplicationContext(), "Label: " + name, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onMarkerPickComplete(@Nullable final MarkerPickResult result) {
        if (result == null) { return; }
        Toast.makeText(getApplicationContext(), "Marker: " + result.getMarker().getMarkerId(), Toast.LENGTH_SHORT).show();
    }
}
