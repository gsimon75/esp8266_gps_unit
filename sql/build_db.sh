#!/usr/bin/env bash

PROJECT_ROOT="${1:=.}"
DBFILE="$PROJECT_ROOT/www/saga.db"

rm -f -- "$DBFILE"

(cat -- "$PROJECT_ROOT"/sql/*.sql;  echo ".quit") | sqlite3 "$DBFILE"
