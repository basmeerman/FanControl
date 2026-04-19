#pragma once

// AsyncWebServer wrapper — single-page UI on port 80 + ElegantOTA at
// /update + captive-portal magic URLs + WebSocket mount point.
//
// See webserver.cpp for route-level docs.

#include <Arduino.h>

class AsyncWebServer;

namespace webserver {

  // Create the AsyncWebServer on port 80, wire all routes, mount the
  // WebSocket, and mount ElegantOTA with the currently stored password.
  // Call after storage::begin() and wifi_manager::begin().
  void begin();

  // Accessor so websocket.cpp can attach its AsyncWebSocket instance.
  AsyncWebServer& instance();

  // Re-mount ElegantOTA with a new admin password. Called after the
  // user saves a new OTA password via the web UI.
  void applyOtaPassword(const String& newPassword);

} // namespace webserver
