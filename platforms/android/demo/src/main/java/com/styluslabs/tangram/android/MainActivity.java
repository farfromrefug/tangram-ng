package com.styluslabs.tangram.android;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.graphics.PointF;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
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
import com.styluslabs.tangram.geometry.Polyline;
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
 * Tangram-NG Android demo.
 *
 * <p>Demonstrates the hillshade/slope features from AscendMaps scenes and the following
 * examples (mirroring the desktop glfwApp "Examples" panel):
 * <ol>
 *   <li>Drawn Sources / Layers – add GeoJSON data sources at runtime via SceneUpdates</li>
 *   <li>Routes – build a route polyline by tapping the map</li>
 *   <li>Local Search – pick the nearest named feature at the map centre</li>
 *   <li>Search Results – manage and fly to picked features</li>
 *   <li>Bookmarks – save and recall camera positions</li>
 *   <li>3D Terrain – toggle terrain-3d.yaml import via SceneUpdates</li>
 *   <li>Custom Shader Rendering – enable hillshade/slope overlays</li>
 *   <li>Selection Highlighting – highlight a feature by OSM ID</li>
 * </ol>
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

    // -------------------------------------------------------------------------
    // Inner data classes (mirror glfwApp example structs)
    // -------------------------------------------------------------------------

    private static class Bookmark {
        String name;
        double lng, lat;
        Marker marker;
    }

    private static class SearchResult {
        String name;
        double lng, lat;
        Marker marker;
    }

    private static class DrawnLayer {
        String sourceName;
    }

    // -------------------------------------------------------------------------
    // Hillshade demo state
    // -------------------------------------------------------------------------

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

    // -------------------------------------------------------------------------
    // Examples state (mirrors glfwApp global variables)
    // -------------------------------------------------------------------------

    // -- Drawn Sources / Layers
    private final List<DrawnLayer> drawnLayers = new ArrayList<>();
    private String drawnLayerName = "my-layer";
    private String drawnLayerGeoJson =
            "{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\","
            + "\"geometry\":{\"type\":\"Point\",\"coordinates\":[0,0]},"
            + "\"properties\":{\"name\":\"Origin\"}}]}";

    // -- Routes
    private boolean addRouteWaypointOnClick = false;
    private final List<LngLat> routeWaypoints = new ArrayList<>();
    private Marker routePolylineMarker;

    // -- Local Search / Search Results
    private boolean localSearchActive = false;
    private String searchQuery = "";
    private final List<SearchResult> searchResults = new ArrayList<>();

    // -- Bookmarks
    private String bookmarkName = "My Place";
    private final List<Bookmark> bookmarks = new ArrayList<>();

    // -- 3D Terrain
    private boolean terrain3dEnabled = false;

    // -- Selection Highlighting
    private String selectedOsmId = "";

    // -------------------------------------------------------------------------
    // Activity lifecycle
    // -------------------------------------------------------------------------

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

        Button btnExamples = (Button)findViewById(R.id.btnExamples);
        btnExamples.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { showExamplesMenu(); }
        });

        view = (MapView)findViewById(R.id.map);
        view.getMapAsync(this, getHttpHandler());
    }

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

    private int dp(float dp) {
        return (int)(dp * getResources().getDisplayMetrics().density);
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
                        // Routes: add a waypoint when the mode is active
                        if (addRouteWaypointOnClick && tappedPoint != null) {
                            addRouteWaypoint(tappedPoint);
                        }
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
        // Local Search intercepts the next feature pick to store it as a search result.
        if (localSearchActive) {
            localSearchActive = false;
            if (result == null) {
                Toast.makeText(this, "No named feature found at centre", Toast.LENGTH_SHORT).show();
                return;
            }
            String name = result.getProperties().get("name");
            if (name == null || name.isEmpty()) {
                Toast.makeText(this, "No named feature found at centre", Toast.LENGTH_SHORT).show();
                return;
            }
            LngLat pos = map.screenPositionToLngLat(result.getScreenPosition());
            if (pos != null) { addSearchResult(name, pos); }
            return;
        }
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

    // =========================================================================
    // Examples (mirrors glfwApp showExamplesGUI)
    // =========================================================================

    /** Shows the top-level examples menu, listing the 8 available example categories. */
    private void showExamplesMenu() {
        final String[] items = {
                "Drawn Sources / Layers",
                "Routes",
                "Local Search",
                "Search Results (" + searchResults.size() + ")",
                "Bookmarks (" + bookmarks.size() + ")",
                "3D Terrain",
                "Custom Shader Rendering",
                "Selection Highlighting"
        };
        new AlertDialog.Builder(this)
                .setTitle("Examples")
                .setItems(items, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        switch (which) {
                            case 0: showDrawnSourcesDialog(); break;
                            case 1: showRoutesDialog();       break;
                            case 2: showLocalSearchDialog();  break;
                            case 3: showSearchResultsDialog(); break;
                            case 4: showBookmarksDialog();    break;
                            case 5: showTerrainDialog();      break;
                            case 6: showCustomShaderDialog(); break;
                            case 7: showSelectionDialog();    break;
                        }
                    }
                })
                .show();
    }

    // -------------------------------------------------------------------------
    // 1. Drawn Sources / Layers
    // -------------------------------------------------------------------------

    private void showDrawnSourcesDialog() {
        final EditText etName = new EditText(this);
        etName.setText(drawnLayerName);
        etName.setHint("Source name");

        final EditText etGeoJson = new EditText(this);
        etGeoJson.setText(drawnLayerGeoJson);
        etGeoJson.setHint("GeoJSON");
        etGeoJson.setMinLines(3);

        StringBuilder info = new StringBuilder();
        if (drawnLayers.isEmpty()) {
            info.append("No layers added yet.");
        } else {
            info.append("Active layers:");
            for (DrawnLayer dl : drawnLayers) { info.append(" ").append(dl.sourceName); }
        }
        final TextView tvInfo = new TextView(this);
        tvInfo.setText(info.toString());

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(dp(16), dp(8), dp(16), dp(8));
        layout.addView(tvInfo);
        layout.addView(etName);
        layout.addView(etGeoJson);

        ScrollView scrollView = new ScrollView(this);
        scrollView.addView(layout);

        new AlertDialog.Builder(this)
                .setTitle("Drawn Sources / Layers")
                .setView(scrollView)
                .setPositiveButton("Add Layer", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        drawnLayerName   = etName.getText().toString().trim();
                        drawnLayerGeoJson = etGeoJson.getText().toString().trim();
                        if (!drawnLayerName.isEmpty() && !drawnLayerGeoJson.isEmpty()) {
                            addDrawnLayer(drawnLayerName, drawnLayerGeoJson);
                        }
                    }
                })
                .setNeutralButton("Clear All", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) { clearDrawnLayers(); }
                })
                .setNegativeButton("Close", null)
                .show();
    }

    private void addDrawnLayer(String name, String geojson) {
        map.loadSceneFileAsync(getCurrentSceneUrl(), Arrays.asList(
                new SceneUpdate("sources." + name + ".type",         "GeoJSON"),
                new SceneUpdate("sources." + name + ".data",         geojson),
                new SceneUpdate("layers."  + name + ".data.source",  name),
                new SceneUpdate("layers."  + name + ".draw.points.color", "red"),
                new SceneUpdate("layers."  + name + ".draw.points.size",  "12px"),
                new SceneUpdate("layers."  + name + ".draw.points.order", "5000")
        ));
        DrawnLayer dl = new DrawnLayer();
        dl.sourceName = name;
        drawnLayers.add(dl);
        Log.d(TAG, "Added drawn layer: " + name);
    }

    private void clearDrawnLayers() {
        for (DrawnLayer dl : drawnLayers) {
            map.loadSceneFileAsync(getCurrentSceneUrl(), Arrays.asList(
                    new SceneUpdate("layers." + dl.sourceName + ".draw.points.size", "0px")
            ));
        }
        drawnLayers.clear();
        Log.d(TAG, "Drawn layers cleared");
    }

    // -------------------------------------------------------------------------
    // 2. Routes
    // -------------------------------------------------------------------------

    private void showRoutesDialog() {
        final CheckBox cbWaypoint = new CheckBox(this);
        cbWaypoint.setText("Add waypoint on map tap");
        cbWaypoint.setChecked(addRouteWaypointOnClick);

        final TextView tvWaypoints = new TextView(this);
        tvWaypoints.setText(getWaypointsText());

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(dp(16), dp(8), dp(16), dp(8));
        layout.addView(cbWaypoint);
        layout.addView(tvWaypoints);

        new AlertDialog.Builder(this)
                .setTitle("Routes")
                .setView(layout)
                .setPositiveButton("Apply", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        addRouteWaypointOnClick = cbWaypoint.isChecked();
                        Log.d(TAG, "Route waypoint on tap: " + addRouteWaypointOnClick);
                    }
                })
                .setNeutralButton("Clear Route", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) { clearRoute(); }
                })
                .setNegativeButton("Close", null)
                .show();
    }

    private String getWaypointsText() {
        if (routeWaypoints.isEmpty()) { return "No waypoints."; }
        StringBuilder sb = new StringBuilder("Waypoints: " + routeWaypoints.size() + "\n");
        for (int i = 0; i < routeWaypoints.size(); i++) {
            LngLat wp = routeWaypoints.get(i);
            sb.append("WP").append(i + 1).append(": ")
              .append(String.format(Locale.US, "%.5f, %.5f\n", wp.longitude, wp.latitude));
        }
        return sb.toString();
    }

    private void addRouteWaypoint(LngLat point) {
        routeWaypoints.add(point);
        if (routePolylineMarker == null) {
            routePolylineMarker = map.addMarker();
            routePolylineMarker.setStylingFromString(
                    "{ style: lines, color: blue, width: 6px, order: 5001 }");
        }
        if (routeWaypoints.size() >= 2) {
            routePolylineMarker.setPolyline(new Polyline(routeWaypoints, null));
        }
        Log.d(TAG, "Route waypoint: " + point.longitude + ", " + point.latitude);
    }

    private void clearRoute() {
        routeWaypoints.clear();
        if (routePolylineMarker != null) {
            map.removeMarker(routePolylineMarker);
            routePolylineMarker = null;
        }
        Log.d(TAG, "Route cleared");
    }

    // -------------------------------------------------------------------------
    // 3. Local Search in Vector Tiles
    // -------------------------------------------------------------------------

    private void showLocalSearchDialog() {
        final EditText etQuery = new EditText(this);
        etQuery.setText(searchQuery);
        etQuery.setHint("Feature name (display only)");

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(dp(16), dp(8), dp(16), dp(8));

        TextView tvDesc = new TextView(this);
        tvDesc.setText("Picks the nearest named feature at the map centre.");
        layout.addView(tvDesc);
        layout.addView(etQuery);

        new AlertDialog.Builder(this)
                .setTitle("Local Search in Vector Tiles")
                .setView(layout)
                .setPositiveButton("Search at Centre", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        searchQuery = etQuery.getText().toString();
                        pickFeatureAtCenter();
                    }
                })
                .setNegativeButton("Close", null)
                .show();
    }

    private void pickFeatureAtCenter() {
        if (map == null || view == null) { return; }
        localSearchActive = true;
        float cx = view.getWidth()  / 2f;
        float cy = view.getHeight() / 2f;
        map.pickFeature(cx, cy);
        Log.d(TAG, "Local search: picking at centre (" + cx + ", " + cy + ")");
    }

    private void addSearchResult(String name, LngLat pos) {
        Marker marker = map.addMarker();
        marker.setPoint(pos);
        marker.setStylingFromString(
                "{ style: text, text_source: function(){return 'RESULT';}, "
                + "font: { size: 10px, fill: orange } }");
        SearchResult sr = new SearchResult();
        sr.name   = name;
        sr.lng    = pos.longitude;
        sr.lat    = pos.latitude;
        sr.marker = marker;
        searchResults.add(sr);
        Toast.makeText(this, "Found: " + name, Toast.LENGTH_SHORT).show();
        Log.d(TAG, "Search result: " + name + " at " + pos.longitude + ", " + pos.latitude);
    }

    // -------------------------------------------------------------------------
    // 4. Search Results
    // -------------------------------------------------------------------------

    private void showSearchResultsDialog() {
        if (searchResults.isEmpty()) {
            new AlertDialog.Builder(this)
                    .setTitle("Search Results")
                    .setMessage("No results yet. Use 'Local Search' to pick features.")
                    .setPositiveButton("OK", null)
                    .show();
            return;
        }
        final String[] items = new String[searchResults.size()];
        for (int i = 0; i < searchResults.size(); i++) {
            SearchResult r = searchResults.get(i);
            items[i] = String.format(Locale.US, "%s (%.4f, %.4f)", r.name, r.lng, r.lat);
        }
        new AlertDialog.Builder(this)
                .setTitle("Search Results — tap to fly")
                .setItems(items, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        SearchResult r = searchResults.get(which);
                        map.updateCameraPosition(
                                CameraUpdateFactory.newLngLatZoom(new LngLat(r.lng, r.lat), 16),
                                800, null);
                    }
                })
                .setPositiveButton("Clear All", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) { clearSearchResults(); }
                })
                .setNegativeButton("Close", null)
                .show();
    }

    private void clearSearchResults() {
        for (SearchResult r : searchResults) {
            if (r.marker != null) { map.removeMarker(r.marker); }
        }
        searchResults.clear();
        Log.d(TAG, "Search results cleared");
    }

    // -------------------------------------------------------------------------
    // 5. Bookmarks
    // -------------------------------------------------------------------------

    private void showBookmarksDialog() {
        final EditText etName = new EditText(this);
        etName.setText(bookmarkName);
        etName.setHint("Bookmark name");

        StringBuilder bmText = new StringBuilder();
        if (bookmarks.isEmpty()) {
            bmText.append("No bookmarks saved.");
        } else {
            bmText.append("Saved bookmarks:\n");
            for (Bookmark bm : bookmarks) {
                bmText.append("• ").append(bm.name)
                      .append(String.format(Locale.US, " (%.4f, %.4f)\n", bm.lng, bm.lat));
            }
        }
        final TextView tvBookmarks = new TextView(this);
        tvBookmarks.setText(bmText.toString());

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(dp(16), dp(8), dp(16), dp(8));
        layout.addView(etName);
        layout.addView(tvBookmarks);

        ScrollView sv = new ScrollView(this);
        sv.addView(layout);

        new AlertDialog.Builder(this)
                .setTitle("Bookmarks")
                .setView(sv)
                .setPositiveButton("Bookmark View", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        bookmarkName = etName.getText().toString().trim();
                        if (bookmarkName.isEmpty()) { bookmarkName = "Bookmark"; }
                        addBookmark(bookmarkName);
                    }
                })
                .setNeutralButton("Fly to…", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        showFlyToBookmarkDialog();
                    }
                })
                .setNegativeButton("Close", null)
                .show();
    }

    private void addBookmark(String name) {
        CameraPosition cam = map.getCameraPosition();
        Bookmark bm = new Bookmark();
        bm.name   = name;
        bm.lng    = cam.longitude;
        bm.lat    = cam.latitude;
        bm.marker = map.addMarker();
        bm.marker.setPoint(new LngLat(bm.lng, bm.lat));
        String styling = "{ style: text, text_source: function(){return '" + name + "';}, "
                + "font: { size: 11px, fill: '#c040c0', stroke: { color: white, width: 3 } } }";
        bm.marker.setStylingFromString(styling);
        bookmarks.add(bm);
        Log.d(TAG, "Bookmark: " + name + " at " + bm.lng + ", " + bm.lat);
    }

    private void showFlyToBookmarkDialog() {
        if (bookmarks.isEmpty()) {
            Toast.makeText(this, "No bookmarks saved yet.", Toast.LENGTH_SHORT).show();
            return;
        }
        final String[] items = new String[bookmarks.size()];
        for (int i = 0; i < bookmarks.size(); i++) {
            Bookmark bm = bookmarks.get(i);
            items[i] = String.format(Locale.US, "%s (%.4f, %.4f)", bm.name, bm.lng, bm.lat);
        }
        new AlertDialog.Builder(this)
                .setTitle("Fly to Bookmark")
                .setItems(items, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        Bookmark bm = bookmarks.get(which);
                        map.updateCameraPosition(
                                CameraUpdateFactory.newLngLatZoom(new LngLat(bm.lng, bm.lat), 15),
                                800, null);
                    }
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    // -------------------------------------------------------------------------
    // 6. 3D Terrain
    // -------------------------------------------------------------------------

    private void showTerrainDialog() {
        String state = terrain3dEnabled ? "ENABLED" : "DISABLED";
        String action = terrain3dEnabled ? "Disable 3D Terrain" : "Enable 3D Terrain";
        new AlertDialog.Builder(this)
                .setTitle("3D Terrain")
                .setMessage("3D terrain is currently " + state + ".\n"
                        + "Toggles the terrain-3d.yaml scene import via SceneUpdates.")
                .setPositiveButton(action, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) { toggleTerrain3d(); }
                })
                .setNegativeButton("Close", null)
                .show();
    }

    private void toggleTerrain3d() {
        terrain3dEnabled = !terrain3dEnabled;
        if (terrain3dEnabled) {
            map.loadSceneFileAsync(getCurrentSceneUrl(), Arrays.asList(
                    new SceneUpdate("import", "scenes/terrain-3d.yaml"),
                    new SceneUpdate("global.show_land_polygons", "false")
            ));
        } else {
            map.loadSceneFileAsync(getCurrentSceneUrl(), Arrays.asList(
                    new SceneUpdate("global.show_land_polygons", "true")
            ));
        }
        Log.d(TAG, "3D terrain: " + terrain3dEnabled);
    }

    // -------------------------------------------------------------------------
    // 7. Custom Shader Rendering
    // -------------------------------------------------------------------------

    private void showCustomShaderDialog() {
        final String[] items = {
                "Enable Hillshade",
                "Enable Slope Angle",
                "Enable Custom Slope",
                "Disable All Overlays"
        };
        new AlertDialog.Builder(this)
                .setTitle("Custom Shader Rendering")
                .setItems(items, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        switch (which) {
                            case 0:
                                map.loadSceneFileAsync(getCurrentSceneUrl(), Arrays.asList(
                                        new SceneUpdate("import", "scenes/hillshade.yaml"),
                                        new SceneUpdate("global.show_hypsometric", "true")
                                ));
                                break;
                            case 1:
                                map.loadSceneFileAsync(getCurrentSceneUrl(), Arrays.asList(
                                        new SceneUpdate("import", "scenes/slope-angle.yaml"),
                                        new SceneUpdate("hillshade.shaders.uniforms.u_slope_angle_opacity", "0.75")
                                ));
                                break;
                            case 2:
                                map.loadSceneFileAsync(getCurrentSceneUrl(), Arrays.asList(
                                        new SceneUpdate("import", "scenes/custom-slope-shading.yaml"),
                                        new SceneUpdate("hillshade.shaders.uniforms.u_custom_slope_opacity", "0.75")
                                ));
                                break;
                            case 3:
                                map.loadSceneFileAsync(getCurrentSceneUrl(), Arrays.asList(
                                        new SceneUpdate("global.show_hypsometric", "false"),
                                        new SceneUpdate("hillshade.shaders.uniforms.u_slope_angle_opacity", "0.0"),
                                        new SceneUpdate("hillshade.shaders.uniforms.u_custom_slope_opacity", "0.0")
                                ));
                                break;
                        }
                    }
                })
                .setNegativeButton("Close", null)
                .show();
    }

    // -------------------------------------------------------------------------
    // 8. Selection Highlighting
    // -------------------------------------------------------------------------

    private void showSelectionDialog() {
        final EditText etOsmId = new EditText(this);
        etOsmId.setText(selectedOsmId);
        etOsmId.setHint("OSM ID");

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(dp(16), dp(8), dp(16), dp(8));

        TextView tvDesc = new TextView(this);
        tvDesc.setText("Set global.selected_osm_id to highlight a feature.\n"
                + "Tap a feature to see its OSM ID in logcat, then enter it here.");
        layout.addView(tvDesc);
        layout.addView(etOsmId);

        new AlertDialog.Builder(this)
                .setTitle("Selection Highlighting")
                .setView(layout)
                .setPositiveButton("Highlight", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        selectedOsmId = etOsmId.getText().toString().trim();
                        map.loadSceneFileAsync(getCurrentSceneUrl(), Arrays.asList(
                                new SceneUpdate("global.selected_osm_id", selectedOsmId)
                        ));
                        Log.d(TAG, "Highlight OSM ID: " + selectedOsmId);
                    }
                })
                .setNeutralButton("Clear", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        selectedOsmId = "";
                        etOsmId.setText("");
                        map.loadSceneFileAsync(getCurrentSceneUrl(), Arrays.asList(
                                new SceneUpdate("global.selected_osm_id", "")
                        ));
                        Log.d(TAG, "Selection highlight cleared");
                    }
                })
                .setNegativeButton("Close", null)
                .show();
    }
}
