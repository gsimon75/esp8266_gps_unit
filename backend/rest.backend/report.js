const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("report");
const utils = require("../utils");

const re_extract_cn = /\bCN=([^,]*)/i;

function op_insert(req) {
    let unit_dn = req.get("X-SSL-Subject-DN");
    if (!unit_dn) {
        throw utils.error(400, "Missing SSL subject DN");
    }
    let unit_cn = re_extract_cn.exec(unit_dn);
    if (!unit_cn) {
        throw utils.error(400, "SSL subject DN has no CN");
    }
    let record = { ...req.body, unit: unit_cn[1] };
    logger.debug("op_insert(" + JSON.stringify(record) + ")");

    return db.traces().insertOne(record).then(() => null);
}

router.post("/",                (req, res, next) => utils.mwrap(req, res, next, () => op_insert(req)));

module.exports = router;

// vim: set sw=4 ts=4 et:
