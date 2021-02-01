const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("unit");
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


function op_get_status(req) {
    logger.debug("op_get_status()");
    if (!req.session || !req.session.is_technician) {
        throw utils.error(401, "must be technician");
    }
    const pipe = filtered_pipeline(req);
    return db.cursor_all(db.units().aggregate(pipe));
}


function op_get_trace(req) {
    logger.debug("op_get_trace()");
    if (!req.session || !req.session.is_technician) {
        throw utils.error(401, "must be technician");
    }
    const pipe = filtered_pipeline(req);
    return db.cursor_all(db.traces().aggregate(pipe));
}


/*
 * 1. What units do we have/had? -> List of units: id, name, status, user
 * Narrowing: 
 * - by date (until (def: now), days, hours, minutes, seconds (def: last one only)
 * - by status (offline, charging, available, in_use)
 * Endpoint: GET /unit/status?until=1611775014&hours=3&status=available
 * Endpoint: GET /unit/status/Unit%201
 *
 * 2. Where is/was a unit? -> List of unit positions (location, charge)
 * - by date (until (def: now), days, hours, minutes, seconds (def: last one only)
 * Endpoint: GET /unit/trace/Unit%201?until=1611775014&hours=3
 */

router.get("/status",           (req, res, next) => utils.mwrap(req, res, next, () => op_get_status(req)));
router.get("/status/:name",     (req, res, next) => utils.mwrap(req, res, next, () => op_get_status(req)));
router.get("/trace",            (req, res, next) => utils.mwrap(req, res, next, () => op_get_trace(req)));
router.get("/trace/:name",      (req, res, next) => utils.mwrap(req, res, next, () => op_get_trace(req)));

module.exports = router;

// vim: set sw=4 ts=4 et:
