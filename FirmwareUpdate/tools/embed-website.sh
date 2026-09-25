#!/bin/sh
# Build the web UI and bake it into the firmware image.
#
#   tools/embed-website.sh
#
# Run this after changing anything under website/. It regenerates
# ILT_OS/Applications/ILT_TESTRIG/WebsiteAsset.c, which the firmware build
# compiles into the dedicated .website flash region; the next `cmake --build`
# picks it up.
set -e

here=$(cd "$(dirname "$0")/.." && pwd)
cd "$here/website"

echo "building the web UI..."
npm run build >/dev/null

cd "$here"
python3 tools/embed_website.py website/dist \
    --output ILT_OS/Applications/ILT_TESTRIG/WebsiteAsset.c

echo "done -- rebuild the firmware to pick it up"
