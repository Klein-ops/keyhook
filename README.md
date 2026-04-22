# keyhook

A lightweight Linux input event trigger that executes scripts based on key press patterns.

## Features

- Multi-click mode — Trigger after N presses within a configurable time window.
- Long-press mode — Trigger while holding a key, no release required.
- Zero CPU usage — Blocks on input events, consumes no CPU when idle.
- Self-contained — Single static binary with no external dependencies.

## Usage

keyhook -d <device> -k <keycode> -s <script> [options]

### Required Arguments

-d    Input device path (e.g., /dev/input/event1)
-k    Keycode to monitor (decimal)
-s    Script path to execute when triggered

### Options

-m    Trigger mode: multi or long (default: multi)
-c    Press count for multi-click mode (default: 3)
-t    Timeout in ms for multi-click mode (default: 1000)
-l    Duration in ms for long-press mode (default: 3000)
-e    Exit after first trigger
-p    Print PID to stdout on startup
-v    Verbose debug output to stderr
-h    Show help message

### Examples

# Trigger on 3 volume-up presses within 1 second
keyhook -d /dev/input/event1 -k 115 -s /path/to/script.sh

# Trigger after holding volume-down for 5 seconds
keyhook -d /dev/input/event1 -k 114 -m long -l 5000 -s /path/to/script.sh

# Trigger once and exit, with debug output
keyhook -d /dev/input/event1 -k 115 -s /path/to/script.sh -e -v

# Run in background and capture PID
keyhook -d /dev/input/event1 -k 115 -s /path/to/script.sh -p &
PID=$!

## Common Keycodes

114    KEY_VOLUMEDOWN
115    KEY_VOLUMEUP
116    KEY_POWER
102    KEY_HOME
158    KEY_BACK

## License

GPL-2.0
