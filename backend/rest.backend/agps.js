const express = require("express");
const router = express.Router();
const fs = require("fs");
const db = require("../database");
const logger = require("../logger").getLogger("agps");
const utils = require("../utils");

const GPSTIME_START = 315964800; // unix timestamp of 1980-01-06T00:00:00Z
const SECONDS_IN_A_WEEK = 7 * 24 * 60 * 60;
const UBX_MAGIC = new Uint8Array([0xb5, 0x62]);

function ubx_message(m_class, m_id, payload) {
    let buffer = Buffer.alloc(8 + payload.length);
    buffer[0] = UBX_MAGIC[0];
    buffer[1] = UBX_MAGIC[1];
    buffer[2] = m_class;
    buffer[3] = m_id;
    buffer.writeUInt16LE(payload.length, 4);
    payload.copy(buffer, 6);

    let ck_a = 0;
    let ck_b = 0;
    for (let i = 2; i < (buffer.length - 2); ++i) {
           ck_a = (ck_a + buffer[i]) & 0xff;
           ck_b = (ck_b + ck_a) & 0xff;
    }
    buffer[buffer.length - 2] = ck_a;
    buffer[buffer.length - 1] = ck_b;
    return buffer;
}


function AID_INI() {
    // time: seconds since unix epoch 1970-01-01T00:00:00Z
    let gnow = Math.round(new Date().getTime() / 1000)- GPSTIME_START;
    let payload = Buffer.alloc(48);

    payload.writeInt32LE(0, 0);                                     // .lat, deg*1e-7
    payload.writeInt32LE(0, 4);                                     // .lon, deg*1e-7
    payload.writeInt32LE(0, 8);                                     // .alt, cm
    payload.writeUInt32LE(0, 12);                                   // .posAcc, cm
    payload.writeUInt16LE(0x01, 16);                                // .tmCfg
    payload.writeUInt16LE(gnow / SECONDS_IN_A_WEEK, 18);            // .wn 
    payload.writeUInt32LE(1000 * (gnow % SECONDS_IN_A_WEEK), 20);   // .tow, ms
    payload.writeInt32LE(0, 24);                                    // .towNs, ns
    payload.writeUInt32LE(10000, 28);                               // .tAccMs, ms
    payload.writeUInt32LE(0, 32);                                   // .tAccNs, ns
    payload.writeInt32LE(0, 36);                                    // .clkD
    payload.writeUInt32LE(0, 40);                                   // .clkDAcc
    payload.writeUInt32LE(0x0a, 44);                                // .flags, 0x0a = tp + time

    return ubx_message(0x0b, 0x01, payload);
}

function update_agps(req, res) {
    let message = Buffer.from(req.body.message);
    logger.debug("update_agps(): type=" + req.body.type + ", svid=" + req.body.svid + ", length=" + message.length);
    return db.agps().updateOne({
        type: req.body.type,
        svid: req.body.svid,
    }, {
        $set: {
            message,
        },
    }, {
        upsert: true,
    }).then(() => null);
}

function get_agps(req, res) {
    let msgs = [
        AID_INI(),
    ];

    return db.agps().find({}).forEach(m => { 
        msgs.push(Buffer.from(m.message.buffer));
    }).then(() => {
        let result =  Buffer.concat(msgs);
        logger.debug("get_agps(), length=" + result.length);
        res.set("Content-Type", "application/ubx"); // this is how the u-blox servers send it too
        return result;
    });
}

router.post("/",               (req, res, next) => utils.mwrap(req, res, next, () => update_agps(req, res)));
router.get("/",                (req, res, next) => utils.mwrap(req, res, next, () => get_agps(req, res)));

module.exports = router;

// vim: set sw=4 ts=4 et:
