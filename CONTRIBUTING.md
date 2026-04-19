# Contributing to FanControl

Thanks for your interest in improving FanControl. This doc covers the local
build, the branching model, how release signing works, and the CHANGELOG rule.

`PROJECT_PLAN.md` is the single source of truth for features, architecture, and
quality gates — read it before opening a non-trivial PR. `CLAUDE.md` summarises
the hard architectural rules (no `delay()`, NVS return-check, fan fail-safe,
separate FreeRTOS tasks, etc.).

## Building locally

You need [PlatformIO Core](https://platformio.org/install/cli) (Python 3.11+).

```bash
pio run -e lolin_d32            # build firmware for the LOLIN D32
pio test -e native              # run Unity host tests
pio test -e native -f test_sensor   # run a single test folder
pio device monitor -b 115200    # serial monitor at 115200 baud
pio run -t upload               # OTA upload (espota → fancontrol.local)
```

CI runs `pio run` and `pio test -e native` on every push and PR to `main` and
`develop`. The firmware must stay under 1.5 MB — the CI job hard-fails otherwise.

## Branching model

```
main          stable releases, each with a tag + GitHub Release
  |
  +-- develop   integration branch, CI must be green
        |
        +-- feature/<short-name>   work here, PR into develop
        +-- fix/<short-name>       bug fixes, PR into develop
```

- Feature branches come off `develop` and PR back into `develop`.
- A release is cut by merging `develop` into `main` (`--no-ff`), then pushing a
  `vX.Y.Z` tag against `main`. The tag push triggers `release.yml`, which
  injects the version into `src/version.h`, builds, signs, and publishes.
- `main` is never a direct work branch.

Versioning follows [SemVer](https://semver.org/). Breaking MQTT topic or web API
changes bump MAJOR; new backwards-compatible features bump MINOR; bugfixes bump
PATCH. Pre-releases use suffixes like `v1.1.0-beta.1` and are auto-marked as
prereleases by the workflow (any tag containing `-`).

## Release signing (RSA + openssl, SmartEVSE-3.5 compatible)

Signed release binaries allow OTA clients and users to verify authenticity. The
scheme matches SmartEVSE-3.5 exactly: a 2048-bit RSA key signs a SHA-256 digest
of `firmware.bin`, and the 256-byte signature is **prepended** to the firmware
to produce `firmware.signed.bin`.

Generating the keypair (do this once, keep the private key OUT of the repo):

```bash
# Generate signing keypair (one time, keep private key OUT of the repo)
openssl genrsa -out fancontrol_signing_key.pem 2048
openssl rsa -in fancontrol_signing_key.pem -pubout -out fancontrol_signing_key.pub.pem

# Add the private key to GitHub secrets as SECRET_RSA_KEY
gh secret set SECRET_RSA_KEY < fancontrol_signing_key.pem

# Verifying a release artifact locally:
# 1. Split signature off the signed binary (signature is first 256 bytes for 2048-bit RSA)
dd if=FanControl-vX.Y.Z.signed.bin of=firmware.sign bs=1 count=256
dd if=FanControl-vX.Y.Z.signed.bin of=firmware.bin  bs=1 skip=256
openssl dgst -verify fancontrol_signing_key.pub.pem -keyform PEM -sha256 \
  -signature firmware.sign firmware.bin
# → "Verified OK"
```

If `SECRET_RSA_KEY` is not set (for example, on a fork), the signing step skips
silently and the release simply omits `.signed.bin`. `.bin` and `.bin.sha256`
are always published.

## CHANGELOG

Every PR must update `CHANGELOG.md` under the `## [Unreleased]` section.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

When releasing, the maintainer:
1. Renames `## [Unreleased]` to `## [X.Y.Z] - YYYY-MM-DD`.
2. Adds a fresh empty `## [Unreleased]` on top.
3. Merges to `main` and pushes the `vX.Y.Z` tag.

The release workflow extracts the matching section with `awk` and uses it as
the GitHub Release body — so keep headings clean and the version format exact.
