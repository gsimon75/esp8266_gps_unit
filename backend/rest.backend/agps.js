const express = require("express");
const router = express.Router();
const fs = require("fs");
const db = require("../database");
const logger = require("../logger").getLogger("agps");
const utils = require("../utils");

const re_extract_cn = /\bCN=([^,]*)/i;

const agps_data = fs.readFileSync("./ota/agps.dat");

function op_get(req, res) {
    logger.debug("op_get()");
    res.set("Content-Type", "application/ubx"); // this is how the u-blox servers send it too
    return agps_data;
}

router.get("/",                (req, res, next) => utils.mwrap(req, res, next, () => op_get(req, res)));

module.exports = router;

// vim: set sw=4 ts=4 et:
