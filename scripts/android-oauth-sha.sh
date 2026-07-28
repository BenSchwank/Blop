#!/usr/bin/env bash
# Print SHA-1 / SHA-256 fingerprints for an Android upload keystore (OAuth console).
# Usage: bash scripts/android-oauth-sha.sh <keystore> <alias>
# Password: ANDROID_KEYSTORE_PASSWORD env, or keytool will prompt.
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <keystore.jks|keystore.keystore> <alias>" >&2
  exit 2
fi

ks=$1
alias_name=$2
if [[ ! -f "$ks" ]]; then
  echo "Keystore not found: $ks" >&2
  exit 1
fi

args=(-list -v -keystore "$ks" -alias "$alias_name")
if [[ -n "${ANDROID_KEYSTORE_PASSWORD:-}" ]]; then
  args+=(-storepass "$ANDROID_KEYSTORE_PASSWORD")
fi

echo "Fingerprints for alias '$alias_name' in $ks:"
keytool "${args[@]}" | awk '
  /SHA1:/ { print }
  /SHA256:/ { print }
'
