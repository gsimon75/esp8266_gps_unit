const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("unit");
const utils = require("../utils");


function get_units(pipe) {
    let items = {};
    let promises = [];
    return db.unit_status().aggregate(pipe).forEach(u => {
        items[u.unit] = {
            unit: u.unit,
            status: u.status,
            user: u.user,
            status_time: u.time,
        };
        let pipe_last_entry_of_this_unit = [
            { $match: { unit: u.unit } },
            { $sort: { time: -1 } },
            { $limit: 1 },
        ];
        promises.push(Promise.all([
            db.unit_location().aggregate(pipe_last_entry_of_this_unit).next(), // [0]
            db.unit_battery().aggregate(pipe_last_entry_of_this_unit).next(),  // [1]
        ]));
    }).then(() => {
        return Promise.all(promises);
    }).then(locbats => {
        locbats.forEach(locbat => {
            let loc = locbat[0];
            let bat = locbat[1];
            if (loc) {
                items[loc.unit].location_time = loc.time;
                items[loc.unit].lat = loc.lat;
                items[loc.unit].lon = loc.lon;
                items[loc.unit].azi = loc.azi;
                items[loc.unit].spd = loc.spd;
            }
            if (bat) {
                items[bat.unit].battery_time = bat.time;
                items[bat.unit].bat = bat.bat;
            }
        });
        return items;
    });
}


function op_get_all(req) {
    logger.debug("op_get_all()");
    const status = req.query.status;
    let filter;
    if (!status) {
        filter = {
            $or: [
                {
                    status: "available",
                },
                {
                    status: "in_use",
                    user: req.session.email,
                },
            ],
        };
    }
    else if (status == "in_use") {
        filter = {
            status: "in_use",
            user: req.session.email,
        };
    }
    else if (status == "available") {
        filter = {
            status: "available",
        };
    }
    else {
        throw utils.error(400, "Invalid status filter");
    }

    const pipe = [
        { $match: filter },
    ];
    return get_units(pipe);
}


function op_get(req) {
    const unit = req.params.unit;
    logger.debug("op_get('" + unit + "')");
    const filter = {
        unit,
        $or: [
            {
                status: { $ne: "in_use" },
            },
            {
                status: "in_use",
                user: req.session.email,
            },
        ],
    };
    const pipe = [
        { $match: filter },
    ];
    return get_units(pipe);
}


function op_take(req) {
    const unit = req.params.unit;
    const nonce = req.query.nonce;
    logger.debug("op_take('" + unit + "', " + nonce + ")");
    utils.require_client(req);
    if (!unit || !nonce) {
        throw utils.error(400, "Name and nonce are mandatory");
    }
    const filter = {
        unit,
        status: "available",
    };
    return db.unit_status().aggregate([
        { $match: filter, },
        { $limit: 1, },
    ]).next().then(u => {
        if (!u) {
            throw utils.error(403, "That unit is not available");
        }
        return db.unit_startup().aggregate([
            { $match: { unit, } },
            { $sort: { time: -1 } },
            { $limit: 1 },
        ]).next();
    }).then(u => {
        if (!u) {
            throw utils.error(500, "That unit is not online");
        }
        if (u.nonce != nonce) {
            throw utils.error(403, "Incorrect/outdated unit nonce");
        }
        return db.unit_status().updateOne(
            { unit, }, 
            {
                $set: {
                    status: "in_use",
                    user: req.session.email,
                },
            },
            {}
        );
    }).then(() => {
        let now = Math.round(new Date().getTime() / 1000);
        const record = {
            unit,
            time: now,
            status: "in_use",
            user: req.session.email,
        };
        events.emitter.emit("sendit", "unit_status", record);
        return null;
    });
}


function op_return(req) {
    const unit = req.params.unit;
    logger.debug("op_return('" + unit + "')");
    utils.require_client(req);
    if (!unit) {
        throw utils.error(400, "Name is mandatory");
    }
    const filter = {
        unit,
        status: "in_use",
        user: req.session.email,
    };
    return db.unit_status().aggregate([
        { $match: filter, },
        { $limit: 1, },
    ]).next().then(u => {
        if (!u) {
            throw utils.error(403, "You do not have that unit");
        }
        return db.unit_status().updateOne(
            { unit, }, 
            {
                $set: {
                    status: "available",    // FIXME: if bat < limit, then "charging"?
                    user: null,
                },
            },
            {}
        );
    }).then(() => {
        let now = Math.round(new Date().getTime() / 1000);
        const record = {
            unit,
            time: now,
            status: "available",
            user: null,
        };
        events.emitter.emit("sendit", "unit_status", record);
        return null;
    });
}


router.get("/",                 (req, res, next) => utils.mwrap(req, res, next, () => op_get_all(req)));
router.get("/:unit",            (req, res, next) => utils.mwrap(req, res, next, () => op_get(req)));
router.post("/take/:unit",      (req, res, next) => utils.mwrap(req, res, next, () => op_take(req)));
router.post("/return/:unit",    (req, res, next) => utils.mwrap(req, res, next, () => op_return(req)));

module.exports = router;

// vim: set sw=4 ts=4 et:
