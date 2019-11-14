![TS3D](./logo.gif)

## Overview

This is a work-in-progress first-person shooter. It uses terminal-based 3D
graphics.

## Building

Run make in the project root. This does not account for updates to dependencies.
To update those, see DEPENDENCIES.

To build the tests, run `make tests`. A binary `tests` will be produced. This
can then be used with the `ceeteef` command.

## Running

Currently, to run a demo sort of thing, execute either ./ts3d columns or
./ts3d title-screen after running Make. To exit, press 'x'.

To run the tests and print the colored output to a file, do `make run-tests`.
The file is `tests.log`. It is best viewed with `cat` or `less -R`.

## Data

The data/ directory holds game data. The data is stored in a combination of JSON
and simpler custom formats. It is organized into sub-directories.

## Dependencies

The external/ folder holds dependencies to other projects, with source code and
such bundled here. There are currently three dependencies, all of which I myself
created. d3d/ is the 3D graphics library. json/ is the JSON parser.
c-test-functions/ (or ctf) is the testing framework.

The ctf API is included, but not the executable. To run the tests, you will need
to install that in your PATH yourself.

To update the depencies from Github, run ./update-all from external/.
