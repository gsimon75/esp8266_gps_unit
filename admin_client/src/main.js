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

import axios from "axios"
import VueAxios from "vue-axios"
Vue.use(VueAxios, axios)

import Vuetify from "vuetify/lib";
Vue.use(Vuetify);

import App from "./App.vue";

const vuetify = new Vuetify({
    global: {
        ripples: false,
    },
    icons: {
        iconfont: "mdiSvg",
    },
});

new Vue({
    render: h => h(App),
    vuetify,
}).$mount("#app");

// vim: set sw=4 ts=4 indk= et:
