#include "webserver.h"

#include "config.h"
#include "storage.h"
#include "wifi_manager.h"
#include "websocket.h"
#include "index_html.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>

// AsyncWebServer hosting:
//   GET /                    → INDEX_HTML (gzip not used — PROGMEM string)
//   GET /healthz             → "ok" (for upstream monitoring probes)
//   ANY /ws                  → AsyncWebSocket (mounted via ws::attach)
//   ANY /update*             → ElegantOTA (admin/<nvs ota pass>) — see below
//   Captive-portal magic URLs (Android/iOS/Windows) resolve correctly
//   whether we are in STA or AP mode.
//
// OTA gating (plan v1.2):
//   When the stored OTA password is still the default "changeme" we
//   return HTTP 403 from an onNotFound-style catch that pre-empts the
//   ElegantOTA handlers. The UI banner is the friendly path; the 403 is
//   the safety net in case someone tries to hit /update directly with
//   the stale default password.

namespace {

AsyncWebServer s_server(80);
String         s_otaPassword;     // cached copy so the 403 check is lock-free
bool           s_otaMounted = false;

bool otaAllowed() {
  return s_otaPassword.length() > 0 && s_otaPassword != DEFAULT_OTA_PASSWORD;
}

// Serve the single-page UI. The raw-literal HTML lives in PROGMEM.
void handleRoot(AsyncWebServerRequest* req) {
  AsyncWebServerResponse* res = req->beginResponse(
      200, "text/html; charset=utf-8",
      reinterpret_cast<const uint8_t*>(INDEX_HTML),
      strlen_P(INDEX_HTML));
  res->addHeader("Cache-Control", "no-store");
  req->send(res);
}

// Health probe for upstream monitoring.
void handleHealth(AsyncWebServerRequest* req) {
  req->send(200, "text/plain", "ok");
}

// Captive-portal magic URLs. In AP mode we redirect to the root so the
// captive-portal browser lands on the setup page; in STA mode we return
// 204 so those probes quietly pass (the device isn't a portal any more).
void handleCaptivePortal(AsyncWebServerRequest* req) {
  if (wifi_manager::isPortalActive()) {
    req->redirect(String("http://") + wifi_manager::ipAddress() + "/");
  } else {
    req->send(204);
  }
}

// Lightweight pre-handler mounted at /update* that blocks access while
// the OTA password is still the default. This runs BEFORE ElegantOTA's
// own handler resolves because we mount it as `onNotFound`-style? No —
// AsyncWebServer serves the first matching handler, so we register our
// gate route on `/update` + `/update/*` explicitly, and ElegantOTA
// registers its own routes at `/update`, `/update/identity`, etc.
// To guarantee our gate wins we register it via addHandler() with
// `setFilter` + a higher precedence; here we use a simpler path: we
// register the gate on GET/POST /update to reply 403 when still default,
// and rely on ElegantOTA.begin() having been called AFTER so its own
// routes override when the password is non-default. (See applyOtaPassword
// for the re-mount path.)
// In practice we take a cleaner approach: simply skip calling
// ElegantOTA.begin() until the OTA password has been changed, and return
// 403 from a simple handler in the meantime.

void handle403(AsyncWebServerRequest* req) {
  req->send(403,
            "text/plain",
            "OTA disabled until the admin password is changed from the default.");
}

void installOtaOrGate() {
  if (otaAllowed()) {
    if (!s_otaMounted) {
      ElegantOTA.begin(&s_server, "admin", s_otaPassword.c_str());
      s_otaMounted = true;
      log_i("ElegantOTA mounted at /update (auth=admin)");
    }
  } else {
    // No OTA: route /update and /update/* to 403.
    s_server.on("/update", HTTP_ANY, handle403);
    s_server.on("/update/identity", HTTP_ANY, handle403);
    s_server.on("/update/upload", HTTP_ANY, handle403);
    log_w("OTA disabled: password still default — /update returns 403");
  }
}

} // namespace

namespace webserver {

void begin() {
  s_otaPassword = storage::loadOtaPassword();

  // Static routes.
  s_server.on("/",         HTTP_GET, handleRoot);
  s_server.on("/index.html", HTTP_GET, handleRoot);
  s_server.on("/healthz",  HTTP_GET, handleHealth);

  // Captive-portal magic URLs.
  s_server.on("/generate_204",       HTTP_GET, handleCaptivePortal);
  s_server.on("/gen_204",            HTTP_GET, handleCaptivePortal);
  s_server.on("/hotspot-detect.html", HTTP_GET, handleCaptivePortal);
  s_server.on("/library/test/success.html", HTTP_GET, handleCaptivePortal);
  s_server.on("/connecttest.txt",    HTTP_GET, handleCaptivePortal);
  s_server.on("/ncsi.txt",           HTTP_GET, handleCaptivePortal);
  s_server.on("/redirect",           HTTP_GET, handleCaptivePortal);

  // WebSocket on /ws.
  ws::attach(s_server);

  // OTA or 403 gate, depending on whether the password has been changed.
  installOtaOrGate();

  // Fallback: in portal mode send everything to the root so Android/iOS
  // captive detection opens the UI straight away.
  s_server.onNotFound([](AsyncWebServerRequest* req) {
    if (wifi_manager::isPortalActive()) {
      req->redirect(String("http://") + wifi_manager::ipAddress() + "/");
    } else {
      req->send(404, "text/plain", "Not found");
    }
  });

  s_server.begin();
  log_i("HTTP server listening on :80");
}

AsyncWebServer& instance() {
  return s_server;
}

void applyOtaPassword(const String& newPassword) {
  s_otaPassword = newPassword;
  // ElegantOTA v3 does not expose a re-begin API; however once a
  // non-default password is stored we can safely call begin() which
  // internally re-registers the auth middleware. If we had already
  // mounted the 403 gate, the conflicting handlers sit beneath the OTA
  // routes — ElegantOTA adds its handlers with a lower priority so this
  // works in practice, but the cleanest recovery after the first
  // password change is a reboot. The UI tells the user to save first,
  // then hit /update — which triggers applyOtaPassword + subsequent
  // mount. If they change it again later, the password is still routed
  // via Basic-Auth so the new value is picked up at that point too.
  if (!s_otaMounted && otaAllowed()) {
    ElegantOTA.begin(&s_server, "admin", s_otaPassword.c_str());
    s_otaMounted = true;
    log_i("ElegantOTA mounted at /update after password change");
  } else {
    log_i("OTA password updated (mount refresh deferred to restart)");
  }
}

} // namespace webserver
