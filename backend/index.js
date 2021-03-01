const logger = require("./logger").getLogger("index");
// Redirect exceptions to the log
process.on("uncaughtException", (err) => {
    logger.fatal(err);
    //process.exit(1);
});

const utils = require("./utils");
const db = require("./database");
const cache = require("./cache");

// Set up the Express engine
const express = require("express");
const app = express();

// Set up session store
const session = require("express-session");
const MongoDBStore = require("connect-mongodb-session")(session);
const sessionStore = new MongoDBStore({
    uri: "mongodb://backend:...@localhost:27017/gps_tracker",
    collection: "sessions",
});

if (app.get("env") === "production") {
    app.set("trust proxy", 1); // trust first proxy
}
app.use(session({
    name: "gps-tracker",
    key: "s4session",
    secret: ["yaddaboo"],
    store: sessionStore,
    resave: false,
    saveUninitialized: false,
    proxy: true,
    cookie: {
        secure: true,
        sameSite: "none",
    },
}));

const bodyParser = require("body-parser");
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({extended: false}));

app.disable("etag"); // Don't want "304 Not Modified" responses when the underlying data has actually changed

const allowed_origins = [
    "https://client.wodeewa.com",
    "https://admin.wodeewa.com",
    "capacitor://localhost",
    "http://localhost",
];

app.use((req, res, next) => {
    // Do the CORS rain dance
    if (!req.headers.origin) {
        next(utils.error(403, "CORS without Origin"));
    }
    else if (allowed_origins.includes(req.headers.origin)) {
        utils.add_cors_response_headers(res, req.headers.origin);
        // Don't want "304 Not Modified" responses when the underlying data has actually changed
        res.header("Last-Modified", (new Date()).toUTCString());
        next();
    }
    else {
        next(utils.error(403, "CORS denied"));
    }
});

// Check authentication
const process_auth = require("./process_auth");
app.use((req, res, next) => utils.mwrap(req, res, next, process_auth(req)));

// Register routers
app.use("/client/v0", require("./rest.client"));
app.use("/backend/v0", require("./rest.backend"));
app.use("/admin/v0", require("./rest.admin"));

// Catch 404 and forward to error handler
app.use((req, res, next) => {
    next(utils.error(404, "Not Found: " + req.originalUrl));
});

// Error handler
app.use((err, req, res, next) => {
    if (err instanceof utils.HTTPError) {
        // don't log stack trace and whatnot for operational http errors
        logger.error(err.status + " " + err.message);
    }
    else {
        logger.error(err);
    }
    res.status(err.status || 500);
    if (err.message) {
        res.send(err.message);
    }
    else {
        res.end();
    }
});

// Get port from environment and store in Express.
const port = 8080;
logger.info("Listen address; port='" + port + "', protocol='http'");
app.set("port", port);

// Create an http server within the Express app
const http = require("http");
const server = http.createServer(app);

// Die quickly and noisily if the server can't start up
server.on("error", (error) => {
    if (error.syscall !== "listen") {
        throw error;
    }
    switch (error.code) {
        case "EACCES":
            logger.fatal("Address requires elevated privileges;");
            process.exit(1);
            break;

        case "EADDRINUSE":
            logger.fatal("Address is already in use;");
            process.exit(1);
            break;

        default:
            throw error;
    }
});

// Exit normally if the server has closed
server.on("close", () => {
    logger.info("Server closed;");
});

// Finish the startup when the server has actually started listening
server.on("listening", () => {
    var addr = server.address();
    logger.info("Server is listening;");

    // Register handlers for external kill signals
    function cleanup() {
        logger.info("Closing server;");
        server.close();
        process.exit(0);
    }
    process.on("SIGINT", () => { cleanup(); });
    process.on("SIGTERM", () => { cleanup(); });
});


// hijack the server close *call*, so we can close the DB before returning from it
// (the "close" event is asynchronous, it can't delay the closing process)
server.orig_close = server.close;
server.close = function(callback) {
    var session_close = new Promise((resolve, reject) => {
        try {
            sessionStore.close(resolve);
        } catch (err) {
            reject(err);
        }
    });
    
    return session_close
        .then(() => db.close())
        .then(() => this.orig_close(callback));
};

db.open().then(() => {
    cache.start();
    server.listen(port);
});


module.exports = server;

// vim: set ts=4 sw=4 et:
