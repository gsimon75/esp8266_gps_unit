const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("report");
const utils = require("../utils");
const events = require("../events");

const re_extract_cn = /\bCN=([^,]*)/i;

function op_report(req) {
    let unit_dn = req.get("X-SSL-Subject-DN");
    if (!unit_dn) {
        throw utils.error(400, "Missing SSL subject DN");
    }
    let unit_cn = re_extract_cn.exec(unit_dn);
    if (!unit_cn) {
        throw utils.error(400, "SSL subject DN has no CN");
    }
    let unit = unit_cn[1];
    let now = Math.round(new Date().getTime() / 1000);

    logger.debug("op_report, unit='" + unit + "', report:" + JSON.stringify(req.body));
    let promises = [];
    if (("lat" in req.body) && ("lon" in req.body)) {
        let record = {
            unit,
            time: now,
            lat: req.body.lat,
            lon: req.body.lon,
            azi: req.body.azi,
            spd: req.body.spd,
        }
        events.emitter.emit("sendit", "unit_location", record);
        promises.push(db.unit_location().insertOne(record));
    }
    if ("bat" in req.body) {
        let record = {
            unit,
            time: now,
            bat: req.body.bat,
        }
        events.emitter.emit("sendit", "unit_battery", record);
        promises.push(db.unit_battery().insertOne(record));
    }

    return Promise.all(promises).then(() => null);
}

router.post("/",                (req, res, next) => utils.mwrap(req, res, next, () => op_report(req)));

module.exports = router;

// vim: set sw=4 ts=4 et:
