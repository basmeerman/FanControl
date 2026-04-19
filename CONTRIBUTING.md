# Contributing to FanControl

FanControl is an ESPHome project — the firmware is a single YAML file
([`fancontrol.yaml`](fancontrol.yaml)) compiled by ESPHome. If you want the
short version: get ESPHome installed, make changes, `esphome compile
fancontrol.yaml`, open a PR.

## Local setup

```bash
pipx install esphome
cp secrets.yaml.example secrets.yaml
# Edit secrets.yaml — WiFi creds, API encryption key, OTA password.
# Generate an API key with: openssl rand -base64 32
```

`secrets.yaml` is gitignored. Never commit real credentials.

## Development loop

```bash
esphome config   fancontrol.yaml   # lint only (seconds)
esphome compile  fancontrol.yaml   # full build (tens of seconds)
esphome run      fancontrol.yaml   # compile + flash (USB or OTA)
esphome logs     fancontrol.yaml   # stream runtime logs
```

The important gate is **compile**, not config — ESPHome only type-checks
lambdas at compile time.

## Branching model

```
main                stable releases, each tagged vX.Y.Z
  └─ develop        integration branch, CI must be green
        ├─ feature/<short-name>
        └─ fix/<short-name>
```

`main` is protected: PRs required, CI green before merge. `develop` requires CI
green. Solo-maintainer mode has `required_approving_review_count: 0` so you
can self-merge.

Release flow:

```bash
git checkout main
git merge develop --no-ff -m "Release v0.X.Y"
git tag vX.Y.Z
git push origin main --tags
# `release.yml` fires automatically on the tag push, compiles, and publishes
# firmware.{bin,ota.bin,factory.bin} + SHA-256 checksums to GitHub Releases.
```

## CHANGELOG rule

Every PR updates `CHANGELOG.md` under `## [Unreleased]` in
[Keep-a-Changelog](https://keepachangelog.com/en/1.1.0/) format. At release
time, the maintainer renames `## [Unreleased]` to `## [X.Y.Z] - YYYY-MM-DD`
and adds a fresh empty `## [Unreleased]` on top. The release workflow
extracts the matching section with `awk` and uses it as the GitHub Release
body — keep headings exact.

## Release verification

v0.2.0+ releases are **not RSA-signed**. Verify with the published SHA-256:

```bash
sha256sum -c FanControl-vX.Y.Z.bin.sha256
# → FanControl-vX.Y.Z.bin: OK
```

Pre-v0.2.0 releases (v0.1.0, v0.1.1) were RSA-signed under the old custom
C++ architecture and still verifiable against
[`docs/signing_public_key.pem`](docs/signing_public_key.pem) — see
[`SECURITY.md`](SECURITY.md).

## Architecture rules for PRs

Summarised here; full list in [`CLAUDE.md`](CLAUDE.md):

- Prefer built-in ESPHome components over lambdas.
- Lambdas stay short; the fan-curve interpolation is the exception.
- Every tunable is a `number.template` with `restore_value: true`.
- Safety behaviour (sensor stall → fan 100 %) must be present somewhere in the
  YAML, even if you refactor its shape.
- No hard-coded credentials. Ever.

## Reporting issues

Open a GitHub issue with the appropriate label (`bug`, `enhancement`,
`safety`, `docs`). For security-sensitive reports, use a
[private security advisory](https://github.com/basmeerman/FanControl/security/advisories/new)
instead.
