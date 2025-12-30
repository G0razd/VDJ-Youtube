# VDJ YouTube Plugin

A powerful VirtualDJ plugin that integrates YouTube searching, streaming, and caching capabilities directly into your DJ software.

![C++](https://img.shields.io/badge/c++-%2300599C.svg?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![VirtualDJ](https://img.shields.io/badge/VirtualDJ-blue?style=for-the-badge)
![YouTube](https://img.shields.io/badge/YouTube-%23FF0000.svg?style=for-the-badge&logo=YouTube&logoColor=white)

## Features

*   **Search & Stream**: Search YouTube directly from VirtualDJ and stream audio/video instantly.
*   **High Quality**: Supports up to **1080p** video and high-quality audio (requires `ffmpeg`).
*   **Caching**: Download tracks for offline use ("Download Now" context menu).
*   **History**: Automatically keeps track of your cached songs in a "History" folder.
*   **Playlist Caching**: Download entire playlists for offline access.
*   **Metadata**: Saves artist, title, and duration information.

## Tech Stack

*   **Language**: C++17
*   **SDK**: VirtualDJ Plugin SDK 8
*   **Backend**: [yt-dlp](https://github.com/yt-dlp/yt-dlp) for media extraction
*   **JSON Parsing**: [nlohmann/json](https://github.com/nlohmann/json)
*   **Build System**: CMake

## Installation

1.  Download the latest release from the [Releases](../../releases) page.
2.  Extract the contents of the zip file (`Youtube.dll`, `yt-dlp.exe`, `icon.bmp`).
3.  Copy the files to your VirtualDJ **OnlineSources** plugins folder.

**Common Locations:**

*   **Documents**: `C:\Users\<YourUser>\Documents\VirtualDJ\Plugins64\OnlineSources\`
*   **AppData**: `C:\Users\<YourUser>\AppData\Local\VirtualDJ\Plugins64\OnlineSources\`

*Note: If the `OnlineSources` folder does not exist, create it.*

## Usage

1.  Open VirtualDJ.
2.  Navigate to the **Online Music** folder in the browser.
3.  You should see the **Youtube** source.
4.  **Search**: Type in the search bar to find songs.
5.  **Stream**: Drag a song to a deck to play it immediately.
6.  **Download**: Right-click a song and select **Download Now** to cache it.
7.  **History**: Expand the "History" folder to see your downloaded tracks.

## Support

If you find this plugin useful, consider supporting the development!

[![Ko-fi](https://img.shields.io/badge/Ko--fi-F16061?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/g0razd)

## Author

**g0razd**
