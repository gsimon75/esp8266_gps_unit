const logger = require("./logger").getLogger("auth");
const db = require("./database");
const utils = require("./utils");

/*
 * https://firebase.google.com/docs/admin/setup
 * https://firebase.google.com/docs/auth/admin/verify-id-tokens
 */
const fba = require("firebase-admin");
const serviceAccount = require("./scooterfleet-firebase-adminsdk-9sbip-0ef9e8fa28.json");
fba.initializeApp({
  credential: fba.credential.cert(serviceAccount),
  databaseURL: "https://scooterfleet.firebaseio.com"
});

const anonymous = { id: -1, is_admin: false, is_technician: false };

function process_auth(req) {
    {
        utils.dump_request(req);
        var h;
        h = req.get("Authorization");
        if (h) {
            logger.debug("req.Authorization = '" + h + "'");
        }
        h = req.get("Cookie");
        if (h) {
            logger.debug("req.Cookie = '" + h + "'");
        }
        if (req.session) {
            for (let k in req.session) {
                logger.debug("session." + k + " = '" + JSON.stringify(req.session[k]) + "'");
            }
        }
    }

    if (req.method == "OPTIONS") {
        return Promise.resolve();
    }

    // For development ONLY
    const x_real_ip = req.get("X-Real-IP");
    if (x_real_ip && (x_real_ip == "127.0.0.1")) {
        req.session.email = req.get("X-Test-Email") || "gabor.simon75@gmail.com";
        req.session.name = req.get("X-Test-Name") || "Test User";
        req.session.is_admin = JSON.parse(req.get("X-Test-Is-Admin") || "true");
        req.session.is_technician = JSON.parse(req.get("X-Test-Is-Technician") || "true");
        req.session.provider = "test";
        req.session.cookie.expires = null;
        return Promise.resolve();
    }

    if (req.session && req.session.email) {
        // already have a session, don't care about the "Authorization" header
        logger.debug("Session already exists");
        return Promise.resolve();
    }

    const bearer_token = req.get("Authorization");

    if (!bearer_token) {
        logger.debug("No session, no token, going anonymous");
        delete req.session.email;
        delete req.session.name;
        delete req.session.provider;
        delete req.session.is_admin;
        delete req.session.is_technician;
        return Promise.resolve();
    }
    
    if (!bearer_token.startsWith("Bearer ")) {
        logger.error("Invalid bearer token '" + bearer_token + "'");
        return Promise.reject(utils.error(401, "ID token must start with 'Bearer '"));
    }
    
    logger.debug("No session but token present, doing sign-in");
    return fba.auth().verifyIdToken(bearer_token.substring(7)).catch(err => {
        throw utils.error(403, err);
    }).then(decodedToken => {
        // https://firebase.google.com/docs/reference/admin/node/admin.auth.DecodedIdToken
        //logger.debug("decodedToken=" + JSON.stringify(decodedToken));
        // decodedToken={"name":"Gabor Simon","picture":"https://lh4.googleusercontent.com/.../photo.jpg","iss":"https://securetoken.google.com/fitnesstest-...","aud":"fitnesstest-...","auth_time":1603797293,"user_id":"uk...X2","sub":"uk...X2","iat":1603885448,"exp":1603889048,"email":"gabor.simon@saga4.com","email_verified":true,"firebase":{"identities":{"google.com":["10...7"],"email":["gabor.simon@saga4.com"]},"sign_in_provider":"google.com"},"uid":"uk...X2"}
        return db.users().findOne({email: decodedToken.email}).then(result => {
            // FIXME: handle banned/disabled accounts
            req.session.provider = decodedToken.firebase.sign_in_provider;
            req.session.email = decodedToken.email;
            req.session.name = decodedToken.name;
            //req.session.cookie.expires = new Date((decodedToken.exp - 5) * 1000); // let the cookie expire 5 seconds earlier than the token
            req.session.cookie.maxAge = 2 * 60 * 1000; // FIXME: 2 minute validity for testing
            logger.debug("Sign-in successful, req.session=" + JSON.stringify(req.session));
            if (result) {
                // known user, retrieve his admin/technician status
                req.session.is_admin = result.is_admin;
                req.session.is_technician = result.is_technician;
            }
            else {
                // new user, auto-subcribe as plain user
                req.session.is_admin = false;
                req.session.is_technician = false;
                return db.users.insertOne({email: req.session.email, is_admin: req.session.is_admin, is_technician: req.session.is_technician }).then(() => { return; }); // return undefined when done
            }
        });
    });
}

module.exports = process_auth;

// vim: set ts=4 sw=4 et:
