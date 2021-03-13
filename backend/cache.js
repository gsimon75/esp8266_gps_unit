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
        unit_cache[u.unit] = {
            type: "unit",
            unit: u.unit,
        };
    }
    let changed = 
        (unit_cache[u.unit].location_time != u.time) ||
        (unit_cache[u.unit].lat != u.lat) ||
        (unit_cache[u.unit].lon != u.lon) ||
        (unit_cache[u.unit].azi != u.azi) ||
        (unit_cache[u.unit].spd != u.spd);

    unit_cache[u.unit].location_time = u.time;
    unit_cache[u.unit].lat = u.lat;
    unit_cache[u.unit].lon = u.lon;
    unit_cache[u.unit].azi = u.azi;
    unit_cache[u.unit].spd = u.spd;

    return changed;
}

function upsert_status(u) {
    if (!(u.unit in unit_cache)) {
        unit_cache[u.unit] = {};
    }
    let changed =
        (unit_cache[u.unit].status_time != u.time) ||
        (unit_cache[u.unit].status != u.status) ||
        (unit_cache[u.unit].user != u.user);

    unit_cache[u.unit].status_time = u.time;
    unit_cache[u.unit].status = u.status;
    unit_cache[u.unit].user = u.user;

    return changed;
}

function upsert_battery(u) {
    if (!(u.unit in unit_cache)) {
        unit_cache[u.unit] = {};
    }
    let changed =
        (unit_cache[u.unit].battery_time != u.time) ||
        (unit_cache[u.unit].bat != u.bat);

    unit_cache[u.unit].battery_time = u.time;
    unit_cache[u.unit].bat = u.bat;

    return changed;
}

let handler = (etype, data) => {
    let send_composite = false;

    if (etype == null) {
        emitter.removeListener("sendit", handler);
    }
    else if (etype == "unit_location") {
        send_composite = upsert_location(data);
    }
    else if (etype == "unit_status") {
        send_composite = upsert_status(data);
    }
    else if (etype == "unit_battery") {
        send_composite = upsert_battery(data);
    }

    if (send_composite) {
        unit_cache[data.unit].mark_for_sending = true;
        setImmediate(() => {
            if (unit_cache[data.unit].mark_for_sending) {
                delete unit_cache[data.unit].mark_for_sending;
                //events.emitter.emit("sendit", "unit", unit_cache[data.unit]);
                events.fetch_emitter.emit("sendit", unit_cache[data.unit]);
            }
        });
    }
};

function start() {
    db.unit_location().aggregate(pipe_last_records).forEach(upsert_location);
    db.unit_status().aggregate(pipe_last_records).forEach(upsert_status);
    db.unit_battery().aggregate(pipe_last_records).forEach(upsert_battery);
    events.emitter.addListener("sendit", handler);
}

module.exports = {
    start,
    unit: unit_cache,
}

// vim: set sw=4 ts=4 et:
