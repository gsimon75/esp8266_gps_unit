const SERVER_CLIENT_ID = "486487305536-esjb4i5m1el65gd2p4hbjenu62mf3tnp.apps.googleusercontent.com"; // from google-services.json

export default {
    init_auth_plugin(context) {
        console.log("Initializing mobile app");
        return new Promise(resolve => { 
            document.addEventListener("deviceready", resolve, false);
        }).then(() => {
            console.log("DeviceReady");
            var mobilePlugin = window.FirebasePlugin;
            context.commit("set_auth_plugin", mobilePlugin);
            return new Promise((resolve, reject) => {
                mobilePlugin.isUserSignedIn(
                    isSignedIn => {
                        if (isSignedIn) {
                            mobilePlugin.getCurrentUser(resolve, reject);
                        }
                        else {
                            reject({ code: "auth/invalid-user-token", message: "No persistent data to sign in with" });
                        }
                    },
                    reject
                );
            });
        }).then(user => {
            context.commit("logged_in", {
                name: user.name,
                email: user.email,
                photo_url: user.photoUrl,
                id_token: user.idToken,
                provider_id: user.providerId,
                uid: user.uid,
            });
        });
    },
    sign_out(context) {
        return new Promise((resolve, reject) => context.rootState.auth_plugin.signOutUser(resolve, reject));
    },
    sign_in_with_google(context) {
        return new Promise((resolve, reject) => context.rootState.auth_plugin.authenticateUserWithGoogle(SERVER_CLIENT_ID, resolve, reject)
        ).then(credential => new Promise((resolve, reject) => context.rootState.auth_plugin.signInWithCredential(credential, resolve, reject))
        ).then(() => new Promise((resolve, reject) => context.rootState.auth_plugin.getCurrentUser(resolve, reject))
        ).then(user => {
            return {
                name: user.name,
                email: user.email,
                photo_url: user.photoUrl,
                id_token: user.idToken,
                provider_id: user.providerId,
                uid: user.uid,
            };
        });
    },
};

// vim: set sw=4 ts=4 indk= et:
