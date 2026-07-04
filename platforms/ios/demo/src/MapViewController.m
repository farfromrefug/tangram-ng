//
//  MapViewController.m
//  Hillshade/slope feature demo for iOS
//
//  Demonstrates use of scene files from:
//  https://github.com/farfromrefug/ascendmaps/tree/abd695a8f13e7c0cfffc96b4daf4f19a336cceda/assets
//
//  Features shown:
//   - Hillshading (scenes/hillshade.yaml, elevation from ArcGIS WorldElevation3D)
//   - Slope overlay toggle (slope-angle / custom-slope-shading) via updateGlobals (no reload)
//   - Custom raster layer (ArcGIS satellite from mapsources.default.yaml) via SceneUpdates + reload
//   - Contour lines toggle; metric_units read from config.default.yaml
//
//  Initial camera: Austrian Alps (Hohe Tauern) lng 12.80 / lat 47.32 / zoom 10
//

#import "MapViewController.h"
#import <CoreLocation/CoreLocation.h>

// Hillshade demo scene — imports hillshade.yaml, slope-angle.yaml, custom-slope-shading.yaml
static NSString * const kSceneHillshadeDemo = @"asset:///scene-hillshade-demo.yaml";

// ---- config.default.yaml values used in this demo ----
// In a full app these would be parsed from the YAML file.
static const BOOL  kConfigMetricUnits = YES;  // config.default.yaml: metric_units
static const float kConfigViewZoom    = 10.f; // config.default.yaml: view.zoom (adjusted for Alps)
static const double kDemoLng = 12.80;
static const double kDemoLat = 47.32;

// Slope mode constants
typedef NS_ENUM(NSInteger, SlopeMode) {
    SlopeModeOff = 0,
    SlopeModeAngle,       // slope-angle.yaml overlay (coloured by slope angle)
    SlopeModeCustom       // custom-slope-shading.yaml overlay (user-defined range)
};

@interface MapViewController () <CLLocationManagerDelegate> {
    SlopeMode  _slopeMode;
    BOOL       _satelliteLayerEnabled;
    BOOL       _contoursEnabled;
}

@property (strong, nonatomic) UIButton *btnSlope;
@property (strong, nonatomic) UIButton *btnLayer;
@property (strong, nonatomic) UIButton *btnContours;
@property (strong, nonatomic) CLLocationManager *locationManager;

- (void)addControlButtons;
- (void)toggleSlope;
- (void)toggleSatelliteLayer;
- (void)toggleContours;
- (NSArray<TGSceneUpdate *> *)buildSceneUpdates;
- (void)updateButtonLabels;
- (NSString *)slopeModeLabel;

@end

@implementation MapViewController

#pragma mark - View Controller

- (void)viewDidLoad
{
    [super viewDidLoad];

    _slopeMode              = SlopeModeOff;
    _satelliteLayerEnabled  = NO;
    _contoursEnabled        = YES;

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

    // Set initial camera — Austrian Alps (Hohe Tauern)
    // zoom value from config.default.yaml: view.zoom
    CLLocationCoordinate2D alps = CLLocationCoordinate2DMake(kDemoLat, kDemoLng);
    TGCameraPosition *camera = [[TGCameraPosition alloc] initWithCenter:alps
                                                                   zoom:kConfigViewZoom
                                                                bearing:0
                                                                  pitch:0];
    [mapView setCameraPosition:camera];

    // Load hillshade demo scene
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

    // Slope toggle — cycles OFF → Slope Angle → Custom Slope → OFF
    self.btnSlope = [UIButton buttonWithType:UIButtonTypeSystem];
    self.btnSlope.translatesAutoresizingMaskIntoConstraints = NO;
    [self.btnSlope setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    self.btnSlope.titleLabel.font = [UIFont systemFontOfSize:12];
    [self.btnSlope addTarget:self action:@selector(toggleSlope)
            forControlEvents:UIControlEventTouchUpInside];
    [controlBar addSubview:self.btnSlope];

    // Satellite layer — adds/removes ArcGIS satellite from mapsources.default.yaml
    self.btnLayer = [UIButton buttonWithType:UIButtonTypeSystem];
    self.btnLayer.translatesAutoresizingMaskIntoConstraints = NO;
    [self.btnLayer setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    self.btnLayer.titleLabel.font = [UIFont systemFontOfSize:12];
    [self.btnLayer addTarget:self action:@selector(toggleSatelliteLayer)
            forControlEvents:UIControlEventTouchUpInside];
    [controlBar addSubview:self.btnLayer];

    // Contours toggle — reads kConfigMetricUnits from config.default.yaml
    self.btnContours = [UIButton buttonWithType:UIButtonTypeSystem];
    self.btnContours.translatesAutoresizingMaskIntoConstraints = NO;
    [self.btnContours setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    self.btnContours.titleLabel.font = [UIFont systemFontOfSize:12];
    [self.btnContours addTarget:self action:@selector(toggleContours)
               forControlEvents:UIControlEventTouchUpInside];
    [controlBar addSubview:self.btnContours];

    [self updateButtonLabels];

    [NSLayoutConstraint activateConstraints:@[
        [controlBar.leadingAnchor  constraintEqualToAnchor:self.view.leadingAnchor],
        [controlBar.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [controlBar.bottomAnchor   constraintEqualToAnchor:self.view.bottomAnchor],
        [controlBar.heightAnchor   constraintEqualToConstant:56],

        [self.btnSlope.leadingAnchor  constraintEqualToAnchor:controlBar.leadingAnchor],
        [self.btnSlope.centerYAnchor  constraintEqualToAnchor:controlBar.centerYAnchor],
        [self.btnSlope.widthAnchor    constraintEqualToAnchor:controlBar.widthAnchor
                                                   multiplier:1.0/3.0],

        [self.btnLayer.centerXAnchor  constraintEqualToAnchor:controlBar.centerXAnchor],
        [self.btnLayer.centerYAnchor  constraintEqualToAnchor:controlBar.centerYAnchor],
        [self.btnLayer.widthAnchor    constraintEqualToAnchor:controlBar.widthAnchor
                                                   multiplier:1.0/3.0],

        [self.btnContours.trailingAnchor constraintEqualToAnchor:controlBar.trailingAnchor],
        [self.btnContours.centerYAnchor  constraintEqualToAnchor:controlBar.centerYAnchor],
        [self.btnContours.widthAnchor    constraintEqualToAnchor:controlBar.widthAnchor
                                                      multiplier:1.0/3.0],
    ]];
}

#pragma mark - Slope Overlay Toggle

/**
 * Cycles slope mode: OFF → Slope Angle → Custom Slope Shading → OFF.
 *
 * Uses -[TGMapView updateGlobals:rebuildTiles:] — no scene reload required.
 * scene-hillshade-demo.yaml binds the shader uniforms to globals:
 *   u_slope_angle_opacity  → global.slope_angle_opacity
 *   u_custom_slope_opacity → global.custom_slope_opacity
 */
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

#pragma mark - Satellite Layer Toggle (from mapsources.default.yaml)

/**
 * Adds or removes the ArcGIS Satellite raster source.
 *
 * The source URL is taken from mapsources.default.yaml (arcgis-satellite entry).
 * Adding a source requires a scene reload with SceneUpdates; removal reloads without them.
 * The satellite-overlay style used by the layer is defined in scene-hillshade-demo.yaml.
 */
- (void)toggleSatelliteLayer
{
    _satelliteLayerEnabled = !_satelliteLayerEnabled;

    TGMapView *mapView = (TGMapView *)self.view;
    [mapView loadSceneAsyncFromURL:[NSURL URLWithString:kSceneHillshadeDemo]
                       withUpdates:[self buildSceneUpdates]];

    [self updateButtonLabels];
    NSLog(@"Satellite layer: %@", _satelliteLayerEnabled ? @"ON" : @"OFF");
}

/**
 * Returns SceneUpdates for the satellite layer.
 * Slope state is handled separately via updateGlobals; only the satellite source definition
 * needs to be passed as SceneUpdates on scene reload.
 */
- (NSArray<TGSceneUpdate *> *)buildSceneUpdates
{
    if (!_satelliteLayerEnabled) {
        return @[];
    }
    // arcgis-satellite source from mapsources.default.yaml
    return @[
        [[TGSceneUpdate alloc] initWithPath:@"sources.arcgis-satellite.type"
                                      value:@"Raster"],
        [[TGSceneUpdate alloc] initWithPath:@"sources.arcgis-satellite.url"
                                      value:@"https://server.arcgisonline.com/arcgis/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"],
        [[TGSceneUpdate alloc] initWithPath:@"sources.arcgis-satellite.max_zoom"
                                      value:@"19"],
        // Layer rendered with the satellite-overlay style defined in scene-hillshade-demo.yaml
        [[TGSceneUpdate alloc] initWithPath:@"layers.arcgis-satellite-layer.data.source"
                                      value:@"arcgis-satellite"],
        [[TGSceneUpdate alloc] initWithPath:@"layers.arcgis-satellite-layer.draw.satellite-overlay.order"
                                      value:@"500"],
    ];
}

#pragma mark - Contour Lines Toggle

/**
 * Toggles contour lines on/off.
 *
 * kConfigMetricUnits is sourced from config.default.yaml (metric_units: true).
 * global.metric_units drives #ifdef METRIC_UNITS in the hillshade shader,
 * selecting 25m/50m/100m intervals (metric) vs. ft-based intervals (imperial).
 */
- (void)toggleContours
{
    _contoursEnabled = !_contoursEnabled;

    TGMapView *mapView = (TGMapView *)self.view;
    [mapView updateGlobals:@[
        [[TGSceneUpdate alloc] initWithPath:@"global.show_contours"
                                      value:_contoursEnabled ? @"true" : @"false"],
        // global.metric_units from config.default.yaml: metric_units
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

    // After a scene reload (e.g. satellite layer toggle), re-apply current slope state.
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

@end
