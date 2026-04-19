#include "wifi_manager.h"

#include "config.h"
#include "storage.h"
#include "watchdog.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>

// See wifi_manager.h for the public contract.
//
// Decisions:
//   - Captive-portal SSID is hardcoded "FanControl-Setup" (plan v1.2).
//     No password; the portal is deliberately short-lived and users are
//     guided to set creds within minutes.
//   - STA connect timeout is 30 s in begin(). The task retries every
//     WIFI_RETRY_INTERVAL_MS if we end up in AP mode.
//   - A DNSServer on UDP/53 answers every query with 192.168.4.1 — this
//     is the captive-portal hook that makes phones open the setup page.

namespace {

constexpr char     PORTAL_SSID[]    = "FanControl-Setup";
constexpr uint16_t DNS_PORT         = 53;
constexpr uint32_t STA_CONNECT_TIMEOUT_MS = 30000;
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_NETMASK(255, 255, 255, 0);

DNSServer   s_dns;
bool        s_portalActive = false;
bool        s_dnsActive    = false;
bool        s_mdnsActive   = false;
String      s_currentSsid;  // filled on STA connect or AP start

// Attempt a single STA connect. Returns true on success within timeout.
bool connectSta(const WifiConfig& cfg) {
  if (cfg.ssid.isEmpty()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEFAULT_MDNS_HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);  // we own NVS via storage::*, don't let WiFi lib write
  WiFi.disconnect(true, true);

  log_i("WiFi STA: connecting to '%s'", cfg.ssid.c_str());
  WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());

  const uint32_t t0 = millis();
  while ((millis() - t0) < STA_CONNECT_TIMEOUT_MS) {
    if (WiFi.status() == WL_CONNECTED) {
      s_currentSsid = cfg.ssid;
      log_i("WiFi STA connected: ip=%s rssi=%d",
            WiFi.localIP().toString().c_str(), WiFi.RSSI());
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }

  log_w("WiFi STA timeout for '%s' (rc=%d)", cfg.ssid.c_str(), (int)WiFi.status());
  return false;
}

void startMdns() {
  if (s_mdnsActive) return;
  if (MDNS.begin(DEFAULT_MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    s_mdnsActive = true;
    log_i("mDNS: %s.local", DEFAULT_MDNS_HOSTNAME);
  } else {
    log_w("mDNS begin failed");
  }
}

void stopMdns() {
  if (!s_mdnsActive) return;
  MDNS.end();
  s_mdnsActive = false;
}

// Start the open captive-portal AP and the DNS catch-all.
void startPortal() {
  stopMdns();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_NETMASK);
  const bool ok = WiFi.softAP(PORTAL_SSID);  // no password per plan v1.2
  if (!ok) {
    log_e("Portal AP start failed");
    return;
  }
  s_currentSsid = PORTAL_SSID;

  // Catch-all DNS: answer every query with our own IP → phones flag this
  // as a captive portal and pop up the browser.
  s_dns.setErrorReplyCode(DNSReplyCode::NoError);
  if (s_dns.start(DNS_PORT, "*", AP_IP)) {
    s_dnsActive = true;
  } else {
    log_w("DNSServer start failed — captive-portal detection may not trigger");
  }

  s_portalActive = true;
  log_w("Captive portal active: ssid='%s' ip=%s",
        PORTAL_SSID, AP_IP.toString().c_str());
}

void stopPortal() {
  if (!s_portalActive) return;
  if (s_dnsActive) {
    s_dns.stop();
    s_dnsActive = false;
  }
  WiFi.softAPdisconnect(true);
  s_portalActive = false;
  log_i("Captive portal stopped");
}

} // namespace

namespace wifi_manager {

void begin() {
  const WifiConfig cfg = storage::loadWifi();

  if (!cfg.ssid.isEmpty() && connectSta(cfg)) {
    startMdns();
    return;
  }
  log_w("No valid WiFi creds or STA failed — starting captive portal");
  startPortal();
}

void task(void* /*arg*/) {
  watchdog::subscribeCurrentTask();
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    watchdog::reset();

    if (s_portalActive) {
      // Service captive-portal DNS every tick so requests don't queue up.
      if (s_dnsActive) s_dns.processNextRequest();

      // Try to escape the portal periodically if the user saved creds.
      static uint32_t s_lastRetryMs = 0;
      const uint32_t now = millis();
      if ((now - s_lastRetryMs) >= WIFI_RETRY_INTERVAL_MS) {
        s_lastRetryMs = now;
        const WifiConfig cfg = storage::loadWifi();
        if (!cfg.ssid.isEmpty()) {
          log_i("Portal: retrying STA with stored creds");
          stopPortal();
          if (connectSta(cfg)) {
            startMdns();
          } else {
            startPortal();  // fall back again
          }
        }
      }
      // DNS needs frequent ticks.
      vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(50));
      continue;
    }

    // STA path: watch for link loss.
    if (WiFi.status() != WL_CONNECTED) {
      log_w("WiFi STA disconnected — reconnecting");
      stopMdns();
      const WifiConfig cfg = storage::loadWifi();
      if (!connectSta(cfg)) {
        log_w("STA reconnect failed — falling back to captive portal");
        startPortal();
      } else {
        startMdns();
      }
    }

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(WIFI_RETRY_INTERVAL_MS));
  }
}

bool isConnected() {
  return !s_portalActive && WiFi.status() == WL_CONNECTED;
}

bool isPortalActive() {
  return s_portalActive;
}

String currentSsid() {
  if (s_portalActive) return PORTAL_SSID;
  if (WiFi.status() == WL_CONNECTED) return WiFi.SSID();
  return s_currentSsid;
}

int rssi() {
  if (s_portalActive) return 0;
  return WiFi.RSSI();
}

String ipAddress() {
  if (s_portalActive) return AP_IP.toString();
  return WiFi.localIP().toString();
}

} // namespace wifi_manager
