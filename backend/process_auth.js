const logger = require("./logger").getLogger("auth");
const db = require("./database");
const utils = require("./utils");

/*
 * https://firebase.google.com/docs/admin/setup
 * https://firebase.google.com/docs/auth/admin/verify-id-tokens
 */
const fba = require("firebase-admin");
const serviceAccount = require("./fitnesstest-2d3ba-firebase-adminsdk-95sfb-e80f43f52a.json");
fba.initializeApp({
  credential: fba.credential.cert(serviceAccount),
  databaseURL: "https://fitnesstest-2d3ba.firebaseio.com"
});

const anonymous = { id: -1, is_trainer: false, is_admin: false };

function check_user(query_promise) {
    return query_promise.catch(err => {
        logger.error("Saga4 auth failed: " + err);
        throw utils.error(500, err);
    }).then(results => {
        if (results.length == 0) {
            //throw utils.error(403, "Not a registered user");
            return anonymous;
        }
        if (results[0].status == 3) { // deleted account
            //throw utils.error(403, "Not a registered user");
            return anonymous;
        }
        if (results[0].status == 2) { // blocked account
            throw utils.error(403, "User is blocked");
        }
        if (results[0].status == 1) { // suspended account
            throw utils.error(403, "Account is suspended");
        }
        return results[0];
    });
}

function process_auth(req) {
    const bearer_token = req.get("Authorization");

    if (!bearer_token) {
        if (!req.session.uid) {
            req.session.uid = -1;
            req.session.is_trainer = false;
            req.session.is_admin = false;
        }
        if (!req.session.euid) {
            req.session.euid = req.session.uid;
        }
        return Promise.resolve();
    }
    
    if (!bearer_token.startsWith("Bearer ")) {
        return Promise.reject(utils.error(400, "ID token must start with 'Bearer '"));
    }
    
    return fba.auth().verifyIdToken(bearer_token.substring(7)).catch(err => {
        throw utils.error(403, err);
    }).then(decodedToken => {
        // https://firebase.google.com/docs/reference/admin/node/admin.auth.DecodedIdToken
        //logger.debug("decodedToken=" + JSON.stringify(decodedToken));
        // decodedToken={"name":"Gabor Simon","picture":"https://lh4.googleusercontent.com/.../photo.jpg","iss":"https://securetoken.google.com/fitnesstest-2d3ba","aud":"fitnesstest-2d3ba","auth_time":1603797293,"user_id":"uk...X2","sub":"uk...X2","iat":1603885448,"exp":1603889048,"email":"gabor.simon@saga4.com","email_verified":true,"firebase":{"identities":{"google.com":["10...7"],"email":["gabor.simon@saga4.com"]},"sign_in_provider":"google.com"},"uid":"uk...X2"}
        req.session.ext_uid = decodedToken.uid;
        req.session.ext_provider = decodedToken.firebase.sign_in_provider;
        req.session.ext_email = decodedToken.email;
        req.session.ext_name = decodedToken.name;
        req.session.cookie.expires = new Date((decodedToken.exp - 5) * 1000); // let the cookie expire 5 seconds earlier than the token
        return check_user(db.query("SELECT id, status, is_trainer, is_admin FROM Client_t WHERE ext_uid=?", [ decodedToken.uid ]));
    }).then(user => {
        req.session.uid = 0 | user.id;
        req.session.is_trainer = !!user.is_trainer;
        req.session.is_admin = !!user.is_admin;
        const sudo_target = 0 | req.get("X-Sudo");
        if (!sudo_target) {
            req.session.euid = req.session.uid;
            return Promise.resolve();
        }
        else if (!user.is_trainer && !user.is_admin) {
            throw utils.error(403, "May not access the resource of someone else");
        }
        else {
            return check_user(db.query("SELECT id, status, is_trainer, is_admin FROM Client_t WHERE id=?", [ sudo_target ])).then(target_user => {
                if (target_user.is_trainer || target_user.is_admin) {
                    throw utils.error(403, "May not access the resource of someone else");
                }
                req.session.euid = sudo_target;
            });
        }
    });
}

module.exports = process_auth;

// vim: set ts=4 sw=4 et:
