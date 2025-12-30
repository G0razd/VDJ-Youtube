# Changelog

All notable changes to this project will be documented in this file.

## [1.2] - 2025-12-30
### Fixed
- Disabled "Download Now" context menu temporarily due to stability issues.
- Fixed GitHub Actions release workflow permissions (403 error).

## [1.1] - 2025-12-30
### Fixed
- Fixed `yt-dlp` path detection: now falls back to system `yt-dlp` if the bundled executable is missing.
- Fixed 1080p playback issues by forcing MP4 container merging.

## [1.0] - 2025-12-30
### Added
- Initial release.
- Search and Stream from YouTube.
- 1080p video support (requires ffmpeg).
- Caching system ("Download Now").
- History folder for cached tracks.
- Playlist caching support.
- Embedded icon.
