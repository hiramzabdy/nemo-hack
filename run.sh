#!/bin/bash
# Run the dev build of Nemo with local GSettings schema.
# Does NOT replace the system Nemo.
set -e
cd "$(dirname "$0")"
GSETTINGS_SCHEMA_DIR="$(pwd)/build/schemas"
export GSETTINGS_SCHEMA_DIR
exec ./build/src/nemo "$@"
