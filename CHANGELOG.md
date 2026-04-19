# Changelog

All notable changes to FanControl will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Release notes for each tagged version are extracted from this file by the
`release.yml` workflow — keep the `## [x.y.z] - YYYY-MM-DD` heading format exact.

## [Unreleased]

## [0.1.0] - 2026-04-19

### Added

- Phase 1 scaffold (PROJECT_PLAN.md §9 Phase 1):
  - PlatformIO project for `lolin_d32` (Arduino framework, `min_spiffs.csv` partitions)
  - NVS storage wrapper (`storage.{h,cpp}`) around `Preferences.h` with checked writes
  - GPIO map (`config.h`): DHT22=4, FAN_PWM=25, STATUS_LED=5, FACTORY_RESET=0
  - CI/CD pipeline: PlatformIO build + size gate + artifact upload, release workflow
    with SmartEVSE-3.5-compatible RSA firmware signing, Keep-a-Changelog release notes.

[Unreleased]: https://github.com/basmeerman/FanControl/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/basmeerman/FanControl/releases/tag/v0.1.0
