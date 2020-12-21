const logger = require("./logger").getLogger("utils");

class HTTPError extends Error {
    constructor (status, message, error) {
        super(message);
        this.status = status;
        this.error = error;
    }
}

function error(status, message) {
    if (message instanceof HTTPError) {
        message.status = status;
        return message;
    }
    if (message instanceof Error) {
        return new HTTPError(status, message.message, message);
    }
    return new HTTPError(status, message);
}

function result(status, content, resource_id) {
    return { status, content, resource_id };
}

function promisify(f, ...args) {
    return new Promise((resolve, reject) => {
        try {
            resolve(f(...args));
        } catch(err) {
            reject(err);
        }
    });
}

function mwrap(req, res, next, handler) {
    var promise;
    if (typeof(handler) === "function") {
        promise = promisify(handler);
    }
    else if (handler instanceof Promise) {
        promise = handler;
    }
    else {
        promise = Promise.resolve(handler);
    }

    promise.then(response => {
        logger.debug("response = " + JSON.stringify(response));
        if (response === undefined) {
            next();
        }
        else if (response == null) {
            res.status(204);
            res.end();
        }
        else {
            res.status(200);
            res.json(response);
        }
    }).catch(err => {
        if (err instanceof HTTPError) {
            logger.debug("error = " + JSON.stringify(err));
            res.status(err.status || 500);
            if (err.message) {
                res.send(err.message);
            }
            else {
                res.end();
            }
        }
        else {
            next(err);
        }
    });
}

function require_client(req, client_id) {
    if (req.session.euid <= 0) {
        throw error(401, "Authentication needed");
    }
    if (client_id && (req.session.euid != client_id)) {
        throw error(403, "Not owner of data");
    }
}

function require_trainer(req) {
    require_client(req);
    if (!req.session.is_trainer && !req.session.is_admin) {
        throw error(403, "Must be trainer");
    }
}

function require_admin(req) {
    require_client(req);
    if (!req.session.is_admin) {
        throw error(403, "Must be admin");
    }
}

function require_body(req, attrs) {
    for (let attr of attrs) {
        if (req.body[attr] === undefined) {
            throw utils.error(400, "Missing " + attr);
        }
    }
}

function dump_request(req) {
    // https://expressjs.com/en/guide/routing.html
    // req is-a IncomingMessage
    //
    // req.headers
    // req.rawHeaders
    // req.url
    // req.method
    // req.statusCode
    // req.statusMessage
    // req.query = {}
    // req.body = {}

    logger.debug("Request; hostname='" + req.hostname + "', ip='" + req.ip + "', method='" + req.method + "', url='" + req.url + "'");
    for (let name in req.headers) {
        logger.debug("Request header; name='" + name + "', value='" + req.headers[name] + "'");
    }
    for (let name in req.params) {
        logger.debug("Request param; name='" + name + "', value='" + req.params[name] + "'");
    }
    for (let name in req.query) {
        logger.debug("Request query; name='" + name + "', value='" + req.query[name] + "'");
    }
    for (let name in req.body) {
        logger.debug("Request body; name='" + name + "', value='" + req.body[name] + "'");
    }
    for (let name in req.cookies) {
        logger.debug("Request cookie; name='" + name + "', value='" + req.cookies[name] + "'");
    }
}


module.exports = {
    HTTPError,
    error,
    result,
    promisify,
    mwrap,
    require_client,
    require_trainer,
    require_admin,
    require_body,
    dump_request,
}

// vim: set ts=4 sw=4 et:
