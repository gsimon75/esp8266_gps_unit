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

import NotFound from "@/views/NotFound.vue";
import SignIn from "@/views/SignIn.vue";
import Home from "@/views/Home.vue";
import SiteMap from "@/views/SiteMap.vue";
import TakeScooter from "@/views/TakeScooter.vue";
import ReturnScooter from "@/views/ReturnScooter.vue";
import Account from "@/views/Account.vue";

import { latLng } from "leaflet";

import { Plugins } from "@capacitor/core";
const { SplashScreen, Geolocation, Storage } = Plugins;

import { EventBus } from "@/modules/event-bus";
import auth from "@/modules/auth";
import data from "@/modules/data";

Storage.getItem = function (key) { return this.get({key}).then(v => (v === null) ? null : JSON.parse(v.value)); }
Storage.setItem = function (key, value) { return this.set({key, value: JSON.stringify(value)}); }
Storage.removeItem = function (key) { return this.remove({key}) }
Storage.key = function (n) { return this.keys().then(k => ((0 <= n) && (n < k.length)) ? k[n] : null) }

const vuexLocalStorage = new VuexPersist({
    key: "vuex",
    asyncStorage: (typeof cordova !== "undefined"),
    storage: (typeof cordova !== "undefined") ? Storage : window.localStorage,
    modules: ["auth"],
});

const axios = require("axios");
const http = require("http");
const httpAgent = new http.Agent({ keepAlive: true });

const ax = axios.create({ httpAgent, withCredentials: true });

const store = new Vuex.Store({
    state: {
        ax,
        is_started: false,
        sign_in_ready: false,
        reauth_in_progress: false,
        app_bar_info: "...",
        auth_plugin: null,
        current_location: latLng(0, 0),
        scooters_in_use: [],
        near_station: null,
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
        set_signin_state(state, value) {
            state.sign_in_ready = value;
        },
        set_reauth(state, value) {
            state.reauth_in_progress = value;
        },
        set_location(state, loc) {
            if (loc && (loc.lat !== undefined) && (loc.lng !== undefined)) {
                state.current_location = loc;
            }
        },
        take_scooter(state, id) {
        },
        return_scooter(state, id) {
        },
    },
    actions: {
    },
    modules: {
        auth,
        data,
    },
    plugins: [vuexLocalStorage.plugin],
});

ax.interceptors.response.use(function (response) {
    return response;
}, function (error) {
    // [2021-02-14T12:43:20.627] [DEBUG] utils - error = {"status":403,"error":{"code":"auth/id-token-expired","message":"Firebase ID token has expired. Get a fresh ID token from your client app and try again (auth/id-token-expired). See https://firebase.google.com/docs/auth/admin/verify-id-tokens for details on how to retrieve an ID token."}}
    //
    // axios intercepted response: {"message":"Request failed with status code 403","name":"Error","stack":"Error: Request failed with status code 403\n    at createError (webpack-internal:///./node_modules/axios/lib/core/createError.js:16:15)\n    at settle (webpack-internal:///./node_modules/axios/lib/core/settle.js:17:12)\n    at XMLHttpRequest.handleLoad (webpack-internal:///./node_modules/axios/lib/adapters/xhr.js:62:7)","config":{"url":"/v0/unit/trace/Simulated?before=1613306600&num=8","method":"get","headers":{"Accept":"application/json, text/plain, */*","Authorization":"Bearer eyJ..."},"transformRequest":[null],"transformResponse":[null],"timeout":0,"withCredentials":true,"xsrfCookieName":"XSRF-TOKEN","xsrfHeaderName":"X-XSRF-TOKEN","maxContentLength":-1,"maxBodyLength":-1,"httpAgent":{}}}
    //
    // axios intercepted, error.response: {"data":"","status":403,"statusText":"Firebase ID token has expired. Get a fresh ID token from your client app and try again (auth/id-token-expired). See https://firebase.google.com/docs/auth/admin/verify-id-tokens for details on how to retrieve an ID token.","headers":{"last-modified":"Mon, 01 Mar 2021 16:10:47 GMT"},"config":{"url":"v0/whoami","method":"get","headers":{"Accept":"application/json, text/plain, */*","Authorization":"Bearer eyJh...d6g"},"baseURL":"https://client.wodeewa.com","transformRequest":[null],"transformResponse":[null],"timeout":0,"withCredentials":true,"xsrfCookieName":"XSRF-TOKEN","xsrfHeaderName":"X-XSRF-TOKEN","maxContentLength":-1,"maxBodyLength":-1,"httpAgent":{}},"request":{}}
    //
    console.log("axios intercepted, error.response: " + JSON.stringify(error.response));
    //console.log("own props of error: " + JSON.stringify(Object.getOwnPropertyNames(error))); // own props of error: ["stack","message","config","request","response","isAxiosError","toJSON"]

    if ((error.response.status == 401) && !store.state.reauth_in_progress) {
        store.commit("set_reauth", true);
        console.log("TODO: get a new cookie and retry");
        return store.dispatch("sign_in_to_backend").then(() => {
            store.commit("set_reauth", false);
            return ax.request(error.config);
        });
    }

    if ((error.response.status == 403) && error.response.statusText.includes("(auth/id-token-expired)") && !store.state.reauth_in_progress) {
        store.commit("set_reauth", true);
        console.log("TODO: get a new id token and retry, provider_id='" + store.state.auth.provider_id + "'");
        //if (store.state.auth.provider_id == "google.com") {
        //}
        return store.dispatch("sign_in_with_google").then(() => {
            store.commit("set_reauth", false);
            if (error.config.url == "v0/whoami") {  // the sign-in itself -> already processed
                return Promise.resolve();
            }
            else {
                return ax.request(error.config);
            }
        });
    }

    return Promise.reject(error);
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
            path: "/sign_in",
            name: "Sign In",
            component: SignIn
        },
        {
            path: "/",
            name: "Home",
            component: Home
        },
        {
            path: "/take_scooter",
            name: "Take Scooter",
            component: TakeScooter
        },
        {
            path: "/return_scooter",
            name: "Return Scooter",
            component: ReturnScooter
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
        store.state.ax.defaults.baseURL = "https://client.wodeewa.com";
        // current location handler
        // https://capacitorjs.com/docs/apis/geolocation#geolocationposition
        const watch_location = function (loc, err) {
            if (err) {
                console.log("watch_location err=" + err);
            }
            else if ((loc.latitude !== undefined) && (loc.longitude !== undefined)) {
                console.log("loc.timestamp=" + loc.timestamp);
                console.log("loc.latitude=" + loc.latitude);
                console.log("loc.longitude=" + loc.longitude);
                store.commit("set_location", latLng(loc.latitude, loc.longitude));
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
    else {
        EventBus.$once("signed-in", () => {
            store.state.ax.get("/v0/unit/Simulated").then(response => {
                const u = response.data[0];
                console.log("Initial unit qwer " + JSON.stringify(u));
                const loc = latLng(u.lat, u.lon);
                console.log("Initial unit location " + loc);
                store.commit("set_location", loc);
            });
        });
        EventBus.$on("unit", u => {
            if ((u.unit == "Simulated") && u.lat && u.lon) {
                const loc = latLng(u.lat, u.lon);
                console.log("Updating unit location " + loc);
                store.commit("set_location", loc);
            }
        });
    }

    EventBus.$once("signed-in", () => {
        store.dispatch("data/fetch_stations");
        store.dispatch("data/fetch_units");
        EventBus.$on("unit", u => store.dispatch("data/update_unit", u));
    });

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
