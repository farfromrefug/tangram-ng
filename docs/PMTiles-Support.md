# PMTiles Support in Tangram-NG

## Overview

Tangram-NG now supports [PMTiles](https://github.com/protomaps/PMTiles), a cloud-optimized single-file archive format for tile pyramids.

## Usage

### Scene YAML Configuration

Local file:
```yaml
sources:
  my-tiles:
    type: MVT
    url: file:///path/to/tiles.pmtiles
```

HTTP source (fully supported):
```yaml
sources:
  remote-tiles:
    type: MVT
    url: https://example.com/tiles.pmtiles
```

## Features

- Local file support ✓
- HTTP support with range requests ✓
- Gzip decompression ✓
- Thread-safe async I/O ✓
- Root directory caching ✓
- Integrates with existing source chain ✓

## Build

Enabled by default. To disable:
```bash
cmake -DTANGRAM_PMTILES_DATASOURCE=OFF ..
```
