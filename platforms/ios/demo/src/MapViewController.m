//
//  MapViewController.m
//  Tangram-NG iOS demo
//
//  Demonstrates the hillshade/slope features from AscendMaps scenes and the following
//  examples (mirroring the desktop glfwApp "Examples" panel):
//
//   1. Drawn Sources / Layers  – add GeoJSON data sources at runtime via SceneUpdates
//   2. Routes                  – build a route polyline by tapping the map
//   3. Local Search            – pick the nearest named feature at the map centre
//   4. Search Results          – manage and fly to picked features
//   5. Bookmarks               – save and recall camera positions
//   6. 3D Terrain              – toggle terrain-3d.yaml import via SceneUpdates
//   7. Custom Shader Rendering – enable hillshade/slope overlays
//   8. Selection Highlighting  – highlight a feature by OSM ID
//
//  Initial camera: Austrian Alps (Hohe Tauern) lng 12.80 / lat 47.32 / zoom 10
//

#import "MapViewController.h"
#import <CoreLocation/CoreLocation.h>

// Hillshade demo scene — imports hillshade.yaml, slope-angle.yaml, custom-slope-shading.yaml
static NSString * const kSceneHillshadeDemo = @"asset:///scene-hillshade-demo.yaml";

// ---- config.default.yaml values used in this demo ----
static const BOOL  kConfigMetricUnits = YES;
static const float kConfigViewZoom    = 10.f;
static const double kDemoLng = 12.80;
static const double kDemoLat = 47.32;

// Slope mode constants
typedef NS_ENUM(NSInteger, SlopeMode) {
    SlopeModeOff = 0,
    SlopeModeAngle,
    SlopeModeCustom
};

@interface MapViewController () <CLLocationManagerDelegate> {
    // Hillshade demo state
    SlopeMode  _slopeMode;
    BOOL       _satelliteLayerEnabled;
    BOOL       _contoursEnabled;

    // Examples state (mirrors glfwApp global variables)
    NSMutableArray<NSMutableDictionary *> *_drawnLayers;   // @{@"name": ...}
    NSString *_drawnLayerName;
    NSString *_drawnLayerGeoJson;

    BOOL _addRouteWaypointOnClick;
    NSMutableArray<NSValue *> *_routeWaypoints;            // CLLocationCoordinate2D in NSValue
    TGMarker *_routePolylineMarker;

    BOOL _localSearchActive;
    NSString *_searchQuery;
    NSMutableArray<NSMutableDictionary *> *_searchResults; // @{@"name":, @"lng":, @"lat":, @"marker":}

    NSString *_bookmarkName;
    NSMutableArray<NSMutableDictionary *> *_bookmarks;     // @{@"name":, @"lng":, @"lat":, @"marker":}

    BOOL _terrain3dEnabled;
    NSString *_selectedOsmId;
}

@property (strong, nonatomic) UIButton *btnSlope;
@property (strong, nonatomic) UIButton *btnLayer;
@property (strong, nonatomic) UIButton *btnContours;
@property (strong, nonatomic) UIButton *btnExamples;
@property (strong, nonatomic) CLLocationManager *locationManager;

- (void)addControlButtons;

// Hillshade
- (void)toggleSlope;
- (void)toggleSatelliteLayer;
- (void)toggleContours;
- (NSArray<TGSceneUpdate *> *)buildSceneUpdates;
- (void)updateButtonLabels;
- (NSString *)slopeModeLabel;

// Examples
- (void)showExamplesMenu;
- (void)showDrawnSourcesDialog;
- (void)showRoutesDialog;
- (void)showLocalSearchDialog;
- (void)showSearchResultsDialog;
- (void)showBookmarksDialog;
- (void)showTerrainDialog;
- (void)showCustomShaderDialog;
- (void)showSelectionDialog;

@end

@implementation MapViewController

#pragma mark - View Controller

- (void)viewDidLoad
{
    [super viewDidLoad];

    _slopeMode              = SlopeModeOff;
    _satelliteLayerEnabled  = NO;
    _contoursEnabled        = YES;

    // Examples initial state
    _drawnLayers       = [NSMutableArray array];
    _drawnLayerName    = @"my-layer";
    _drawnLayerGeoJson = @"{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\","
                          "\"geometry\":{\"type\":\"Point\",\"coordinates\":[0,0]},"
                          "\"properties\":{\"name\":\"Origin\"}}]}";

    _addRouteWaypointOnClick = NO;
    _routeWaypoints          = [NSMutableArray array];
    _routePolylineMarker     = nil;

    _localSearchActive = NO;
    _searchQuery       = @"";
    _searchResults     = [NSMutableArray array];

    _bookmarkName = @"My Place";
    _bookmarks    = [NSMutableArray array];

    _terrain3dEnabled = NO;
    _selectedOsmId    = @"";

    TGMapView *mapView = (TGMapView *)self.view;
    mapView.mapViewDelegate = self;
    mapView.gestureDelegate = self;

    self.locationManager = [[CLLocationManager alloc] init];
    self.locationManager.delegate = self;

    [self addControlButtons];
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];

    TGMapView *mapView = (TGMapView *)self.view;

    CLLocationCoordinate2D alps = CLLocationCoordinate2DMake(kDemoLat, kDemoLng);
    TGCameraPosition *camera = [[TGCameraPosition alloc] initWithCenter:alps
                                                                   zoom:kConfigViewZoom
                                                                bearing:0
                                                                  pitch:0];
    [mapView setCameraPosition:camera];

    [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                       withUpdates:nil];
}

#pragma mark - Control Buttons

- (void)addControlButtons
{
    UIView *controlBar = [[UIView alloc] init];
    controlBar.backgroundColor = [UIColor colorWithWhite:0 alpha:0.7];
    controlBar.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:controlBar];

    // ---- Row 1: hillshade controls ----

    // Slope toggle
    self.btnSlope = [UIButton buttonWithType:UIButtonTypeSystem];
    self.btnSlope.translatesAutoresizingMaskIntoConstraints = NO;
    [self.btnSlope setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    self.btnSlope.titleLabel.font = [UIFont systemFontOfSize:12];
    [self.btnSlope addTarget:self action:@selector(toggleSlope)
            forControlEvents:UIControlEventTouchUpInside];
    [controlBar addSubview:self.btnSlope];

    // Satellite layer toggle
    self.btnLayer = [UIButton buttonWithType:UIButtonTypeSystem];
    self.btnLayer.translatesAutoresizingMaskIntoConstraints = NO;
    [self.btnLayer setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    self.btnLayer.titleLabel.font = [UIFont systemFontOfSize:12];
    [self.btnLayer addTarget:self action:@selector(toggleSatelliteLayer)
            forControlEvents:UIControlEventTouchUpInside];
    [controlBar addSubview:self.btnLayer];

    // Contours toggle
    self.btnContours = [UIButton buttonWithType:UIButtonTypeSystem];
    self.btnContours.translatesAutoresizingMaskIntoConstraints = NO;
    [self.btnContours setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    self.btnContours.titleLabel.font = [UIFont systemFontOfSize:12];
    [self.btnContours addTarget:self action:@selector(toggleContours)
               forControlEvents:UIControlEventTouchUpInside];
    [controlBar addSubview:self.btnContours];

    // ---- Row 2: examples button ----
    self.btnExamples = [UIButton buttonWithType:UIButtonTypeSystem];
    self.btnExamples.translatesAutoresizingMaskIntoConstraints = NO;
    [self.btnExamples setTitle:@"⚙ Examples" forState:UIControlStateNormal];
    [self.btnExamples setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    self.btnExamples.titleLabel.font = [UIFont systemFontOfSize:12];
    [self.btnExamples addTarget:self action:@selector(showExamplesMenu)
               forControlEvents:UIControlEventTouchUpInside];
    [controlBar addSubview:self.btnExamples];

    [self updateButtonLabels];

    [NSLayoutConstraint activateConstraints:@[
        [controlBar.leadingAnchor  constraintEqualToAnchor:self.view.leadingAnchor],
        [controlBar.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [controlBar.bottomAnchor   constraintEqualToAnchor:self.view.bottomAnchor],
        [controlBar.heightAnchor   constraintEqualToConstant:96],

        // Row 1
        [self.btnSlope.leadingAnchor  constraintEqualToAnchor:controlBar.leadingAnchor],
        [self.btnSlope.topAnchor      constraintEqualToAnchor:controlBar.topAnchor],
        [self.btnSlope.heightAnchor   constraintEqualToConstant:48],
        [self.btnSlope.widthAnchor    constraintEqualToAnchor:controlBar.widthAnchor
                                                   multiplier:1.0/3.0],

        [self.btnLayer.centerXAnchor  constraintEqualToAnchor:controlBar.centerXAnchor],
        [self.btnLayer.topAnchor      constraintEqualToAnchor:controlBar.topAnchor],
        [self.btnLayer.heightAnchor   constraintEqualToConstant:48],
        [self.btnLayer.widthAnchor    constraintEqualToAnchor:controlBar.widthAnchor
                                                   multiplier:1.0/3.0],

        [self.btnContours.trailingAnchor constraintEqualToAnchor:controlBar.trailingAnchor],
        [self.btnContours.topAnchor      constraintEqualToAnchor:controlBar.topAnchor],
        [self.btnContours.heightAnchor   constraintEqualToConstant:48],
        [self.btnContours.widthAnchor    constraintEqualToAnchor:controlBar.widthAnchor
                                                      multiplier:1.0/3.0],

        // Row 2
        [self.btnExamples.leadingAnchor  constraintEqualToAnchor:controlBar.leadingAnchor],
        [self.btnExamples.trailingAnchor constraintEqualToAnchor:controlBar.trailingAnchor],
        [self.btnExamples.bottomAnchor   constraintEqualToAnchor:controlBar.bottomAnchor],
        [self.btnExamples.heightAnchor   constraintEqualToConstant:48],
    ]];
}

#pragma mark - Slope Overlay Toggle

- (void)toggleSlope
{
    _slopeMode = (_slopeMode + 1) % 3;

    NSString *slopeAngleOpacity  = (_slopeMode == SlopeModeAngle)  ? @"0.75" : @"0.0";
    NSString *customSlopeOpacity = (_slopeMode == SlopeModeCustom) ? @"0.75" : @"0.0";

    TGMapView *mapView = (TGMapView *)self.view;
    [mapView updateGlobals:@[
        [[TGSceneUpdate alloc] initWithPath:@"global.slope_angle_opacity"  value:slopeAngleOpacity],
        [[TGSceneUpdate alloc] initWithPath:@"global.custom_slope_opacity" value:customSlopeOpacity]
    ] rebuildTiles:YES];

    [self updateButtonLabels];
    NSLog(@"Slope mode: %@", [self slopeModeLabel]);
}

#pragma mark - Satellite Layer Toggle

- (void)toggleSatelliteLayer
{
    _satelliteLayerEnabled = !_satelliteLayerEnabled;

    TGMapView *mapView = (TGMapView *)self.view;
    [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                       withUpdates:[self buildSceneUpdates]];

    [self updateButtonLabels];
    NSLog(@"Satellite layer: %@", _satelliteLayerEnabled ? @"ON" : @"OFF");
}

- (NSArray<TGSceneUpdate *> *)buildSceneUpdates
{
    if (!_satelliteLayerEnabled) { return @[]; }
    return @[
        [[TGSceneUpdate alloc] initWithPath:@"sources.arcgis-satellite.type"
                                      value:@"Raster"],
        [[TGSceneUpdate alloc] initWithPath:@"sources.arcgis-satellite.url"
                                      value:@"https://server.arcgisonline.com/arcgis/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"],
        [[TGSceneUpdate alloc] initWithPath:@"sources.arcgis-satellite.max_zoom"
                                      value:@"19"],
        [[TGSceneUpdate alloc] initWithPath:@"layers.arcgis-satellite-layer.data.source"
                                      value:@"arcgis-satellite"],
        [[TGSceneUpdate alloc] initWithPath:@"layers.arcgis-satellite-layer.draw.satellite-overlay.order"
                                      value:@"500"],
    ];
}

#pragma mark - Contour Lines Toggle

- (void)toggleContours
{
    _contoursEnabled = !_contoursEnabled;

    TGMapView *mapView = (TGMapView *)self.view;
    [mapView updateGlobals:@[
        [[TGSceneUpdate alloc] initWithPath:@"global.show_contours"
                                      value:_contoursEnabled ? @"true" : @"false"],
        [[TGSceneUpdate alloc] initWithPath:@"global.metric_units"
                                      value:kConfigMetricUnits ? @"true" : @"false"],
    ] rebuildTiles:YES];

    [self updateButtonLabels];
    NSLog(@"Contours: %@ (metric=%@)", _contoursEnabled ? @"ON" : @"OFF",
          kConfigMetricUnits ? @"YES" : @"NO");
}

#pragma mark - Helpers

- (NSString *)slopeModeLabel
{
    switch (_slopeMode) {
        case SlopeModeAngle:  return @"Slope Angle";
        case SlopeModeCustom: return @"Custom Slope";
        default:              return @"OFF";
    }
}

- (void)updateButtonLabels
{
    [self.btnSlope    setTitle:[NSString stringWithFormat:@"Slope: %@", [self slopeModeLabel]]
                      forState:UIControlStateNormal];
    [self.btnLayer    setTitle:[NSString stringWithFormat:@"Satellite: %@",
                                _satelliteLayerEnabled ? @"ON" : @"OFF"]
                      forState:UIControlStateNormal];
    [self.btnContours setTitle:[NSString stringWithFormat:@"Contours: %@",
                                _contoursEnabled ? @"ON" : @"OFF"]
                      forState:UIControlStateNormal];
}

#pragma mark - TGMapViewDelegate

- (void)mapView:(TGMapView *)mapView didLoadScene:(int)sceneID withError:(nullable NSError *)sceneError
{
    if (sceneError) {
        NSLog(@"Scene load error: %@", sceneError);
        return;
    }
    NSLog(@"Scene loaded: %d", sceneID);

    if (_slopeMode != SlopeModeOff) {
        NSString *slopeAngleOpacity  = (_slopeMode == SlopeModeAngle)  ? @"0.75" : @"0.0";
        NSString *customSlopeOpacity = (_slopeMode == SlopeModeCustom) ? @"0.75" : @"0.0";
        [mapView updateGlobals:@[
            [[TGSceneUpdate alloc] initWithPath:@"global.slope_angle_opacity"  value:slopeAngleOpacity],
            [[TGSceneUpdate alloc] initWithPath:@"global.custom_slope_opacity" value:customSlopeOpacity],
        ] rebuildTiles:YES];
    }
}

- (void)mapViewDidCompleteLoading:(TGMapView *)mapView
{
    NSLog(@"Map view did complete loading");
}

- (void)mapView:(TGMapView *)mapView regionDidChangeAnimated:(BOOL)animated
{
    // no-op
}

- (void)mapView:(TGMapView *)mapView didSelectFeature:(nullable NSDictionary *)feature
    atScreenPosition:(CGPoint)position
{
    if (!feature) { return; }
    NSString *name = feature[@"name"];

    // Local Search intercepts the next feature pick to store it as a search result.
    if (_localSearchActive) {
        _localSearchActive = NO;
        if (!name || name.length == 0) {
            NSLog(@"Local search: no named feature at centre");
            return;
        }
        CLLocationCoordinate2D coord = [mapView coordinateFromViewPosition:position];
        [self addSearchResult:name atCoordinate:coord];
        return;
    }

    NSLog(@"Feature selected: %@", name ?: @"(no name)");
}

- (void)mapView:(TGMapView *)mapView didSelectLabel:(nullable TGLabelPickResult *)labelPickResult
    atScreenPosition:(CGPoint)position
{
    if (!labelPickResult) { return; }
    NSLog(@"Label selected: %@", labelPickResult.properties[@"name"] ?: @"(no name)");
}

- (void)mapView:(TGMapView *)mapView didSelectMarker:(nullable TGMarkerPickResult *)markerPickResult
    atScreenPosition:(CGPoint)position
{
    if (!markerPickResult) { return; }
    NSLog(@"Marker selected");
}

- (void)mapView:(TGMapView *)mapView didCaptureScreenshot:(UIImage *)screenshot
{
    NSLog(@"Screenshot captured");
}

#pragma mark - TGRecognizerDelegate

- (void)mapView:(TGMapView *)mapView recognizer:(UIGestureRecognizer *)recognizer
        didRecognizeSingleTapGesture:(CGPoint)location
{
    // Routes: add a waypoint when the mode is active
    if (_addRouteWaypointOnClick) {
        CLLocationCoordinate2D coord = [mapView coordinateFromViewPosition:location];
        [self addRouteWaypoint:coord];
    }

    [mapView pickFeatureAt:location];
    [mapView pickLabelAt:location];

    CLLocationCoordinate2D coords = [mapView coordinateFromViewPosition:location];
    TGCameraPosition *camera = [mapView cameraPosition];
    camera.center = coords;
    [mapView setCameraPosition:camera withDuration:0.4
                      easeType:TGEaseTypeCubic
                      callback:nil];
}

- (void)mapView:(TGMapView *)mapView recognizer:(UIGestureRecognizer *)recognizer
        didRecognizeLongPressGesture:(CGPoint)location
{
    NSLog(@"Long press at %.1f, %.1f", location.x, location.y);
}

#pragma mark - CLLocationManagerDelegate

- (void)locationManager:(CLLocationManager *)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status
{
    // Location tracking not used in this demo
}

// =========================================================================
#pragma mark - Examples (mirrors glfwApp showExamplesGUI)
// =========================================================================

- (void)showExamplesMenu
{
    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:@"Examples"
                         message:nil
                  preferredStyle:UIAlertControllerStyleActionSheet];

    __weak __typeof(self) weakSelf = self;

    [alert addAction:[UIAlertAction actionWithTitle:@"Drawn Sources / Layers"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            [weakSelf showDrawnSourcesDialog];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Routes"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            [weakSelf showRoutesDialog];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Local Search"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            [weakSelf showLocalSearchDialog];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:[NSString stringWithFormat:@"Search Results (%lu)",
                                                    (unsigned long)_searchResults.count]
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            [weakSelf showSearchResultsDialog];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:[NSString stringWithFormat:@"Bookmarks (%lu)",
                                                    (unsigned long)_bookmarks.count]
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            [weakSelf showBookmarksDialog];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"3D Terrain"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            [weakSelf showTerrainDialog];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Custom Shader Rendering"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            [weakSelf showCustomShaderDialog];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Selection Highlighting"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            [weakSelf showSelectionDialog];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
        style:UIAlertActionStyleCancel handler:nil]];

    // iPad: anchor to the button
    alert.popoverPresentationController.sourceView = self.btnExamples;
    alert.popoverPresentationController.sourceRect = self.btnExamples.bounds;

    [self presentViewController:alert animated:YES completion:nil];
}

// -------------------------------------------------------------------------
// 1. Drawn Sources / Layers
// -------------------------------------------------------------------------

- (void)showDrawnSourcesDialog
{
    NSMutableString *info = [NSMutableString string];
    if (_drawnLayers.count == 0) {
        [info appendString:@"No layers added yet."];
    } else {
        [info appendString:@"Active layers:"];
        for (NSDictionary *dl in _drawnLayers) {
            [info appendFormat:@" %@", dl[@"name"]];
        }
    }

    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:@"Drawn Sources / Layers"
                         message:info
                  preferredStyle:UIAlertControllerStyleAlert];

    [alert addTextFieldWithConfigurationHandler:^(UITextField *tf) {
        tf.placeholder = @"Source name";
        tf.text = self->_drawnLayerName;
    }];
    [alert addTextFieldWithConfigurationHandler:^(UITextField *tf) {
        tf.placeholder = @"GeoJSON";
        tf.text = self->_drawnLayerGeoJson;
    }];

    __weak __typeof(self) weakSelf = self;

    [alert addAction:[UIAlertAction actionWithTitle:@"Add Layer"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            NSString *name    = alert.textFields[0].text ?: @"my-layer";
            NSString *geojson = alert.textFields[1].text ?: @"";
            if (name.length && geojson.length) {
                weakSelf->_drawnLayerName    = name;
                weakSelf->_drawnLayerGeoJson = geojson;
                [weakSelf addDrawnLayerNamed:name geojson:geojson];
            }
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Clear All"
        style:UIAlertActionStyleDestructive handler:^(UIAlertAction *a) {
            [weakSelf clearDrawnLayers];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Close"
        style:UIAlertActionStyleCancel handler:nil]];

    [self presentViewController:alert animated:YES completion:nil];
}

- (void)addDrawnLayerNamed:(NSString *)name geojson:(NSString *)geojson
{
    TGMapView *mapView = (TGMapView *)self.view;
    [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                       withUpdates:@[
        [[TGSceneUpdate alloc] initWithPath:[NSString stringWithFormat:@"sources.%@.type", name]
                                      value:@"GeoJSON"],
        [[TGSceneUpdate alloc] initWithPath:[NSString stringWithFormat:@"sources.%@.data", name]
                                      value:geojson],
        [[TGSceneUpdate alloc] initWithPath:[NSString stringWithFormat:@"layers.%@.data.source", name]
                                      value:name],
        [[TGSceneUpdate alloc] initWithPath:[NSString stringWithFormat:@"layers.%@.draw.points.color", name]
                                      value:@"red"],
        [[TGSceneUpdate alloc] initWithPath:[NSString stringWithFormat:@"layers.%@.draw.points.size", name]
                                      value:@"12px"],
        [[TGSceneUpdate alloc] initWithPath:[NSString stringWithFormat:@"layers.%@.draw.points.order", name]
                                      value:@"5000"],
    ]];
    [_drawnLayers addObject:[NSMutableDictionary dictionaryWithObject:name forKey:@"name"]];
    NSLog(@"Added drawn layer: %@", name);
}

- (void)clearDrawnLayers
{
    TGMapView *mapView = (TGMapView *)self.view;
    for (NSDictionary *dl in _drawnLayers) {
        NSString *name = dl[@"name"];
        [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                           withUpdates:@[
            [[TGSceneUpdate alloc] initWithPath:[NSString stringWithFormat:@"layers.%@.draw.points.size", name]
                                          value:@"0px"],
        ]];
    }
    [_drawnLayers removeAllObjects];
    NSLog(@"Drawn layers cleared");
}

// -------------------------------------------------------------------------
// 2. Routes
// -------------------------------------------------------------------------

- (void)showRoutesDialog
{
    NSMutableString *waypointText = [NSMutableString string];
    if (_routeWaypoints.count == 0) {
        [waypointText appendString:@"No waypoints."];
    } else {
        [waypointText appendFormat:@"Waypoints: %lu\n", (unsigned long)_routeWaypoints.count];
        for (NSUInteger i = 0; i < _routeWaypoints.count; i++) {
            CLLocationCoordinate2D c;
            [_routeWaypoints[i] getValue:&c];
            [waypointText appendFormat:@"WP%lu: %.5f, %.5f\n", (unsigned long)(i+1), c.longitude, c.latitude];
        }
    }

    NSString *toggleTitle = _addRouteWaypointOnClick
        ? @"Disable Waypoint Tap"
        : @"Enable Waypoint Tap";

    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:@"Routes"
                         message:waypointText
                  preferredStyle:UIAlertControllerStyleAlert];

    __weak __typeof(self) weakSelf = self;

    [alert addAction:[UIAlertAction actionWithTitle:toggleTitle
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            weakSelf->_addRouteWaypointOnClick = !weakSelf->_addRouteWaypointOnClick;
            NSLog(@"Route waypoint on tap: %@", weakSelf->_addRouteWaypointOnClick ? @"ON" : @"OFF");
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Clear Route"
        style:UIAlertActionStyleDestructive handler:^(UIAlertAction *a) {
            [weakSelf clearRoute];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Close"
        style:UIAlertActionStyleCancel handler:nil]];

    [self presentViewController:alert animated:YES completion:nil];
}

- (void)addRouteWaypoint:(CLLocationCoordinate2D)coord
{
    NSValue *val = [NSValue value:&coord withObjCType:@encode(CLLocationCoordinate2D)];
    [_routeWaypoints addObject:val];

    TGMapView *mapView = (TGMapView *)self.view;
    if (_routePolylineMarker == nil) {
        _routePolylineMarker = [mapView markerAdd];
        _routePolylineMarker.stylingString = @"{ style: lines, color: blue, width: 6px, order: 5001 }";
    }
    if (_routeWaypoints.count >= 2) {
        NSUInteger count = _routeWaypoints.count;
        CLLocationCoordinate2D *pts = (CLLocationCoordinate2D *)malloc(count * sizeof(CLLocationCoordinate2D));
        for (NSUInteger i = 0; i < count; i++) {
            [_routeWaypoints[i] getValue:&pts[i]];
        }
        _routePolylineMarker.polyline = [[TGGeoPolyline alloc] initWithCoordinates:pts count:count];
        free(pts);
    }
    NSLog(@"Route waypoint: %.5f, %.5f", coord.longitude, coord.latitude);
}

- (void)clearRoute
{
    [_routeWaypoints removeAllObjects];
    if (_routePolylineMarker) {
        TGMapView *mapView = (TGMapView *)self.view;
        [mapView markerRemove:_routePolylineMarker];
        _routePolylineMarker = nil;
    }
    NSLog(@"Route cleared");
}

// -------------------------------------------------------------------------
// 3. Local Search in Vector Tiles
// -------------------------------------------------------------------------

- (void)showLocalSearchDialog
{
    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:@"Local Search in Vector Tiles"
                         message:@"Picks the nearest named feature at the map centre."
                  preferredStyle:UIAlertControllerStyleAlert];

    [alert addTextFieldWithConfigurationHandler:^(UITextField *tf) {
        tf.placeholder = @"Feature name (display only)";
        tf.text = self->_searchQuery;
    }];

    __weak __typeof(self) weakSelf = self;

    [alert addAction:[UIAlertAction actionWithTitle:@"Search at Centre"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            weakSelf->_searchQuery = alert.textFields[0].text ?: @"";
            [weakSelf pickFeatureAtCenter];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Close"
        style:UIAlertActionStyleCancel handler:nil]];

    [self presentViewController:alert animated:YES completion:nil];
}

- (void)pickFeatureAtCenter
{
    TGMapView *mapView = (TGMapView *)self.view;
    _localSearchActive = YES;
    CGPoint centre = CGPointMake(CGRectGetMidX(mapView.bounds), CGRectGetMidY(mapView.bounds));
    [mapView pickFeatureAt:centre];
    NSLog(@"Local search: picking at centre (%.1f, %.1f)", centre.x, centre.y);
}

- (void)addSearchResult:(NSString *)name atCoordinate:(CLLocationCoordinate2D)coord
{
    TGMapView *mapView = (TGMapView *)self.view;
    TGMarker *marker = [mapView markerAdd];
    marker.point = coord;
    marker.stylingString = @"{ style: text, text_source: function(){return 'RESULT';}, "
                            "font: { size: 10px, fill: orange } }";
    NSMutableDictionary *sr = [NSMutableDictionary dictionary];
    sr[@"name"]   = name;
    sr[@"lng"]    = @(coord.longitude);
    sr[@"lat"]    = @(coord.latitude);
    sr[@"marker"] = marker;
    [_searchResults addObject:sr];
    NSLog(@"Search result: %@ at %.5f, %.5f", name, coord.longitude, coord.latitude);
}

// -------------------------------------------------------------------------
// 4. Search Results
// -------------------------------------------------------------------------

- (void)showSearchResultsDialog
{
    if (_searchResults.count == 0) {
        UIAlertController *alert = [UIAlertController
            alertControllerWithTitle:@"Search Results"
                             message:@"No results yet. Use 'Local Search' to pick features."
                      preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK"
            style:UIAlertActionStyleCancel handler:nil]];
        [self presentViewController:alert animated:YES completion:nil];
        return;
    }

    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:@"Search Results"
                         message:@"Tap a result to fly to it."
                  preferredStyle:UIAlertControllerStyleActionSheet];

    __weak __typeof(self) weakSelf = self;

    for (NSDictionary *sr in _searchResults) {
        NSString *title = [NSString stringWithFormat:@"%@ (%.4f, %.4f)",
                           sr[@"name"], [sr[@"lng"] doubleValue], [sr[@"lat"] doubleValue]];
        [alert addAction:[UIAlertAction actionWithTitle:title
            style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
                TGMapView *mapView = (TGMapView *)weakSelf.view;
                CLLocationCoordinate2D coord =
                    CLLocationCoordinate2DMake([sr[@"lat"] doubleValue], [sr[@"lng"] doubleValue]);
                TGCameraPosition *cam = [[TGCameraPosition alloc]
                    initWithCenter:coord zoom:16 bearing:0 pitch:0];
                [mapView setCameraPosition:cam withDuration:0.8
                                  easeType:TGEaseTypeCubic callback:nil];
            }]];
    }

    [alert addAction:[UIAlertAction actionWithTitle:@"Clear All"
        style:UIAlertActionStyleDestructive handler:^(UIAlertAction *a) {
            [weakSelf clearSearchResults];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Close"
        style:UIAlertActionStyleCancel handler:nil]];

    alert.popoverPresentationController.sourceView = self.btnExamples;
    alert.popoverPresentationController.sourceRect = self.btnExamples.bounds;

    [self presentViewController:alert animated:YES completion:nil];
}

- (void)clearSearchResults
{
    TGMapView *mapView = (TGMapView *)self.view;
    for (NSDictionary *sr in _searchResults) {
        TGMarker *marker = sr[@"marker"];
        if (marker) { [mapView markerRemove:marker]; }
    }
    [_searchResults removeAllObjects];
    NSLog(@"Search results cleared");
}

// -------------------------------------------------------------------------
// 5. Bookmarks
// -------------------------------------------------------------------------

- (void)showBookmarksDialog
{
    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:@"Bookmarks"
                         message:nil
                  preferredStyle:UIAlertControllerStyleAlert];

    [alert addTextFieldWithConfigurationHandler:^(UITextField *tf) {
        tf.placeholder = @"Bookmark name";
        tf.text = self->_bookmarkName;
    }];

    __weak __typeof(self) weakSelf = self;

    [alert addAction:[UIAlertAction actionWithTitle:@"Bookmark Current View"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            NSString *name = alert.textFields[0].text;
            if (!name || name.length == 0) { name = @"Bookmark"; }
            weakSelf->_bookmarkName = name;
            [weakSelf addBookmarkNamed:name];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Fly to…"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            [weakSelf showFlyToBookmarkDialog];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Close"
        style:UIAlertActionStyleCancel handler:nil]];

    [self presentViewController:alert animated:YES completion:nil];
}

- (void)addBookmarkNamed:(NSString *)name
{
    TGMapView *mapView = (TGMapView *)self.view;
    TGCameraPosition *cam = [mapView cameraPosition];
    TGMarker *marker = [mapView markerAdd];
    marker.point = cam.center;
    marker.stylingString = [NSString stringWithFormat:
        @"{ style: text, text_source: function(){return '%@';}, "
         "font: { size: 11px, fill: '#c040c0', stroke: { color: white, width: 3 } } }", name];
    NSMutableDictionary *bm = [NSMutableDictionary dictionary];
    bm[@"name"]   = name;
    bm[@"lng"]    = @(cam.center.longitude);
    bm[@"lat"]    = @(cam.center.latitude);
    bm[@"marker"] = marker;
    [_bookmarks addObject:bm];
    NSLog(@"Bookmark: %@ at %.5f, %.5f", name, cam.center.longitude, cam.center.latitude);
}

- (void)showFlyToBookmarkDialog
{
    if (_bookmarks.count == 0) {
        UIAlertController *alert = [UIAlertController
            alertControllerWithTitle:@"Fly to Bookmark"
                             message:@"No bookmarks saved yet."
                      preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK"
            style:UIAlertActionStyleCancel handler:nil]];
        [self presentViewController:alert animated:YES completion:nil];
        return;
    }

    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:@"Fly to Bookmark"
                         message:nil
                  preferredStyle:UIAlertControllerStyleActionSheet];

    __weak __typeof(self) weakSelf = self;

    for (NSDictionary *bm in _bookmarks) {
        NSString *title = [NSString stringWithFormat:@"%@ (%.4f, %.4f)",
                           bm[@"name"], [bm[@"lng"] doubleValue], [bm[@"lat"] doubleValue]];
        [alert addAction:[UIAlertAction actionWithTitle:title
            style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
                TGMapView *mapView = (TGMapView *)weakSelf.view;
                CLLocationCoordinate2D coord =
                    CLLocationCoordinate2DMake([bm[@"lat"] doubleValue], [bm[@"lng"] doubleValue]);
                TGCameraPosition *cam = [[TGCameraPosition alloc]
                    initWithCenter:coord zoom:15 bearing:0 pitch:0];
                [mapView setCameraPosition:cam withDuration:0.8
                                  easeType:TGEaseTypeCubic callback:nil];
            }]];
    }

    [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
        style:UIAlertActionStyleCancel handler:nil]];

    alert.popoverPresentationController.sourceView = self.btnExamples;
    alert.popoverPresentationController.sourceRect = self.btnExamples.bounds;

    [self presentViewController:alert animated:YES completion:nil];
}

// -------------------------------------------------------------------------
// 6. 3D Terrain
// -------------------------------------------------------------------------

- (void)showTerrainDialog
{
    NSString *state  = _terrain3dEnabled ? @"ENABLED" : @"DISABLED";
    NSString *action = _terrain3dEnabled ? @"Disable 3D Terrain" : @"Enable 3D Terrain";
    NSString *msg    = [NSString stringWithFormat:
        @"3D terrain is currently %@.\nToggles the terrain-3d.yaml scene import via SceneUpdates.",
        state];

    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:@"3D Terrain"
                         message:msg
                  preferredStyle:UIAlertControllerStyleAlert];

    __weak __typeof(self) weakSelf = self;

    [alert addAction:[UIAlertAction actionWithTitle:action
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            [weakSelf toggleTerrain3d];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Close"
        style:UIAlertActionStyleCancel handler:nil]];

    [self presentViewController:alert animated:YES completion:nil];
}

- (void)toggleTerrain3d
{
    _terrain3dEnabled = !_terrain3dEnabled;
    TGMapView *mapView = (TGMapView *)self.view;
    if (_terrain3dEnabled) {
        [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                           withUpdates:@[
            [[TGSceneUpdate alloc] initWithPath:@"import"
                                          value:@"scenes/terrain-3d.yaml"],
            [[TGSceneUpdate alloc] initWithPath:@"global.show_land_polygons"
                                          value:@"false"],
        ]];
    } else {
        [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                           withUpdates:@[
            [[TGSceneUpdate alloc] initWithPath:@"global.show_land_polygons"
                                          value:@"true"],
        ]];
    }
    NSLog(@"3D terrain: %@", _terrain3dEnabled ? @"ON" : @"OFF");
}

// -------------------------------------------------------------------------
// 7. Custom Shader Rendering
// -------------------------------------------------------------------------

- (void)showCustomShaderDialog
{
    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:@"Custom Shader Rendering"
                         message:@"Activate slope/hillshade overlays via SceneUpdates."
                  preferredStyle:UIAlertControllerStyleActionSheet];

    __weak __typeof(self) weakSelf = self;

    [alert addAction:[UIAlertAction actionWithTitle:@"Enable Hillshade"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            TGMapView *mapView = (TGMapView *)weakSelf.view;
            [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                               withUpdates:@[
                [[TGSceneUpdate alloc] initWithPath:@"import"
                                              value:@"scenes/hillshade.yaml"],
                [[TGSceneUpdate alloc] initWithPath:@"global.show_hypsometric"
                                              value:@"true"],
            ]];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Enable Slope Angle"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            TGMapView *mapView = (TGMapView *)weakSelf.view;
            [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                               withUpdates:@[
                [[TGSceneUpdate alloc] initWithPath:@"import"
                                              value:@"scenes/slope-angle.yaml"],
                [[TGSceneUpdate alloc] initWithPath:@"hillshade.shaders.uniforms.u_slope_angle_opacity"
                                              value:@"0.75"],
            ]];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Enable Custom Slope"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            TGMapView *mapView = (TGMapView *)weakSelf.view;
            [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                               withUpdates:@[
                [[TGSceneUpdate alloc] initWithPath:@"import"
                                              value:@"scenes/custom-slope-shading.yaml"],
                [[TGSceneUpdate alloc] initWithPath:@"hillshade.shaders.uniforms.u_custom_slope_opacity"
                                              value:@"0.75"],
            ]];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Disable All Overlays"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            TGMapView *mapView = (TGMapView *)weakSelf.view;
            [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                               withUpdates:@[
                [[TGSceneUpdate alloc] initWithPath:@"global.show_hypsometric"
                                              value:@"false"],
                [[TGSceneUpdate alloc] initWithPath:@"hillshade.shaders.uniforms.u_slope_angle_opacity"
                                              value:@"0.0"],
                [[TGSceneUpdate alloc] initWithPath:@"hillshade.shaders.uniforms.u_custom_slope_opacity"
                                              value:@"0.0"],
            ]];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Close"
        style:UIAlertActionStyleCancel handler:nil]];

    alert.popoverPresentationController.sourceView = self.btnExamples;
    alert.popoverPresentationController.sourceRect = self.btnExamples.bounds;

    [self presentViewController:alert animated:YES completion:nil];
}

// -------------------------------------------------------------------------
// 8. Selection Highlighting
// -------------------------------------------------------------------------

- (void)showSelectionDialog
{
    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:@"Selection Highlighting"
                         message:@"Set global.selected_osm_id to highlight a feature.\n"
                                  "Tap a feature to see its OSM ID in the console."
                  preferredStyle:UIAlertControllerStyleAlert];

    [alert addTextFieldWithConfigurationHandler:^(UITextField *tf) {
        tf.placeholder = @"OSM ID";
        tf.text = self->_selectedOsmId;
    }];

    __weak __typeof(self) weakSelf = self;

    [alert addAction:[UIAlertAction actionWithTitle:@"Highlight"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *a) {
            weakSelf->_selectedOsmId = alert.textFields[0].text ?: @"";
            TGMapView *mapView = (TGMapView *)weakSelf.view;
            [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                               withUpdates:@[
                [[TGSceneUpdate alloc] initWithPath:@"global.selected_osm_id"
                                              value:weakSelf->_selectedOsmId],
            ]];
            NSLog(@"Highlight OSM ID: %@", weakSelf->_selectedOsmId);
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Clear"
        style:UIAlertActionStyleDestructive handler:^(UIAlertAction *a) {
            weakSelf->_selectedOsmId = @"";
            TGMapView *mapView = (TGMapView *)weakSelf.view;
            [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                               withUpdates:@[
                [[TGSceneUpdate alloc] initWithPath:@"global.selected_osm_id" value:@""],
            ]];
            NSLog(@"Selection highlight cleared");
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Close"
        style:UIAlertActionStyleCancel handler:nil]];

    [self presentViewController:alert animated:YES completion:nil];
}

@end
