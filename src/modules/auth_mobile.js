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
        return cfaSignIn('google.com').subscribe(user => {
            console.log(JSON.stringify(user));
            /*  with 
                    return cfaSignIn('google.com').pipe(mapUserToUserInfo(),).subscribe(userInfo => { ...
                the result is
                    {
                        "uid":"ygvq2lUBcEfELua5yBah6q5m38m2",
                        "providerId":"firebase",
                        "displayName":"Gabor Simon",
                        "photoURL":"https://lh6.googleusercontent.com/-VTAQRrr_qTQ/AAAAAAAAAAI/AAAAAAAAAAA/AMZuucljlOq9zOypoV3XFHOZjKPnGUbc6g/s96-c/photo.jpg",
                        "phoneNumber":null,
                        "email":"gabor.simon75@gmail.com"
                    }
            */

            /* without mapUserToUserInfo:
             *
                {
                    "uid":"ygvq2lUBcEfELua5yBah6q5m38m2",
                    "displayName":"Gabor Simon",
                    "photoURL":"https://lh6.googleusercontent.com/-VTAQRrr_qTQ/AAAAAAAAAAI/AAAAAAAAAAA/AMZuucljlOq9zOypoV3XFHOZjKPnGUbc6g/s96-c/photo.jpg",
                    "email":"gabor.simon75@gmail.com",
                    "emailVerified":true,
                    "phoneNumber":null,
                    "isAnonymous":false,
                    "tenantId":null,
                    "providerData":[
                        {
                            "uid":"106223510888703572487",
                            "displayName":"Gabor Simon",
                            "photoURL":"https://lh6.googleusercontent.com/-VTAQRrr_qTQ/AAAAAAAAAAI/AAAAAAAAAAA/AMZuucljlOq9zOypoV3XFHOZjKPnGUbc6g/s96-c/photo.jpg",
                            "email":"gabor.simon75@gmail.com",
                            "phoneNumber":null,
                            "providerId":"google.com"
                        }
                    ],
                    "apiKey":"AIzaSyAa8vGbPDQDOtF4cjKqYa_b99hK7KSPqBI",
                    "appName":"[DEFAULT]",
                    "authDomain":"scooterfleet.firebaseapp.com",
                    "stsTokenManager":{
                        "apiKey":"AIzaSyAa8vGbPDQDOtF4cjKqYa_b99hK7KSPqBI",
                        "refreshToken":"AG8BC...iwFXw",
                        "accessToken":"eyJhbGc...ZWr-RWzn_hw",
                        "expirationTime":1605119363000
                    },
                    "redirectEventId":null,
                    "lastLoginAt":"1605115763653",
                    "createdAt":"1605007223334",
                    "multiFactor":{
                        "enrolledFactors":[]
                    }
                }
             */
            user.getIdToken(true).then(idToken => {
                context.commit("logged_in", {
                    name: user.displayName,
                    email: user.email,
                    photo_url: user.photoURL,
                    id_token: idToken,
                    provider_id: user.providerId,
                    uid: user.uid,
                });
            });
        });
    },
};

// vim: set sw=4 ts=4 indk= et:
