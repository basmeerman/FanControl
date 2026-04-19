#pragma once

// CI release.yml rewrites the VERSION line via sed from the git tag.
// Keep the exact format: #define VERSION "x.y.z"
#define VERSION "0.1.0-dev"

#define BUILD_DATE __DATE__ " " __TIME__
