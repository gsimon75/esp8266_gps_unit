const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("startup");
const utils = require("../utils");
const events = require("../events");

const re_extract_cn = /\bCN=([^,]*)/i;

function op_startup(req) {
    let unit_dn = req.get("X-SSL-Subject-DN");
    if (!unit_dn) {
        throw utils.error(400, "Missing SSL subject DN");
    }
    let unit_cn = re_extract_cn.exec(unit_dn);
    if (!unit_cn) {
        throw utils.error(400, "SSL subject DN has no CN");
    }
    let now = Math.round(new Date().getTime() / 1000);
    let record = { ...req.body, unit: unit_cn[1], time: now };
    // NOTE: it should contain the actual unit nonce
    logger.debug("startup(" + JSON.stringify(record) + ")");

    return "ok"; // FIXME: do something with the nonce
}

router.post("/",                (req, res, next) => utils.mwrap(req, res, next, () => op_startup(req)));

module.exports = router;

// vim: set sw=4 ts=4 et:
