# Description

When you press the pause key, draw lines towards the mouse cursor.

# Build

$ sudo yum install cairo-devel libX11-devel
$ make

# Run

The process will run in the forground by default. Pass it "-d" to run in the background.

The provided terminate.sh script can be used to shut the process down if daemonized.

# Todo

The hotkey should be configurable.
