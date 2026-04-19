#pragma once

// AsyncWebSocket (/ws) — 2-second status push + inbound config frames.
//
// See websocket.cpp for JSON shapes and dispatch table.

#include <Arduino.h>

class AsyncWebServer;

namespace ws {

  // Attach the AsyncWebSocket to the given server. Must be called before
  // webserver::instance().begin().
  void attach(AsyncWebServer& server);

  // FreeRTOS task entry. Pushes {"type":"status", ...} every
  // WS_PUSH_INTERVAL_MS and runs AsyncWebSocket housekeeping
  // (cleanupClients) once per second.
  void task(void* arg);

  // Optional — broadcast a single log line as {"type":"log","line":"..."}.
  // Safe to call from any task; the push itself happens on the caller.
  void broadcastLog(const char* line);

} // namespace ws
