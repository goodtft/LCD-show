#!/bin/bash
# update.sh — the ONLY command to run on the Pi after changes are
# pushed to this repo:
#
#   bash update.sh
#
# (bash, not ./ — the GitHub snapshot push cannot preserve the
# executable bit, and this way that never matters.)
#
# It pulls the latest version of the repo and runs the desktop
# setup, which installs/refreshes everything user-side. Local
# "changes" in the Pi's clone are discarded first: on the Pi the
# repo is a deployment target, and the only local changes that
# ever appear there are file-permission flips from the push
# process itself.
#
# The script re-executes itself from /tmp before pulling, so the
# pull can safely rewrite every file in the repo — including this
# one and the setup script it is about to run.

set -u

if [ -z "${MHS35_UPDATE_TMP:-}" ]; then
  DIR=$(cd "$(dirname "$0")" && pwd)
  cp "$0" /tmp/.mhs35-update.sh
  MHS35_UPDATE_TMP=1 exec bash /tmp/.mhs35-update.sh "$DIR"
fi

cd "$1" || { echo "cannot cd to $1"; exit 1; }
echo "updating $(pwd)"

git checkout -- .
git pull --ff-only || { echo "git pull failed — see above"; exit 1; }

bash ./MHS35-desktop-setup.sh
