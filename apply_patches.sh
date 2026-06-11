#!/bin/bash
# Script to apply patches to managed components for ESP-IDF 6.x compatibility

# Ensure we are in the project root
cd "$(dirname "$0")"

# Download components if managed_components is missing or empty
# This uses the idf_component_manager directly to avoid the project() requirement resolution failure
if [ ! -d "managed_components" ] || [ -z "$(ls -A managed_components)" ]; then
    echo "managed_components missing or empty, downloading..."
    # We use a python command that downloads components based on idf_component.yml
    # This is what idf.py build does internally before CMake starts.
    python3 -m idf_component_manager.core project managed-install || true

    # Fallback to idf.py reconfigure if the above fails
    if [ ! -d "managed_components" ]; then
        idf.py reconfigure || true
    fi
fi

# Apply patches
if [ -d "patches" ]; then
    for p in patches/*.patch; do
        if [ -f "$p" ]; then
            echo "Checking patch $p..."
            # Check if patch is already applied
            if patch -p1 -R --dry-run < "$p" > /dev/null 2>&1; then
                echo "Patch $p already applied."
            else
                echo "Applying patch $p..."
                patch -p1 < "$p"
            fi
        fi
    done
else
    echo "No patches directory found."
fi
