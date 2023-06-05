# Description

When you press the pause key, draw lines towards the mouse cursor.

# Build

Install the dependancies

$ sudo yum install cairo-devel libX11-devel

And then use make

$ make

# Run

The process will run in the forground by default. Pass it "-d" to run in the background. The provided terminate.sh script can be used to shut the process down.
