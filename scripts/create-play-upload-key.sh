#!/usr/bin/env bash
set -euo pipefail

key_alias="pulsetime-upload"
keychain_service="com.lukesteuber.pulsetime.upload-key"
config_root="${XDG_CONFIG_HOME:-${HOME}/.config}/pulsetime"
keystore_path="${config_root}/pulsetime-upload.jks"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This setup script stores the password in macOS Keychain and must run on macOS." >&2
  exit 1
fi

if [[ -e "${keystore_path}" ]]; then
  echo "Upload keystore already exists at ${keystore_path}; leaving it unchanged."
  exit 0
fi

mkdir -p "${config_root}"
chmod 700 "${config_root}"

upload_password="$(openssl rand -hex 24)"
security add-generic-password \
  -a "${key_alias}" \
  -s "${keychain_service}" \
  -w "${upload_password}" \
  -U >/dev/null

keytool -genkeypair \
  -keystore "${keystore_path}" \
  -storepass "${upload_password}" \
  -alias "${key_alias}" \
  -keypass "${upload_password}" \
  -keyalg RSA \
  -keysize 4096 \
  -validity 10000 \
  -dname "CN=Luke Steuber, OU=Android, O=Luke Steuber, L=Los Angeles, ST=California, C=US" >/dev/null

chmod 600 "${keystore_path}"
unset upload_password

echo "Created the PulseTime Play upload key at ${keystore_path}."
echo "Back up this keystore and its macOS Keychain password before the first Play upload."
