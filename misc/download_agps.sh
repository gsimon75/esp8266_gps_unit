#!/bin/bash

# This script downloads the AssistNow Offline data in "aid" format for 14 days, so if the output file exists and
# is fresher than 7 days, we don't even try to fetch a new one.
#
# This script is intended to be invoked from a weekly crontab.
#
# https://www.u-blox.com/sites/default/files/products/documents/MultiGNSS-Assistance_UserGuide_%28UBX-13004360%29.pdf

# NOTE: this is an eval token, valid until 16 Apr 2021 only. I hope I get my permanent token until then.
TOKEN="pl8KPL_NJ0iCePXaG_X-gQ"

# NOTE: Oops, the file is too big. 4472 + days * 6708 bytes. We have about 10k...
# Maybe this offline thing isn't exactly for us
# The online thing: curl -o online.all "https://online-live1.services.u-blox.com/GetOnlineData.ashx?token=pl8KPL_NJ0iCePXaG_X-gQ;format=aid;datatype=eph,alm,aux"

RETRIES=4
CONNECT_TIMEOUT=60
MAX_TIME=300

SERVERS=(
    "https://offline-live1.services.u-blox.com"
    "https://offline-live2.services.u-blox.com"
)

DESTFILE="$HOME/src/esp8266_gps_unit/backend/ota/agps.dat"

# 1, 2, 3, 5, 7, 10 or 14
DAYS=14
MIN_AGE_SEC=$((7 * 24*60*60))

# check if it's fresh enough
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
    curl -o "$TEMPFILE" --location --fail --retry $RETRIES --max-time $MAX_TIME --connect-timeout $CONNECT_TIMEOUT "$SERVER/GetOfflineData.ashx?token=$TOKEN;gnss=gps;format=aid;days=$DAYS" && break
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
