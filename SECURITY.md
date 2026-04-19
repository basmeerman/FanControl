# Security Policy

## Verifying release artifacts

Every release attaches three files:

| File | Purpose |
|---|---|
| `FanControl-vX.Y.Z.bin` | Unsigned firmware |
| `FanControl-vX.Y.Z.bin.sha256` | SHA-256 checksum of the unsigned firmware |
| `FanControl-vX.Y.Z.signed.bin` | The signed firmware: a **2048-bit RSA signature prepended** to the unsigned firmware |

The public key used to sign releases lives at
[`docs/signing_public_key.pem`](docs/signing_public_key.pem) in this repository.

### Verify checksum

```bash
sha256sum -c FanControl-vX.Y.Z.bin.sha256
# → FanControl-vX.Y.Z.bin: OK
```

### Verify RSA signature

The signing scheme matches SmartEVSE-3.5 exactly: the first 256 bytes of
`.signed.bin` are the RSA-SHA256 signature, and the remaining bytes are the
original firmware.

```bash
# Split off the 256-byte signature
dd if=FanControl-vX.Y.Z.signed.bin of=firmware.sign bs=1 count=256 status=none
dd if=FanControl-vX.Y.Z.signed.bin of=firmware.bin  bs=1 skip=256  status=none

# Verify against the public key
openssl dgst -verify docs/signing_public_key.pem -keyform PEM -sha256 \
  -signature firmware.sign firmware.bin
# → Verified OK

# Sanity: the extracted firmware should byte-match the unsigned release artifact
cmp firmware.bin FanControl-vX.Y.Z.bin
```

If `openssl` prints anything other than `Verified OK`, **do not flash the
firmware** — the artifact may have been tampered with. Open an issue under
[Reporting a vulnerability](#reporting-a-vulnerability) below.

### Key rotation

If the current signing key is ever compromised or rotated, the replacement
public key will be committed to `docs/signing_public_key.pem` in the same
release (or the release preceding it), and the change will be called out in
`CHANGELOG.md` under a `### Security` heading. Old releases remain signed
with the old key; verify them against the key shipped in the corresponding
tag rather than the current `HEAD`.

## Scope

FanControl runs on a LAN by design (home battery room controller, talking to a
local MQTT broker and serving a local web UI). The following are **explicitly
out of scope** for security hardening at this stage:

- TLS verification for MQTT. Port `8883` enables `WiFiClientSecure::setInsecure()` —
  no CA certificate check. This matches the SmartEVSE-3.5 approach and is adequate
  for a trusted LAN broker. If you run this internet-exposed, pin a CA or
  fingerprint first (tracked by `TODO(security)` in `src/mqtt.cpp`).
- Authentication on the web UI settings pages. OTA is password-gated via
  ElegantOTA; status/settings pages are open to anyone on the LAN.
- Brute-force protection on the OTA form. ElegantOTA does not rate-limit;
  protect the device network-side.

## Reporting a vulnerability

Please open a [private security advisory](https://github.com/basmeerman/FanControl/security/advisories/new)
on GitHub. Do not file public issues for security bugs.

For non-exploitable hardening suggestions (missing guards, weak defaults,
documentation gaps), a regular issue with the `safety` label is fine.
