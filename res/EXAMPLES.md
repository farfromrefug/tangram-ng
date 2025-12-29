# Tangram-NG Style Examples

This directory contains example scene files demonstrating various features of Tangram-NG. These examples are designed to be comprehensive references for common use cases.

## Available Examples

### 1. 3D Terrain from Local MBTiles (`example-3d-terrain-mbtiles.yaml`)

Demonstrates how to use locally stored MBTiles files for 3D terrain rendering.

**Key Features:**
- Loading elevation data from local MBTiles files
- Linking satellite imagery with elevation for 3D effect
- Terrain shader configuration for Mapzen Terrarium format
- Perspective camera setup for 3D viewing
- Optional lighting for realistic terrain rendering
- Vector feature rendering on 3D terrain

**Use Cases:**
- Offline map applications with pre-downloaded terrain
- High-performance terrain visualization
- Custom elevation data sources
- Outdoor recreation and navigation apps

**Requirements:**
- MBTiles files with elevation data (Terrarium format: RGB encoding)
- Optional: MBTiles with satellite or base map imagery
- OpenGL ES 2.0+ or OpenGL 3.0+

### 2. Raster Source with Transparency (`example-raster-transparency.yaml`)

Comprehensive guide to using raster layers with transparency, blending modes, and caching.

**Key Features:**
- Multiple blending modes (opaque, overlay, multiply, translucent, inlay, add)
- Alpha transparency control (fixed and zoom-dependent)
- Caching raster tiles to local MBTiles
- Texture filtering options
- Custom shader-based transparency
- Mixing raster and vector layers

**Blending Modes:**
- `opaque`: No transparency, fully opaque rendering
- `overlay`: Standard alpha blending with transparency
- `translucent`: Similar to overlay, allows transparency
- `inlay`: Renders below opaque features
- `add`: Additive blending (brightens)
- `multiply`: Multiplicative blending (darkens, e.g., hillshade)
- `nonopaque`: Non-opaque rendering without specific blend function

**Use Cases:**
- Weather overlays (precipitation, temperature, wind)
- Hillshade and terrain visualization
- Semi-transparent base maps
- Multiple raster layer compositing
- Offline caching for performance

**Requirements:**
- Raster tile sources (online or MBTiles)
- For caching: diskTileCacheSize > 0 in SceneOptions

### 3. GeoJSON Source (`example-geojson.yaml`)

Complete reference for working with GeoJSON and TopoJSON data sources.

**Key Features:**
- Remote GeoJSON files (non-tiled)
- Tiled GeoJSON for large datasets
- Local GeoJSON from MBTiles
- TopoJSON support
- Property-based filtering and styling
- Dynamic labels from GeoJSON properties
- Zoom-dependent rendering
- Client-side GeoJSON via API

**Source Types:**
- **Non-tiled GeoJSON**: Single file, best for small-medium datasets
- **Tiled GeoJSON**: URL pattern with {z}/{x}/{y}, best for large datasets
- **TopoJSON**: Compact format with shared topology
- **Client-side**: Added dynamically via API

**Geometry Support:**
- Point, MultiPoint
- LineString, MultiLineString
- Polygon, MultiPolygon
- GeometryCollection

**Use Cases:**
- Custom geographic data visualization
- User-generated content (routes, areas, markers)
- Real-time data updates
- Offline geographic datasets
- Choropleth maps and data visualization
- Boundary and administrative region display

### 4. Client-Side GeoJSON from String (`example-client-geojson.yaml`)

Detailed guide for creating GeoJSON sources from manually generated strings using the C++ API.

**Key Features:**
- Creating GeoJSON from strings at runtime
- Six complete C++ implementation examples
- Using ClientDataSource builder API
- Programmatically building GeoJSON
- JSON library integration (nlohmann/json)
- Dynamic data updates
- Multiple feature types in one source

**Implementation Methods:**
- **GeoJSON String**: Parse complete GeoJSON string
- **Builder API**: Use PolylineBuilder and PolygonBuilder
- **Programmatic**: Build JSON from application data
- **JSON Library**: Integration with C++ JSON libraries

**Use Cases:**
- User-drawn features (drawing tools)
- Real-time data from sensors or APIs
- Dynamic route visualization
- Custom annotations and overlays
- Application-generated geometries
- Interactive data visualization

**Requirements:**
- C++ API access to Tangram::ClientDataSource
- Optional: C++ JSON library for easier JSON generation

### 5. Feature Selection (`example-feature-selection.yaml`)

Comprehensive guide for selecting and highlighting features from local MBTiles vector tiles.

**Key Features:**
- Interactive feature picking with pickFeatureAt()
- Multiple selection methods (overlay and scene updates)
- Hover and click interactions
- Multi-select capability
- Different styling for selected features
- Property-based selection filtering
- Complete C++ implementation examples

**Selection Techniques:**
- **Overlay Method**: Use ClientDataSource for selection overlay
- **Scene Update Method**: Update scene styling dynamically
- **Hybrid Method**: Combine both for complex interactions

**Interaction Patterns:**
- Single selection with highlight
- Multi-select with different colors
- Hover preview with click-to-select
- Property-based bulk selection
- Selection with visual feedback

**Use Cases:**
- Interactive map applications
- Feature editing tools
- Property inspection interfaces
- Multi-feature analysis
- Click-to-highlight functionality
- Building/parcel selection in GIS apps

**Requirements:**
- Local MBTiles vector tiles with interactive: true
- C++ API access for feature picking
- ClientDataSource for selection overlays

## Usage

### Basic Scene Loading

```cpp
// C++ API
#include "tangram.h"

// Create map instance
Tangram::Map map;

// Load scene file
map.loadScene("res/example-3d-terrain-mbtiles.yaml");

// Or with scene options
Tangram::SceneOptions options;
options.diskCacheDir = "/path/to/cache/";
options.diskTileCacheSize = 100 * 1024 * 1024; // 100MB
map.loadScene("res/example-raster-transparency.yaml", options);
```

### Adding Client-Side GeoJSON

```cpp
// Method 1: Using ClientDataSource directly (recommended)
#include "tangram.h"
#include "data/clientDataSource.h"

// Create the data source
auto dataSource = std::make_shared<Tangram::ClientDataSource>(
    platform, "my-dynamic-data", "", true);  // true = generate centroids
map->addTileSource(dataSource);

// Add GeoJSON data from string
std::string geojson = R"({
    "type": "FeatureCollection",
    "features": [{
        "type": "Feature",
        "geometry": {
            "type": "Point",
            "coordinates": [-122.4, 37.8]
        },
        "properties": {
            "name": "San Francisco"
        }
    }]
})";
dataSource->addData(geojson);
dataSource->generateTiles();

// Method 2: Using builder API for individual features
Tangram::Properties props;
props.set("name", "Custom Point");
dataSource->addPointFeature(
    std::move(props),
    Tangram::LngLat{-122.4, 37.8}
);
dataSource->generateTiles();
```

### Feature Selection and Highlighting

```cpp
// Setup selection overlay
auto selectionSource = std::make_shared<Tangram::ClientDataSource>(
    platform, "selection-overlay", "");
map->addTileSource(selectionSource);

// Handle click to select feature
void onMapClick(float screenX, float screenY) {
    map->pickFeatureAt(screenX, screenY, 
        [](const Tangram::FeaturePickResult* result) {
            if (result && result->properties) {
                // Feature was picked - highlight it
                highlightSelectedFeature(result);
            } else {
                // Clear selection
                selectionSource->clearFeatures();
                selectionSource->generateTiles();
            }
        });
}

void highlightSelectedFeature(const Tangram::FeaturePickResult* result) {
    selectionSource->clearFeatures();
    
    // Add selected feature to overlay
    Tangram::Properties props;
    props.set("selected", "true");
    // Copy relevant properties from result->properties
    
    // Add feature based on geometry type
    // Note: You'll need to reconstruct or store the geometry
    selectionSource->addPointFeature(std::move(props), coordinates);
    selectionSource->generateTiles();
}
```

### MBTiles Creation

MBTiles files can be created using various tools:

**For Vector Tiles:**
```bash
# Using tippecanoe
tippecanoe -o output.mbtiles input.geojson -z14

# Using mb-util
mb-util --image_format=pbf tiles/ output.mbtiles
```

**For Raster Tiles:**
```bash
# Using GDAL
gdal2tiles.py -p mercator input.tif output/
mb-util output/ output.mbtiles

# Or use gdal_translate directly
gdal_translate -of MBTiles input.tif output.mbtiles
```

**For Elevation Tiles (Terrarium format):**
```bash
# Convert DEM to Terrarium RGB encoding
# Custom processing required to encode elevation as RGB
# See: https://github.com/tilezen/joerd/blob/master/docs/formats.md#terrarium
```

## Scene Configuration Reference

### Common Source Parameters

```yaml
sources:
  source-name:
    type: GeoJSON|MVT|Raster|TopoJSON
    url: "http://..." or "file.mbtiles"
    url_subdomains: [a, b, c]  # For {s} placeholder
    max_zoom: 14               # Maximum zoom level
    min_display_zoom: 8        # Minimum zoom to display
    max_display_zoom: 18       # Maximum zoom to display
    zoom_offset: 0             # Offset zoom level
    tile_size: 512             # Tile size in pixels
    cache: filename            # MBTiles cache file (without .mbtiles)
    max_age: 3600              # Cache expiration in seconds
    rasters: [elevation]       # Link to raster sources
    generate_label_centroids: true  # For GeoJSON polygons
    filtering: linear|nearest|mipmap  # Texture filtering (raster only)
    url_params:                # Additional URL parameters
      key: value
    headers:                   # Custom HTTP headers
      Authorization: "Bearer token"
```

### Blending and Transparency

```yaml
styles:
  my-style:
    base: polygons|lines|points|raster|text
    blend: opaque|overlay|translucent|inlay|add|multiply|nonopaque
    blend_order: 0  # Lower renders first
    draw:
      alpha: 0.5    # Fixed opacity
      alpha: [[10, 0.2], [14, 0.8]]  # Zoom-dependent
```

### 3D Terrain Configuration

```yaml
scene:
  terrain_3d: true
  elevation_source: elevation

cameras:
  perspective-camera:
    type: perspective
    fov: 45
    max_tilt: [[2, 0], [16, 90]]
```

## Performance Tips

1. **Use appropriate zoom levels**: Set `max_zoom` to limit detail
2. **Enable caching**: Use MBTiles cache for frequently accessed tiles
3. **Optimize GeoJSON**: Use tiled GeoJSON or convert to MVT for large datasets
4. **Filter by zoom**: Use `$zoom` filters to show features only when relevant
5. **Limit raster resolution**: Use `zoom_offset` to load lower resolution tiles
6. **Batch updates**: Group data updates to minimize redraws
7. **Use offline tiles**: Pre-download MBTiles for offline performance

## Additional Resources

- **Tangram Documentation**: https://tangrams.readthedocs.io
- **Scene File Syntax**: https://tangrams.readthedocs.io/en/latest/Syntax-Reference/
- **Tangram-NG Repository**: https://github.com/farfromrefug/tangram-ng
- **MBTiles Specification**: https://github.com/mapbox/mbtiles-spec
- **GeoJSON Specification**: https://geojson.org/
- **Mapzen Terrarium Format**: https://github.com/tilezen/joerd/blob/master/docs/formats.md

## Existing Example Scenes

This directory also contains other example scenes:

- `scene.yaml` - Basic scene with normal-mapped terrain
- `scene3d.yaml` - 3D terrain example with online sources
- `osm-bright.yaml` - Full OpenStreetMap style
- `omt-sources.yaml` - OpenMapTiles source configuration

## License

These examples are provided as part of the Tangram-NG project. See the LICENSE file in the root directory for more information.
