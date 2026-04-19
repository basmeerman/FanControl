#pragma once

// WiFi station + captive-portal AP + mDNS.
//
// Plan §F5: connect with stored NVS creds; if none (or timeout) fall
// back to an open AP named FanControl-Setup on 192.168.4.1 with a
// DNSServer catch-all so phones recognise it as a captive portal.
// Once STA is up, advertise mDNS hostname "fancontrol" so
// fancontrol.local resolves on the LAN.
//
// The task (run_on_core 0) periodically retries STA if we are in the
// portal, and reconnects if the STA link drops. It is subscribed to
// the hardware TWDT via watchdog::subscribeCurrentTask().

#include <Arduino.h>
#include "config.h"

namespace wifi_manager {

  // Load NVS creds, attempt STA, or fall back to captive-portal AP.
  // Must be called from setup() after storage::begin().
  void begin();

  // FreeRTOS task entry. Handles STA reconnect and AP-to-STA retry.
  void task(void* arg);

  bool   isConnected();
  bool   isPortalActive();   // true when the captive AP is running
  String currentSsid();      // STA SSID, or the AP SSID in portal mode
  int    rssi();             // 0 in AP mode
  String ipAddress();        // STA IP, or "192.168.4.1" in AP mode

} // namespace wifi_manager
