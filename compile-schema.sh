#!/bin/bash
# Compile GSettings schema for local testing
# Run after `ninja` to pick up schema changes

SCHEMA_DIR="$(dirname "$0")/build/schemas"
SRC_DIR="$(dirname "$0")/src/libnemo-private"

mkdir -p "$SCHEMA_DIR"
cp "$SRC_DIR/org.nemo.gschema.xml" "$SCHEMA_DIR/"
glib-compile-schemas "$SCHEMA_DIR"
echo "Schema compiled to $SCHEMA_DIR"
echo "Run: GSETTINGS_SCHEMA_DIR=$SCHEMA_DIR ./build/src/nemo ~"
