// config of the web app of the project
const web_app_config = {
    apiKey: "AIzaSyApfMOTsdhl_otrcQKgo6V8mYAoCFRt7CE",
    authDomain: "fitnesstest-2d3ba.firebaseapp.com",
    databaseURL: "https://fitnesstest-2d3ba.firebaseio.com",
    projectId: "fitnesstest-2d3ba",
    storageBucket: "fitnesstest-2d3ba.appspot.com",
    messagingSenderId: "486487305536",
    appId: "1:486487305536:web:4c04879d7fdd556e09dea5"
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
