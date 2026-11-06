# Spot

Shazam song recognition and Spotify playlist synchronization for the Omarchy desktop.

Spot listens to your microphone or desktop audio monitor, identifies playing songs using Shazam, and seamlessly connects to Spotify to open tracks or add them directly to your playlists with a single click.

## Features

- **Instant Recognition**: Press Space to sample audio from your mic or desktop sound output.
- **Device Selection**: Switch between microphones, headsets, or desktop monitor loopbacks on the fly.
- **Spotify Integration**: Open in Spotify, copy links, view album artwork, and check if tracks are in your Liked Songs.
- **Playlist Sync**: Spotify-style green checkmark when saved; click to toggle adding/removing from any of your playlists.
- **Omarchy Native**: Follows your Omarchy theme accent color and Hyprland window rounding live.
- **Background Tray Mode**: Minimize Spot to the system tray with continuous listening; notifies you whenever a new song is detected.
- **Multilingual**: Follows Omarchy system language live.
- **History**: Keeps track of your recently discovered songs.

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Space` | Identify song / Stop listening |
| `Ctrl+B` | Minimize to system tray and run in background |
| `?` | Toggle shortcuts cheat sheet |
| `Esc` | Close open dialogs or cancel listening |
| `Q` | Quit Spot |

## Building and Testing

```bash
bin/build    # Compiles with qmake6 and make
bin/test     # Runs unit tests offscreen
bin/install  # Installs package and registers in app launcher
```

## License

MIT License. Copyright (c) 2026 Gustavo.
