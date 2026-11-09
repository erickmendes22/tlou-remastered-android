#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
java com.sun.tools.javac.Main -d "$test_dir" \
  android-launcher/app/src/main/java/ro/cobrabm/fexdroid/FpsMeter.java \
  android-launcher/tests/FpsMeterTest.java
java -cp "$test_dir" ro.cobrabm.fexdroid.FpsMeterTest

java com.sun.tools.javac.Main -d "$test_dir" \
  android-launcher/app/src/main/java/ro/cobrabm/fexdroid/SaveArchive.java \
  android-launcher/tests/SaveArchiveTest.java
java -cp "$test_dir" ro.cobrabm.fexdroid.SaveArchiveTest
