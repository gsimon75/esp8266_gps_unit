const MongoClient = require("mongodb").MongoClient;
const logger = require("./logger").getLogger("database");
const utils = require("./utils");

/* Usage:
    return db.cursor_all(db.traces().find());
    return db.traces().findOne({unit: id});
*/

var client;
var db;
var traces;

async function open() {
    client = new MongoClient("mongodb://backend:zeihiwoofeim@localhost:27017/gps_tracker", {
        useNewUrlParser: true,
        useUnifiedTopology: true,
    });

    try {
        await client.connect();
        db = client.db("gps_tracker");
        traces = db.collection("traces");
    }
    catch (err) {
        logger.error(err.stack);
    }
}

function close() {
    const c = client;
    client = db = traces = undefined;
    return c.close();
}

function cursor_all(c) {
    let items = [];
    return c.forEach(t => { items.push(t); }).then(() => items);
}

module.exports = {
    traces: () => { return traces },
    open,
    close,
    cursor_all,
};

// vim: set ts=4 sw=4 et:
