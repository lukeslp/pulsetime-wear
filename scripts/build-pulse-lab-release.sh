#!/usr/bin/env bash
# Build the release-signed Pulse Lab APK for direct download.
#
# The signing values come from the environment, so run this through the
# release toolkit, which reads them from the login Keychain:
#
#   ~/workspace/android-release-toolkit/scripts/android-release \
#     with-signing --profile pulse-lab -- scripts/build-pulse-lab-release.sh
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
stage_dir="${repo_root}/dist/pulse-lab"

for name in PULSE_LAB_RELEASE_STORE_FILE PULSE_LAB_RELEASE_STORE_PASSWORD \
            PULSE_LAB_RELEASE_KEY_ALIAS PULSE_LAB_RELEASE_KEY_PASSWORD; do
  if [[ -z "${!name:-}" ]]; then
    echo "Missing ${name}. Run this script through the toolkit's with-signing command." >&2
    exit 1
  fi
done

if [[ -z "${JAVA_HOME:-}" ]]; then
  JAVA_HOME="$(/usr/libexec/java_home -v 21 2>/dev/null || true)"
  if [[ -z "${JAVA_HOME}" && -d /opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home ]]; then
    JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
  fi
  export JAVA_HOME
fi
export ANDROID_HOME="${ANDROID_HOME:-${HOME}/Library/Android/sdk}"
export PATH="${JAVA_HOME}/bin:${PATH}"

cd "${repo_root}"
./gradlew clean \
  :shared:testDebugUnitTest \
  :hapticlab:testDebugUnitTest \
  :hapticlab:lintRelease \
  :hapticlab:assembleRelease

build_tools="$(ls -1d "${ANDROID_HOME}"/build-tools/* 2>/dev/null | sort -V | tail -1)"
if [[ -z "${build_tools}" ]]; then
  echo "Android build-tools were not found." >&2
  exit 1
fi

built="hapticlab/build/outputs/apk/release/hapticlab-release.apk"
badging="$("${build_tools}/aapt" dump badging "${built}")"
version_name="$(sed -n "s/^package:.* versionName='\([^']*\)'.*/\1/p" <<<"${badging}")"
if grep -q '^application-debuggable' <<<"${badging}"; then
  echo "The release APK is debuggable; refusing to stage it." >&2
  exit 1
fi
certificate="$("${build_tools}/apksigner" verify --print-certs "${built}" \
  | sed -n 's/^Signer #1 certificate SHA-256 digest: //p')"
if [[ -z "${certificate}" || "${certificate}" == e1faa235* ]]; then
  echo "The release APK is unsigned or signed with the Android debug key." >&2
  exit 1
fi

mkdir -p "${stage_dir}"
find "${stage_dir}" -maxdepth 1 -type f -delete
apk="pulse-lab-${version_name}.apk"
cp "${built}" "${stage_dir}/${apk}"
(cd "${stage_dir}" && shasum -a 256 "${apk}" > SHA256SUMS.txt)
printf 'Release certificate SHA-256: %s\n' "${certificate}" > "${stage_dir}/RELEASE-CERTIFICATE.txt"
echo "Signed Pulse Lab ${version_name} staged in ${stage_dir}."
