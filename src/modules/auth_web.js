// config of the web app of the project
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
        console.log("Initializing web app");
        var webPlugin = require("firebase/app");
        context.commit("set_auth_plugin", webPlugin);

        require("firebase/auth");
        webPlugin.initializeApp(web_app_config);
        webPlugin.auth().useDeviceLanguage(); // or: webPlugin.auth().languageCode = "pt";
        return webPlugin.auth().setPersistence(webPlugin.auth.Auth.Persistence.LOCAL).then(() => {
            // wait for the plugin to initialize
            // yes, this is the official way: https://groups.google.com/g/firebase-talk/c/836OyVNd_Yg/m/fCeSAXkcBQAJ
            return new Promise((resolve, reject) => {
                var unsubscribe = webPlugin.auth().onAuthStateChanged(
                    user => {
                        unsubscribe();
                        if (user) {
                            resolve(user);
                        }
                        else {
                            reject({ code: "auth/invalid-user-token", message: "No persistent data to sign in with" });
                        }
                }, reject);
            });
        }).then(user =>
            user.getIdToken(true).then(idToken => {
                context.commit("logged_in", {
                    name: user.displayName,
                    email: user.email,
                    photo_url: user.photoURL,
                    id_token: idToken,
                    provider_id: user.providerId,
                    uid: user.uid,
                });
            })
        );
    },
    sign_out(context) {
        return context.rootState.auth_plugin.auth().signOut();
    },
    sign_in_with_google(context) {
        var provider = new context.rootState.auth_plugin.auth.GoogleAuthProvider();
        provider.setCustomParameters({
            access_type: "offline",
            prompt: "select_account",
        });
        return context.rootState.auth_plugin.auth().signInWithPopup(provider).then(userCredential => {
            return {
                name: userCredential.user.displayName,
                email: userCredential.user.email,
                photo_url: userCredential.user.photoURL,
                id_token: userCredential.credential.idToken,
                provider_id: userCredential.user.providerId,
                uid: userCredential.user.uid,
            };
        });
    },
};

// vim: set sw=4 ts=4 indk= et:
