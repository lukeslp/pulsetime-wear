#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
key_alias="pulsetime-upload"
keychain_service="com.lukesteuber.pulsetime.upload-key"
config_root="${XDG_CONFIG_HOME:-${HOME}/.config}/pulsetime"
keystore_path="${config_root}/pulsetime-upload.jks"
stage_dir="${repo_root}/dist/play"

if [[ ! -f "${keystore_path}" ]]; then
  echo "Missing upload key. Run scripts/create-play-upload-key.sh first." >&2
  exit 1
fi

upload_password="$(security find-generic-password -a "${key_alias}" -s "${keychain_service}" -w)"
export PULSETIME_UPLOAD_STORE_FILE="${keystore_path}"
export PULSETIME_UPLOAD_STORE_PASSWORD="${upload_password}"
export PULSETIME_UPLOAD_KEY_ALIAS="${key_alias}"
export PULSETIME_UPLOAD_KEY_PASSWORD="${upload_password}"
export JAVA_HOME="$(/usr/libexec/java_home -v 21)"
export ANDROID_HOME="${ANDROID_HOME:-${HOME}/Library/Android/sdk}"

cd "${repo_root}"
./gradlew clean \
  :shared:testDebugUnitTest \
  :app:testDebugUnitTest \
  :mobile:testDebugUnitTest \
  :app:lintRelease \
  :mobile:lintRelease \
  :app:assembleRelease \
  :mobile:assembleRelease \
  :app:bundleRelease \
  :mobile:bundleRelease

mkdir -p "${stage_dir}"
find "${stage_dir}" -maxdepth 1 -type f -delete

phone_aab="${stage_dir}/pulsetime-phone-0.3.0-10000003.aab"
wear_aab="${stage_dir}/pulsetime-wear-0.3.0-20000003.aab"
phone_apk="${stage_dir}/pulsetime-phone-0.3.0-10000003.apk"
wear_apk="${stage_dir}/pulsetime-wear-0.3.0-20000003.apk"
cp mobile/build/outputs/bundle/release/mobile-release.aab "${phone_aab}"
cp app/build/outputs/bundle/release/app-release.aab "${wear_aab}"
cp mobile/build/outputs/apk/release/mobile-release.apk "${phone_apk}"
cp app/build/outputs/apk/release/app-release.apk "${wear_apk}"

jarsigner -verify "${phone_aab}" >/dev/null
jarsigner -verify "${wear_aab}" >/dev/null
apksigner="$(ls -1 "${ANDROID_HOME}"/build-tools/*/apksigner 2>/dev/null | sort -V | tail -1)"
if [[ -z "${apksigner}" ]]; then
  echo "Android apksigner was not found." >&2
  exit 1
fi
"${apksigner}" verify --verbose "${phone_apk}" >/dev/null
"${apksigner}" verify --verbose "${wear_apk}" >/dev/null

phone_cert="$(keytool -printcert -jarfile "${phone_aab}" | awk -F': ' '/SHA256:/{print $2; exit}')"
wear_cert="$(keytool -printcert -jarfile "${wear_aab}" | awk -F': ' '/SHA256:/{print $2; exit}')"
if [[ -z "${phone_cert}" || "${phone_cert}" != "${wear_cert}" ]]; then
  echo "Phone and Wear bundles are not signed with the same upload certificate." >&2
  exit 1
fi

(
  cd "${stage_dir}"
  shasum -a 256 \
    "$(basename "${phone_apk}")" \
    "$(basename "${wear_apk}")" \
    "$(basename "${phone_aab}")" \
    "$(basename "${wear_aab}")" > SHA256SUMS.txt
)
printf 'Upload certificate SHA-256: %s\n' "${phone_cert}" > "${stage_dir}/UPLOAD-CERTIFICATE.txt"
printf '%s\n' \
  'PulseTime 0.3.0 signed Android artifacts' \
  'Phone: version code 10000003' \
  'Wear OS: version code 20000003' \
  'Package: com.lukesteuber.pulsetime.wear' > "${stage_dir}/RELEASE.txt"

unset upload_password PULSETIME_UPLOAD_STORE_PASSWORD PULSETIME_UPLOAD_KEY_PASSWORD
echo "Signed Play artifacts staged in ${stage_dir}."
