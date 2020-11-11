const SERVER_CLIENT_ID = "355671533390-afafi7ma53lgi78jj5f0cs9m12vefkh8.apps.googleusercontent.com"; // from google-services.json

import {User} from 'firebase/app'
import {cfaSignIn, cfaSignOut, mapUserToUserInfo} from 'capacitor-firebase-auth';

const web_app_config = {
    apiKey: "AIzaSyAa8vGbPDQDOtF4cjKqYa_b99hK7KSPqBI",
    authDomain: "scooterfleet.firebaseapp.com",
    databaseURL: "https://scooterfleet.firebaseio.com",
    projectId: "scooterfleet",
    storageBucket: "scooterfleet.appspot.com",
    messagingSenderId: "355671533390",
    appId: "1:355671533390:web:dcfc7550ed03c6cc4ba01f"
};

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
    sign_out(context) {
        return cfaSignOut().subscribe();
    },
    sign_in_with_google(context) {
        return new Promise((resolve, reject) => {
            try {
                cfaSignIn('google.com').subscribe(user => {
                    console.log(JSON.stringify(user));
                    user.getIdToken(true).then(idToken => {
                        resolve({
                            name: user.displayName,
                            email: user.email,
                            photo_url: user.photoURL,
                            id_token: idToken,
                            provider_id: user.providerId,
                            uid: user.uid,
                        });
                    });
                });
            }
            catch (e) {
                reject(e);
            }
        });
    },
};

// vim: set sw=4 ts=4 indk= et:
