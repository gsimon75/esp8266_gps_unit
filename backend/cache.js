const logger = require("./logger").getLogger("cache");
const db = require("./database");
const events = require("./events");

let unit_cache = {}

const pipe_last_records = [
    { $sort: { unit: 1, time: -1 } },
    { $group: {_id: "$unit", lastrec: { $first: "$$CURRENT" } } },
    { $replaceWith: "$lastrec" },
];


function upsert_location(u) {
    if (!(u.unit in unit_cache)) {
        unit_cache[u.unit] = {};
    }
    unit_cache[u.unit].location_time = u.time;
    unit_cache[u.unit].lat = u.lat;
    unit_cache[u.unit].lon = u.lon;
    unit_cache[u.unit].azi = u.azi;
    unit_cache[u.unit].spd = u.spd;
}

function upsert_status(u) {
    if (!(u.unit in unit_cache)) {
        unit_cache[u.unit] = {};
    }
    unit_cache[u.unit].status_time = u.time;
    unit_cache[u.unit].status = u.status;
    unit_cache[u.unit].user = u.user;
}

function upsert_battery(u) {
    if (!(u.unit in unit_cache)) {
        unit_cache[u.unit] = {};
    }
    unit_cache[u.unit].battery_time = u.time;
    unit_cache[u.unit].bat = u.bat;
}

let handler = (etype, data) => {
    if (etype == null) {
        emitter.removeListener("sendit", handler);
    }
    else if (etype == "unit_location") {
        upsert_location(data);
    }
    else if (etype == "unit_status") {
        upsert_status(data);
    }
    else if (etype == "unit_battery") {
        upsert_battery(data);
    }
};

function start() {
    db.unit_location().aggregate(pipe_last_records).forEach(upsert_location);
    db.unit_status().aggregate(pipe_last_records).forEach(upsert_status);
    db.unit_battery().aggregate(pipe_last_records).forEach(upsert_battery);
    events.admin_event_emitter.addListener("sendit", handler);
}

module.exports = {
    start,
    unit: unit_cache,
}

// vim: set sw=4 ts=4 et:
