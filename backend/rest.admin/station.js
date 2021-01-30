const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("station");
const utils = require("../utils");


function get_time_filter(req) {
    var result = {
        until: ("until" in req.query) ? parseInt(req.query.until) : Math.round(Date.now() / 1000),
        duration: null,
    };

    if ("seconds" in req.query)
        result.duration += parseInt(req.query.seconds);
    if ("minutes" in req.query)
        result.duration += 60 * parseInt(req.query.minutes);
    if ("hours" in req.query)
        result.duration += 60 * 60 * parseInt(req.query.hours);
    if ("days" in req.query)
        result.duration += 24 * 60 * 60 * parseInt(req.query.days);
    
    return result;
}

const STATUS_VALUES = [ "offline", "charging", "available", "in_use" ];
const MAX_RESULT_RECORDS = 1024;

function get_status_filter(req) {
    return ("status" in req.query) ? req.query.status.split(",") : null;
}

function filtered_pipeline(req) {
    const name = req.params.name;
    const tf = get_time_filter(req);
    const sf = get_status_filter(req);

    var filter = {
        time: {
            $ne: null,
            $lt: tf.until,
        },
    };
    if (name) {
        filter.unit = { $eq: name };
    }
    if (sf) {
        filter.status = { $in: sf };
    }
    
    var pipe;

    if (tf.duration !== null) {
        filter.time.$gt = tf.until - tf.duration;
        pipe = [
            { $match: filter },
            { $sort: { unit: 1, time: -1 }, },
            { $limit: MAX_RESULT_RECORDS },
        ];
    }
    else {
        // last records only:
        // db.traces.aggregate([ {$match: ...},   {$sort: {unit: 1, time: -1}}, {$group: {_id: "$unit", lastrec: {$first: "$$CURRENT"}}}, {$replaceWith: "$lastrec"}    ])
        pipe = [
            { $match: filter },
            { $sort: { unit: 1, time: -1 } },
            { $group: {_id: "$unit", lastrec: { $first: "$$CURRENT" } } },
            { $replaceWith: "$lastrec" },
            { $limit: MAX_RESULT_RECORDS },
        ];
    }

    return pipe;
}


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
