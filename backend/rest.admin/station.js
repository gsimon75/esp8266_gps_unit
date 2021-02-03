const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("station");
const utils = require("../utils");


function op_get_station(req) {
    logger.debug("op_get_station()");
    const id = req.params.id;
    const pipe = [
    ];

    if (id) {
        pipe.push({ $match: {id: id,} });
    }
    return db.cursor_all(db.stations().aggregate(pipe));
}


router.get("/",                 (req, res, next) => utils.mwrap(req, res, next, () => op_get_station(req)));
router.get("/:id",              (req, res, next) => utils.mwrap(req, res, next, () => op_get_station(req)));

module.exports = router;

// vim: set sw=4 ts=4 et:
