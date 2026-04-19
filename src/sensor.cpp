#include "sensor.h"
#include <DHT.h>
#include <math.h>

namespace {

// DHT22 = DHT_TYPE 22 in the Adafruit library.
DHT     dht(PIN_DHT22, DHT22);

float    s_lastTempC        = NAN;
float    s_lastHumidityPct  = NAN;
uint32_t s_lastReadOkMs     = 0;
bool     s_everReadOk       = false;

} // namespace

namespace sensor {

void begin() {
  dht.begin();
  log_i("DHT22 begin on GPIO %u", PIN_DHT22);
}

bool read() {
  const float t = dht.readTemperature();
  const float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    log_w("DHT22 read failed (NaN) — keeping last-good values");
    return false;
  }

  s_lastTempC       = t;
  s_lastHumidityPct = h;
  s_lastReadOkMs    = millis();
  s_everReadOk      = true;
  log_i("DHT22 ok: %.1f C  %.1f %%", t, h);
  return true;
}

float getTemperatureC() {
  return s_lastTempC;
}

float getHumidityPct() {
  return s_lastHumidityPct;
}

uint32_t lastReadAgeMs() {
  if (!s_everReadOk) return UINT32_MAX;
  return millis() - s_lastReadOkMs;
}

bool isStale() {
  return lastReadAgeMs() > SENSOR_STALL_TIMEOUT_MS;
}

} // namespace sensor
