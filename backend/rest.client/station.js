const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("station");
const utils = require("../utils");


function op_get_station(req) {
    logger.debug("op_get_station()");
    const id = req.params.id;
    const lat = req.query.lat;
    const lon = req.query.lon;
    const _for = req.query.for;
    const num = req.query.num;

    const pipe = [
    ];

    if (lat && lon) {
        let geofilter = {
            near: [ parseFloat(lon), parseFloat(lat) ], // NOTE: no mistake, the correct order is [lon, lat]
            distanceField: "dist",
        };
        pipe.push({ $geoNear: geofilter }); // NOTE: must be the first one
    }

    let filter;

    if (id) {
        filter =  { id };
    }
    else if (_for == "take") {
        filter = { $gt: ["$in_use", 0] }
    }
    else if (_for == "return") {
        filter = { $lt: ["$in_use", "$capacity"] }
    }

    if (filter) {
        pipe.push({ $match: {$expr: filter}  });
    }

    if (num) {
        pipe.push({ $limit: parseInt(num) });
    }
    logger.debug("pipe=" + JSON.stringify(pipe));
    return db.cursor_all(db.stations().aggregate(pipe));
}


router.get("/",                 (req, res, next) => utils.mwrap(req, res, next, () => op_get_station(req)));
router.get("/:id",              (req, res, next) => utils.mwrap(req, res, next, () => op_get_station(req)));

module.exports = router;

// vim: set sw=4 ts=4 et:
