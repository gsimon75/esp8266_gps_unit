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

    if (req.session.email) {
        // already have a session, don't care about the "Authorization" header
        return Promise.resolve();
    }

    const bearer_token = req.get("Authorization");

    if (!bearer_token) {
        if (!req.session.email) {
            req.session.email = null;
            req.session.is_admin = false;
            req.session.is_technician = false;
        }
        return Promise.resolve();
    }
    
    if (!bearer_token.startsWith("Bearer ")) {
        logger.error("Invalid bearer token '" + bearer_token + "'");
        return Promise.reject(utils.error(400, "ID token must start with 'Bearer '"));
    }
    
    return fba.auth().verifyIdToken(bearer_token.substring(7)).catch(err => {
        throw utils.error(403, err);
    }).then(decodedToken => {
        // https://firebase.google.com/docs/reference/admin/node/admin.auth.DecodedIdToken
        //logger.debug("decodedToken=" + JSON.stringify(decodedToken));
        // decodedToken={"name":"Gabor Simon","picture":"https://lh4.googleusercontent.com/.../photo.jpg","iss":"https://securetoken.google.com/fitnesstest-2d3ba","aud":"fitnesstest-2d3ba","auth_time":1603797293,"user_id":"uk...X2","sub":"uk...X2","iat":1603885448,"exp":1603889048,"email":"gabor.simon@saga4.com","email_verified":true,"firebase":{"identities":{"google.com":["10...7"],"email":["gabor.simon@saga4.com"]},"sign_in_provider":"google.com"},"uid":"uk...X2"}
        return db.users().findOne({email: decodedToken.email}).then(result => {
            req.session.is_admin = false;
            req.session.is_technician = false;
            req.session.provider = decodedToken.firebase.sign_in_provider;
            req.session.email = decodedToken.email;
            req.session.name = decodedToken.name;
            req.session.cookie.expires = new Date((decodedToken.exp - 5) * 1000); // let the cookie expire 5 seconds earlier than the token
            if (result) {
                // known user, retrieve his admin/technician status
                req.session.is_admin = result.is_admin;
                req.session.is_technician = result.is_technician;
            }
            else {
                // new user, auto-subcribe as plain user
                return db.users.insertOne({email: decodedToken.email, is_admin: false, is_technician: false })
            }
        });
    });
}

module.exports = process_auth;

// vim: set ts=4 sw=4 et:
