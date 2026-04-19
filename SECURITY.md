# Security Policy

## Threat model

FanControl runs on a LAN by design — a home battery room ventilation controller
that talks to Home Assistant and (optionally) an MQTT broker on the local
network. The security posture is scoped accordingly:

- **In scope:** protecting OTA updates from casual tampering, preventing
  unauthenticated HA control, keeping WiFi credentials off the wire.
- **Out of scope:** defence against attackers already on the LAN mounting
  active man-in-the-middle attacks, supply-chain attacks on ESPHome itself,
  physical access to the device.

If your deployment is internet-exposed (remote WireGuard client excepted),
stop and reconsider — this firmware is not designed for that.

## OTA updates

ESPHome OTA is password-gated (see `ota.password` in `fancontrol.yaml`, set via
`secrets.yaml`). The OTA handshake hashes the binary with SHA-256 and rejects
mismatches. **There is no RSA signature on v0.2.0+ firmware** — the OTA
password plus the checksum are the only authenticity controls.

Verify a downloaded release artifact with its published SHA-256:

```bash
sha256sum -c FanControl-vX.Y.Z.bin.sha256
# → FanControl-vX.Y.Z.bin: OK
```

Use `firmware.factory.bin` for initial USB flashing (includes bootloader +
partition table), `firmware.ota.bin` for over-the-air updates.

## Home Assistant API

The ESPHome native API is encrypted (`api.encryption.key` in `fancontrol.yaml`).
The key is 32 bytes, base64-encoded, stored in `secrets.yaml`. Rotate by
regenerating with `openssl rand -base64 32`, updating `secrets.yaml`, and
reflashing.

## Legacy: pre-v0.2.0 RSA-signed releases

Versions **v0.1.0** and **v0.1.1** were built under the custom PlatformIO +
Arduino architecture and are signed with a 2048-bit RSA key. The scheme
mirrored SmartEVSE-3.5 (`openssl dgst -sign -keyform PEM -sha256`, signature
prepended to `firmware.bin` → `firmware.signed.bin`). The public key is
still shipped at [`docs/signing_public_key.pem`](docs/signing_public_key.pem)
so old releases remain verifiable:

```bash
# v0.1.x only — NOT applicable to v0.2.0+ binaries
dd if=FanControl-v0.1.1.signed.bin of=firmware.sign bs=1 count=256 status=none
dd if=FanControl-v0.1.1.signed.bin of=firmware.bin  bs=1 skip=256  status=none
openssl dgst -verify docs/signing_public_key.pem -keyform PEM -sha256 \
  -signature firmware.sign firmware.bin
# → Verified OK
```

The `SECRET_RSA_KEY` repo secret remains configured but is unused from v0.2.0
onward. It is not deleted, so a maintainer could hypothetically revive signing
for a pre-v0.2.0 historical tag.

### Why the switch away from RSA signing?

v0.2.0 moved the entire firmware to ESPHome. ESPHome's OTA flow does not
support external signature verification; integrating a custom signed-OTA path
would have re-introduced most of the custom C++ layer we were retiring.
Given the LAN-only threat model, the OTA password + SHA-256 checksum is an
acceptable trade-off. See `PROJECT_PLAN.md` "History" for the decision log.

## Known out-of-scope items

- **No brute-force protection on ESPHome OTA.** Protect the device
  network-side; don't expose its HTTP endpoint to the internet.
- **Native API encryption is mandatory but unauthenticated at the transport
  level** — anyone with the API key can fully control the device. Treat the
  key as a root credential.
- **Web dashboard has no authentication.** It's served to anyone on the LAN
  who can reach `fancontrol.local:80`. If you need auth, put the device
  behind a reverse proxy with basic-auth.

## Reporting a vulnerability

Open a [private security advisory](https://github.com/basmeerman/FanControl/security/advisories/new)
on GitHub rather than a public issue. For non-exploitable hardening suggestions
(missing guards, weak defaults, documentation gaps), a regular issue with the
`safety` label is fine.
