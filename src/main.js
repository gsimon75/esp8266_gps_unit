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

import S4Date from "./plugins/S4Date";
Vue.use(S4Date);

import Vuex from "vuex";
Vue.use(Vuex);

import VuexPersist from "vuex-persist";

import VueRouter from "vue-router";
Vue.use(VueRouter);

import "./assets/Roboto.css";
import "./assets/fontawesome5.all.css";
import "leaflet/dist/leaflet.css";

import App from "./App.vue";

import NotFound from "./views/NotFound.vue";
import SignIn from "./views/SignIn.vue";
import Home from "./views/Home.vue";
import About from "./views/About.vue";
import Calendar from "./views/Calendar.vue";
import NutritionTracker from "./views/NutritionTracker.vue";
import DailyStatus from "./views/DailyStatus.vue";
import TrainerTracker from "./views/TrainerTracker.vue";
import Account from "./views/Account.vue";
import Settings from "./views/Settings.vue";

import { Plugins } from "@capacitor/core";
const { SplashScreen } = Plugins;

import auth from "./modules/auth";
//import nutrients from "./modules/nutrients";

const vuexLocalStorage = new VuexPersist({
    key: "vuex",
    storage: window.localStorage,
    modules: ["auth"],
});

const store = new Vuex.Store({
    state: {
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
        set_auth_plugin(state, x) {
            state.auth_plugin = x;
        },
        set_db(state, x) {
            state.db = x;
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
        //nutrients: nutrients,
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
            path: "/about",
            name: "About",
            component: About
        },
        {
            path: "/calendar",
            name: "Calendar",
            component: Calendar
        },
        {
            path: "/nutrition_tracker",
            name: "Nutrition Tracker",
            component: NutritionTracker
        },
        {
            path: "/daily_status",
            name: "Daily Status",
            component: DailyStatus
        },
        {
            path: "/trainer_tracker",
            name: "Trainer Tracker",
            component: TrainerTracker
        },
        {
            path: "/account",
            name: "Account",
            component: Account
        },
        {
            path: "/settings",
            name: "Settings",
            component: Settings
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
    alert("Could not sign in with persisted data: " + error.message);
    store.commit("logged_out");
}).then(() => {
    if (typeof cordova !== "undefined") {
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
