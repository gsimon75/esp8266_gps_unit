#!/bin/bash

set -e

CA_BASE="iot_ca"
CA_SUBJECT="/C=AE/ST=Dubai/L=MotorCity/O=wodeewa/CN=iot"
CA_DAYS=3650

function gen_ca() {
    echo "Generating CA credentials $CA_BASE.* ..."
    [ -s "$CA_BASE.key" -a -s "$CA_BASE.crt" ] || openssl req -nodes -days $CA_DAYS -x509 -extensions v3_ca -newkey rsa:4096 -subj "$CA_SUBJECT" -keyout "$CA_BASE.key" -out "$CA_BASE.crt"
    [ -s "$CA_BASE.crt.der" ] || openssl x509 -in "$CA_BASE.crt" -outform der -out "$CA_BASE.crt.der"
    [ -s "$CA_BASE.srl" ] || openssl rand -hex 16 >"$CA_BASE.srl"
}


UNIT_SUBJECT_BASEDN="/C=AE/ST=Dubai/L=MotorCity/O=wodeewa/OU=iot"
UNIT_DAYS=1200

function gen_unit_credentials() {
    local UNIT_CN="$1"
    local UNIT_BASE="$2"

    if [ -z "$UNIT_CN" ]; then
        echo "Unit CN is missing" >&2
        exit 1
    fi

    # if no filename-base is given, generate it from the CN (s/ /_/g)
    if [ -z "$UNIT_BASE" ]; then
        UNIT_BASE="${UNIT_CN// /_}"
        UNIT_BASE="${UNIT_BASE,,}"
    fi
    
    echo "Generating Unit credentials for '$UNIT_CN' as $UNIT_BASE.* ..."
    [ -s "$UNIT_BASE.key" ] || openssl genrsa -out "$UNIT_BASE.key" 4096
    [ -s "$UNIT_BASE.p8" -a "$UNIT_BASE.p8" -nt "$UNIT_BASE.key" ] || openssl pkcs8 -topk8 -nocrypt -in "$UNIT_BASE.key" -outform der -out "$UNIT_BASE.p8"
    [ -s "$UNIT_BASE.csr" -a "$UNIT_BASE.csr" -nt "$UNIT_BASE.key" ] || openssl req -new -key "$UNIT_BASE.key" -subj "$UNIT_SUBJECT_BASEDN/CN=${UNIT_CN//\//\\\/}" -out "$UNIT_BASE".csr
    [ -s "$UNIT_BASE.crt.der" -a "$UNIT_BASE.crt.der" -nt "$UNIT_BASE.csr" ] || openssl x509 -req -CAkey "$CA_BASE.key" -CA "$CA_BASE.crt" -CAserial "$CA_BASE.srl" -days $UNIT_DAYS -in "$UNIT_BASE.csr" -outform der -out "$UNIT_BASE.crt.der"
}

# You need to do this only once!
gen_ca
gen_unit_credentials "Factory"
gen_unit_credentials "Simulated"
for i in {1..3}; do
    gen_unit_credentials "Unit $i"
done

