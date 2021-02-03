const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("unit");
const utils = require("../utils");


function op_get_all(req) {
    logger.debug("op_get_all()");
    return db.cursor_all(db.unit_status().find());
}


function op_get(req) {
    const id = req.params.id;
    logger.debug("op_get(" + id + ")");
    return db.unit_status().findOne({unit: id});
}


router.get("/",                 (req, res, next) => utils.mwrap(req, res, next, () => op_get_all(req)));
router.get("/:id",              (req, res, next) => utils.mwrap(req, res, next, () => op_get(req)));

module.exports = router;

// vim: set sw=4 ts=4 et:
