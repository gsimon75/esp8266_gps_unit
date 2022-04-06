import {cfaSignIn, cfaSignOut } from "capacitor-firebase-auth";
import { EventBus } from "@/modules/event-bus";
//const WebSocket = require("ws");

const web_app_config = {
    apiKey: "AI...BI",
    authDomain: "scooterfleet.firebaseapp.com",
    databaseURL: "https://scooterfleet.firebaseio.com",
    projectId: "scooterfleet",
    storageBucket: "scooterfleet.appspot.com",
    messagingSenderId: "3...0",
    appId: "1:3...0:web:dcfc7550ed03c6cc4ba01f"
};

function user_has_logged_in(context) {
    console.log("user_has_logged_in()");
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
        return context.dispatch("sign_in_to_backend", idToken);
    });
}

export default {
    state: {
        name: "Anonymous",
        email: "",
        photo_url: "",
        id_token: "",
        provider_id: "",
        uid: "",
    },
    getters: {
        auth(state, getters, rootState) { // eslint-disable-line no-unused-vars
            return state;
        },
        is_logged_in(state, getters, rootState) { // eslint-disable-line no-unused-vars
            return state.id_token != "";
        },
    },
    mutations: {
        logged_in(state, x) {
            console.log("Signed in as " + x.name);
            state.name = x.name;
            state.email = x.email;
            state.photo_url = x.photo_url;
            state.id_token = x.id_token;
            state.provider_id = x.provider_id;
            state.uid = x.uid;
        },
        logged_out(state) {
            console.log("Signed out");
            state.name = "Anonymous";
            state.email = "";
            state.photo_url = "";
            state.id_token = "";
            state.provider_id = "";
            state.uid = "";
        },
    },
    actions: {
        init_auth_plugin(context) {
            var prepare;
            
            if (typeof cordova !== "undefined") {
                console.log("Initializing mobile app");
                prepare = new Promise(resolve => { 
                    document.addEventListener("deviceready", resolve, false);
                });
            }
            else {
                console.log("Initializing web app");
                prepare = Promise.resolve();
            }

            prepare.then(() => {
                var webPlugin = require("firebase/app");
                console.log("webPlugin=" + webPlugin);
                context.commit("set_auth_plugin", webPlugin);

                webPlugin.initializeApp(web_app_config);

                require("firebase/auth");
                webPlugin.auth().useDeviceLanguage(); // or: webPlugin.auth().languageCode = "pt";
                if (typeof cordova === "undefined") {
                    // platform: web
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
                }
            });
        },
        sign_out(context) {
            console.log("sign_out()");
            context.commit("set_signin_state", false);
            return context.rootState.ax.get("v0/logout").then(() => {
                console.log("Signed out from backend");
                EventBus.$emit("signed-out");
                if (typeof cordova !== "undefined") {
                    // platform: mobile
                    return cfaSignOut().subscribe();
                }
                else {
                    // platform: web
                    return context.rootState.auth_plugin.auth().signOut();
                }
            });
        },
        sign_in_to_backend(context, id_token) {
            console.log("Signing in to backend");
            //context.rootState.ax.defaults.headers.common["Authorization"] = "Bearer " + id_token;
            context.rootState.ax.get("v0/whoami", { headers: { Authorization: "Bearer " + id_token}, }).then(result => {
                console.log("Signed in to backend, whoami results: " + JSON.stringify(result.data));

                // ==============================================================================
                // https://javascript.info/server-sent-events
                // https://developer.mozilla.org/en-US/docs/Web/API/EventSource
                var source = new EventSource("/v0/event");
                ["station", "unit"].forEach(t => {
                    let listener = event => {
                        console.log("sse incoming " + t + ": " + event.data);
                        EventBus.$emit(t, JSON.parse(event.data));
                    };
                    source.addEventListener(t, listener, false);
                });
                source.addEventListener("end", event => {
                    console.log("sse wants to close the connection: " + event.data);
                    source.close();
                }, false);
                // ==============================================================================

                EventBus.$emit("signed-in", { yadda: 42 });
                setImmediate(() => context.commit("set_signin_state", true));
            }).catch(err => {
                console.log("Failed to sign in to backend: " + err);
            });
        },
        sign_in_with_google(context) {
            if (typeof cordova !== "undefined") {
                // platform: mobile
                return new Promise((resolve, reject) => {
                    try {
                        console.log("calling cfaSignIn()");
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
                                context.dispatch("sign_in_to_backend", idToken).then(() => {
                                    resolve();
                                });
                            });
                        });
                    }
                    catch (e) {
                        reject(e);
                    }
                });
            }
            else {
                // platform: web
                console.log("calling context.rootState.auth_plugin.auth.GoogleAuthProvider()");
                var provider = new context.rootState.auth_plugin.auth.GoogleAuthProvider();
                provider.setCustomParameters({
                    access_type: "offline",
                    prompt: "select_account",
                });
                return context.rootState.auth_plugin.auth().signInWithPopup(provider).then(() => user_has_logged_in(context));
            }
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
    },
};

// vim: set sw=4 ts=4 indk= et:
