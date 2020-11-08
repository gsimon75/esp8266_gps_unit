import auth_web from "./auth_web";
import auth_mobile from "./auth_mobile";

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
    actions: (typeof cordova !== "undefined") ? auth_mobile : auth_web,
};

// vim: set sw=4 ts=4 indk= et:
