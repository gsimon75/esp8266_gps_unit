const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("unit");
const utils = require("../utils");


function get_time_filter(req) {
    var result = {
        from: null,
        until: null,
        duration: null,
    };
    //until: ("until" in req.query) ? parseInt(req.query.until) : Math.round(Date.now() / 1000),
    if ("from" in req.query)
        result.from = parseInt(req.query.from);
    if ("until" in req.query)
        result.until = parseInt(req.query.until);

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

function get_num_filter(req) {
    return ("num" in req.query) ? parseInt(req.query.num) : MAX_RESULT_RECORDS ;
}

function filtered_pipeline(req) {
    const name = req.params.name;
    const tf = get_time_filter(req);
    const sf = get_status_filter(req);
    const nf = get_num_filter(req);

    var pipe;
    var time_sort_dir = -1;

    var filter = {
        time: {
            $ne: null,
        },
    };
    if (tf.until) {
        filter.time.$lt = tf.until;
        time_sort_dir = -1;
    }
    if (tf.from) {
        filter.time.$gt = tf.from;
        time_sort_dir = 1;
    }
    if (sf) {
        filter.status = { $in: sf };
    }
    
    
    if (tf.from && tf.until && (tf.duration !== null)) {
        // make up your mind...
        delete tf.duration;
    }

    if (tf.duration) {
        if (tf.until) {
            filter.time.$gt = tf.until - tf.duration;
        }
        else if (tf.from) {
            filter.time.$lt = tf.from + tf.duration;
        }
        else {
            filter.time.$gt = Math.round(Date.now() / 1000) - tf.duration;
        }
    }

    if (name) {
        filter.unit = { $eq: name };
        pipe = [
            { $match: filter },
            { $sort: { unit: 1, time: time_sort_dir }, },
            { $limit: nf },
        ];
    }
    else {
        // last records only:
        // db.unit_location.aggregate([ {$match: ...},   {$sort: {unit: 1, time: -1}}, {$group: {_id: "$unit", lastrec: {$first: "$$CURRENT"}}}, {$replaceWith: "$lastrec"}    ])
        pipe = [
            { $match: filter },
            { $sort: { unit: 1, time: -1 } },
            { $group: {_id: "$unit", lastrec: { $first: "$$CURRENT" } } },
            { $replaceWith: "$lastrec" },
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
    return db.cursor_all(db.unit_status().aggregate(pipe));
}


function op_get_trace(req) {
    logger.debug("op_get_trace(), url=" + req.url);
    //utils.dump_request(req);
    if (!req.session || !req.session.is_technician) {
        throw utils.error(401, "must be technician");
    }
    const pipe = filtered_pipeline(req);
    logger.debug("pipeline: " + JSON.stringify(pipe));
    return db.cursor_all(db.unit_location().aggregate(pipe));
}


function op_get_battery(req) {
    logger.debug("op_get_battery()");
    if (!req.session || !req.session.is_technician) {
        throw utils.error(401, "must be technician");
    }
    const pipe = filtered_pipeline(req);
    return db.cursor_all(db.unit_location().aggregate(pipe));
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

router.get("/trace",            (req, res, next) => utils.mwrap(req, res, next, () => op_get_trace(req)));
router.get("/trace/:name",      (req, res, next) => utils.mwrap(req, res, next, () => op_get_trace(req)));
router.get("/status",           (req, res, next) => utils.mwrap(req, res, next, () => op_get_status(req)));
router.get("/status/:name",     (req, res, next) => utils.mwrap(req, res, next, () => op_get_status(req)));
router.get("/battery",            (req, res, next) => utils.mwrap(req, res, next, () => op_get_battery(req)));
router.get("/battery/:name",      (req, res, next) => utils.mwrap(req, res, next, () => op_get_battery(req)));

module.exports = router;

// vim: set sw=4 ts=4 et:
