//const SERVER_CLIENT_ID = "355671533390-afafi7ma53lgi78jj5f0cs9m12vefkh8.apps.googleusercontent.com"; // from google-services.json

//import {User} from 'firebase/app'
import {cfaSignIn, cfaSignOut } from 'capacitor-firebase-auth';

const web_app_config = {
    apiKey: "AIzaSyAa8vGbPDQDOtF4cjKqYa_b99hK7KSPqBI",
    authDomain: "scooterfleet.firebaseapp.com",
    databaseURL: "https://scooterfleet.firebaseio.com",
    projectId: "scooterfleet",
    storageBucket: "scooterfleet.appspot.com",
    messagingSenderId: "355671533390",
    appId: "1:355671533390:web:dcfc7550ed03c6cc4ba01f"
};

function user_has_logged_in(context) {
    const user = context.rootState.auth_plugin.auth().currentUser;
    return user.getIdToken(true).then(idToken => {
        context.commit("logged_in", {
            name: user.displayName || user.email,
            email: user.email,
            photo_url: user.photoURL,
            id_token: idToken,
            provider_id: user.providerId,
            uid: user.uid,
        });
    });
}

export default {
    init_auth_plugin(context) {
        console.log("Initializing mobile app");
        return new Promise(resolve => { 
            document.addEventListener("deviceready", resolve, false);
        }).then(() => {
            console.log("DeviceReady");
            var webPlugin = require("firebase/app");
            context.commit("set_auth_plugin", webPlugin);

            require("firebase/auth");
            webPlugin.initializeApp(web_app_config);
            webPlugin.auth().useDeviceLanguage(); // or: webPlugin.auth().languageCode = "pt";
        });
    },
    sign_out() {
        return cfaSignOut().subscribe();
    },
    sign_in_with_google(context) {
        return new Promise((resolve, reject) => {
            try {
                cfaSignIn('google.com').subscribe(user => {
                    console.log(JSON.stringify(user));
                    user.getIdToken(true).then(idToken => {
                        context.commit("logged_in", {
                            name: user.displayName || user.email,
                            email: user.email,
                            photo_url: user.photoURL,
                            id_token: idToken,
                            provider_id: user.providerId,
                            uid: user.uid,
                        });
                        resolve();
                    });
                });
            }
            catch (e) {
                reject(e);
            }
        });
    },
    sign_in_with_email_pwd(context, payload) {
        if (payload.signup) {
            return context.rootState.auth_plugin.auth().createUserWithEmailAndPassword(payload.email, payload.pwd)
                .then(() =>  context.rootState.auth_plugin.auth().currentUser.updateProfile({displayName: payload.displayName}))
                .then(() => user_has_logged_in(context));
        }
        else {
            return context.rootState.auth_plugin.auth().signInWithEmailAndPassword(payload.email, payload.pwd)
                .then(() => user_has_logged_in(context));
        }
    },
};

// vim: set sw=4 ts=4 indk= et:
