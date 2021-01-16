#!/bin/bash

# This script downloads the AssistNow Offline data in "aid" format for 14 days, so if the output file exists and
# is fresher than 7 days, we don't even try to fetch a new one.
#
# This script is intended to be invoked from a weekly crontab.
#
# https://www.u-blox.com/sites/default/files/products/documents/MultiGNSS-Assistance_UserGuide_%28UBX-13004360%29.pdf

# NOTE: this is an eval token, valid until 16 Apr 2021 only. I hope I get my permanent token until then.
TOKEN="pl8KPL_NJ0iCePXaG_X-gQ"

RETRIES=4
CONNECT_TIMEOUT=60
MAX_TIME=300

SERVERS=(
    "https://offline-live1.services.u-blox.com"
    "https://offline-live2.services.u-blox.com"
)

DESTFILE="$HOME/src/esp8266_gps_unit/backend/ota/agps.dat"
MIN_AGE_SEC=$((7 * 24*60*60))

# check if it's fresher than 7 days
if [ -r "$DESTFILE" ]; then
    FILE_MTIME=$(stat -c %Y "$DESTFILE")
    NOW=$(date +%s)
    FILE_AGE_SEC=$(($NOW - $FILE_MTIME))
    if [ $FILE_AGE_SEC -lt $MIN_AGE_SEC ]; then 
        echo "AGPS file is still valid (age=$FILE_AGE_SEC < $MIN_AGE_SEC), skip fetching" >&2
        exit 0
    fi
fi

# try to fetch the file
TEMPFILE="${DESTFILE}.tmp"
for SERVER in "${SERVERS[@]}"; do
    curl -o "$TEMPFILE" --location --fail --retry $RETRIES --max-time $MAX_TIME --connect-timeout $CONNECT_TIMEOUT "$SERVER/GetOfflineData.ashx?token=$TOKEN;gnss=gps;format=aid;days=14" && break
done

# fail noisily if the server refused it
if [ $? -ne 0 ]; then
    echo "Failed to fetch AGPS file, curl returned $?" >&2
    exit 1
fi

# just some sanity checks
if [ ! -r "$TEMPFILE" -o $(stat -c %s "$TEMPFILE") == 0 ]; then
    echo "Returned file is empty" >&2
    exit 2
fi

# replace the file
if ! mv -f "$TEMPFILE" "$DESTFILE"; then
    # moving failed (file permission problems?)
    echo "Could not move $TEMPFILE to $DESTFILE" >&2
    exit 3
fi

echo "Fetched new AGPS file to $DESTFILE" >&2
exit 0

# vim: set et indk= sw=4 ts=4:
