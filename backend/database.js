const MongoClient = require("mongodb").MongoClient;
const logger = require("./logger").getLogger("database");
const utils = require("./utils");

/* Usage:
    return db.cursor_all(db.unit_location().find());
    return db.unit_location().findOne({unit: id});
*/

var client, db;
var stations, users, agps; // state-like collections
var unit_location, unit_battery, unit_startup, unit_status; // log-like collections

async function open() {
    client = new MongoClient("mongodb://backend:zeihiwoofeim@localhost:27017/gps_tracker", {
        useNewUrlParser: true,
        useUnifiedTopology: true,
    });

    try {
        await client.connect();
        db = client.db("gps_tracker");
        stations = db.collection("stations");
        users = db.collection("users");
        agps = db.collection("agps");
        unit_location = db.collection("unit_location");
        unit_battery = db.collection("unit_battery");
        unit_startup = db.collection("unit_startup");
        unit_status = db.collection("unit_status");
    }
    catch (err) {
        logger.error(err.stack);
    }
}

function close() {
    const c = client;
    client = db =
        stations = users = agps =
        unit_location = unit_battery = unit_startup = unit_status = undefined;
    return c.close();
}

function cursor_all(c) {
    let items = [];
    return c.forEach(t => { items.push(t); }).then(() => items);
}

module.exports = {
    stations: () => { return stations },
    users: () => { return users },
    agps: () => { return agps },
    unit_location: () => { return unit_location },
    unit_battery: () => { return unit_battery },
    unit_startup: () => { return unit_startup },
    unit_status: () => { return unit_status },
    open,
    close,
    cursor_all,
};

// vim: set ts=4 sw=4 et:
