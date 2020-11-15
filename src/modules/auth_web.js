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
        }).then(() => user_has_logged_in(context));
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
        return context.rootState.auth_plugin.auth().signInWithPopup(provider).then(() => user_has_logged_in(context));
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
