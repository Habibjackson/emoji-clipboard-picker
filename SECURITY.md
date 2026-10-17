# Security Policy

## Supported versions

This project is a pre-release scaffold. Security fixes target the current `main` branch; no released version is supported yet.

## Report a vulnerability

Do not disclose a vulnerability or real clipboard data in a public issue. Use the repository host's private vulnerability reporting feature if one is available. Otherwise, open a public issue asking for a private reporting channel, without including technical details. Once a private channel is established, provide the affected commit, reproduction steps using synthetic data, expected impact, and any proposed fix.

Maintainers will coordinate investigation and disclosure with the reporter. There is no published response-time guarantee yet.

## Sensitive data expectations

Clipboard history must remain local to the current user. Protect database files with user-only permissions, avoid payloads in logs and crash reports, and check that paste or clipboard restoration never overwrites a newer user copy. See [TODO.md](TODO.md) for the planned security and privacy checks.
