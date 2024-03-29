import Vue from "vue";
Vue.config.productionTip = false;

let overrideRipple = {
    directives: {
       ripple: {
            inserted: () => {}
       }
    }
}
Vue.mixin(overrideRipple);

import Vuetify from "vuetify/lib";
Vue.use(Vuetify);
import en from "vuetify/es5/locale/en";
import ar from "vuetify/es5/locale/ar";

import Vuex from "vuex";
Vue.use(Vuex);

import VuexPersist from "vuex-persist";

import VueRouter from "vue-router";
Vue.use(VueRouter);

import "@/assets/Roboto.css";
import "@/assets/fontawesome5.all.css";
import "leaflet/dist/leaflet.css";
import "leaflet.markercluster/dist/MarkerCluster.Default.css";

import App from "./App.vue";

import ExitGuard from "@/views/ExitGuard.vue";
import NotFound from "@/views/NotFound.vue";
import SignIn from "@/views/SignIn.vue";
import Home from "@/views/Home.vue";
import SiteMap from "@/views/SiteMap.vue";
import Account from "@/views/Account.vue";
import Users from "@/views/Users.vue";
import Test from "@/views/Test.vue";

import { Plugins } from "@capacitor/core";
const { SplashScreen, Geolocation } = Plugins;

import auth from "@/modules/auth";

const vuexLocalStorage = new VuexPersist({
    key: "vuex",
    storage: window.localStorage,
    modules: ["auth"],
});
       
const axios = require("axios");
const http = require("http");
const httpAgent = new http.Agent({ keepAlive: true });

const ax = axios.create({ httpAgent, withCredentials: true });

ax.interceptors.response.use(function (response) {
    return response;
}, function (error) {
    // [2021-02-14T12:43:20.627] [DEBUG] utils - error = {"status":403,"error":{"code":"auth/id-token-expired","message":"Firebase ID token has expired. Get a fresh ID token from your client app and try again (auth/id-token-expired). See https://firebase.google.com/docs/auth/admin/verify-id-tokens for details on how to retrieve an ID token."}}
    // axios intercepted response: {"message":"Request failed with status code 403","name":"Error","stack":"Error: Request failed with status code 403\n    at createError (webpack-internal:///./node_modules/axios/lib/core/createError.js:16:15)\n    at settle (webpack-internal:///./node_modules/axios/lib/core/settle.js:17:12)\n    at XMLHttpRequest.handleLoad (webpack-internal:///./node_modules/axios/lib/adapters/xhr.js:62:7)","config":{"url":"/v0/unit/trace/Simulated?before=1613306600&num=8","method":"get","headers":{"Accept":"application/json, text/plain, */*","Authorization":"Bearer eyJ..."},"transformRequest":[null],"transformResponse":[null],"timeout":0,"withCredentials":true,"xsrfCookieName":"XSRF-TOKEN","xsrfHeaderName":"X-XSRF-TOKEN","maxContentLength":-1,"maxBodyLength":-1,"httpAgent":{}}}
    console.log("axios intercepted, error.response: " + JSON.stringify(error.response));
    for (let x in error) {
        console.log("for ... in error found: " + x);
    }
    console.log("keys of error: " + JSON.stringify(Object.keys(error)));
    console.log("own props of error: " + JSON.stringify(Object.getOwnPropertyNames(error)));
    return Promise.reject(error);
});

const store = new Vuex.Store({
    state: {
        ax,
        is_started: false,
        sign_in_ready: false,
        app_bar_info: "...",
        auth_plugin: null,
        db: null,
    },
    getters: {
        app_type(state, getters, rootState) { // eslint-disable-line no-unused-vars
            return (typeof cordova === "undefined") ? "web" : "mobile";
        },
    },
    mutations: {
        mark_started(state, x) {
            state.is_started = x;
        },
        set_auth_plugin(state, x) {
            state.auth_plugin = x;
        },
        set_db(state, x) {
            state.db = x;
        },
        set_signin_state(state, value) {
            state.sign_in_ready = value;
        },
    },
    actions: {
        open_db(context) {
            window.sqlitePlugin.openDatabase({
                    name: "saga.db",
                    location: "default",
                    createFromLocation: 1,
                },
                db => {
                    console.log("db opened OK");
                    context.commit("set_db", db);
                },
                error => {
                    alert("db open failed: " + error);
                    context.commit("set_db", null);
                }
            );
        },
    },
    modules: {
        auth: auth,
    },
    plugins: [vuexLocalStorage.plugin],
});

/*
// Notifications - TODO: do we need this?
var checkNotificationPermission = function(requested) {
   store.state.mobilePlugin.hasPermission(
        function(hasPermission) {
            if (hasPermission) { // Granted
                console.log("Remote notifications permission granted");
                store.state.mobilePlugin.getToken(
                    function(token) {
                        console.log("Got FCM token: " + token); // FCM = Firebase Cloud Messaging
                    },
                    function(error) {
                        logError("Failed to get FCM token", error);
                    },
                );
            }
            else if (!requested) { // Request permission
                console.log("Requesting remote notifications permission");
                store.state.mobilePlugin.grantPermission(checkNotificationPermission.bind(this, true));
            }
            else { // Denied
                logError("Notifications won't be shown as permission is denied");
            }
        },
    );
};
*/

// the in-app router
const router = new VueRouter({
    mode: "history",
    base: process.env.BASE_URL,
    routes: [
        {
            path: "/exit_guard",
            name: "Exit Guard",
            component: ExitGuard
        },
        {
            path: "/signin",
            name: "Sign In",
            component: SignIn
        },
        {
            path: "/",
            name: "Home",
            component: Home
        },
        {
            path: "/site_map",
            name: "Site Map",
            component: SiteMap
        },
        {
            path: "/account",
            name: "Account",
            component: Account
        },
        {
            path: "/test",
            name: "Test",
            component: Test
        },
        {
            path: "/users",
            name: "Users",
            component: Users
        },
        {
            path: "*",
            name: "Not Found",
            component: NotFound
        }
    ],
});

// https://github.com/championswimmer/vuex-persist#how-to-know-when-async-store-has-been-replaced
const waitForStorageToBeReady = async (to, from, next) => {
    await store.restored;
    next();
}
router.beforeEach(waitForStorageToBeReady);

// main initialization starts here
store.dispatch("init_auth_plugin").catch(error => {
    console.log("Could not sign in with persisted data: " + error.message);
    store.commit("logged_out");
}).then(() => {
    if (typeof cordova !== "undefined") {
        // current location handler
        // https://capacitorjs.com/docs/apis/geolocation#geolocationposition
        const watch_location = function (loc, err) {
            if (err) {
                console.log("watch_location err=" + err);
            }
            else {
                console.log("loc.timestamp=" + loc.timestamp);
                console.log("loc.latitude=" + loc.latitude);
                console.log("loc.longitude=" + loc.longitude);
            }
        }

        var location_watcher = Geolocation.watchPosition({ enableHighAccuracy: true, },watch_location);
        console.log("location_watcher=" + location_watcher);
        // Geolocation.clearWatch({id: location_watcher});


        //store.dispatch("open_db"); // FIXME: not in this mock
            /*
            console.log("registering onTokenRefresh handler");
            mobilePlugin.onTokenRefresh(
                token => {
                    console.log("FCM Token refreshed: " + token) // FIXME: maybe store it somewhere?
                },
                error => {
                    logError("Failed to refresh FCM token", error);
                },
            );

            console.log("registering registerAuthStateChangeListener handler");
            mobilePlugin.registerAuthStateChangeListener(
                userSignedIn => {
                    console.log("Auth state changed: " + userSignedIn); // FIXME: maybe commit logged_in or logged_out?
                },
            );

            console.log("obtaining notification permission");
            checkNotificationPermission(false); // Check permission then get FCM token / push notifications
            */
    }

    // Initialization of the ui
    console.log("Launching UI");
    const vuetify = new Vuetify({
        global: {
            ripples: false,
        },
        icons: {
            iconfont: "fa",
        },
        lang: {
            locales: { en, ar },
            current: "en",
        },
        //rtl: true,
    });
    /* later in the app:
     * this.$vuetify.lang.current = "ar";
     * this.$vuetify.rtl = true;
     */

    new Vue({
        router,
        store,
        render: h => h(App),
        vuetify,
        mounted() {
            SplashScreen.hide();
        },
        created: function() {
            console.log("main::created app");
        },
    }).$mount("#app");
});

// vim: set sw=4 ts=4 indk= et:
